// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

// Lua-declared networking: RPCs and replicated fields.
//
// What these cases can and cannot prove. They can prove the declarations are parsed, the
// UFunctions are built with the right flags and typed parameters, they land in the class's
// function map, and they are removed again on teardown. They cannot prove an RPC actually
// reaches the other peer - that needs a client/server PIE, and it depends on the engine
// assigning (or us assigning) matching RPC ids on both ends. That last part is the one
// thing here that still needs a networked run; see the "已知边界" note in the docs.

#include "UnLuaNovaBase.h"
#include "LuaBackend.h"
#include "LuaEnv.h"
#include "LuaFunction.h"
#include "UnLuaNovaManager.h"
#include "UnLuaNovaNet.h"
#include "UnLuaNovaNetPayload.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "Perfs/UnLuaNovaBenchmarkProxy.h"

#if WITH_DEV_AUTOMATION_TESTS

static const TCHAR* SpecNetModuleName = TEXT("Spec.NetModule");

namespace
{
    int SpecNetModuleLoader(lua_State* L)
    {
        lua_newtable(L);                                   // M

        lua_newtable(L);                                   // M.ServerRPC
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "Ping");                       // Ping = true -> unreliable
        lua_newtable(L);                                   // { Reliable = true, Params = {...} }
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "Reliable");
        lua_newtable(L);
        lua_pushstring(L, "Int");
        lua_rawseti(L, -2, 1);
        lua_pushstring(L, "Str");
        lua_rawseti(L, -2, 2);
        lua_setfield(L, -2, "Params");
        lua_setfield(L, -2, "Chat");
        lua_setfield(L, -2, "ServerRPC");

        lua_newtable(L);
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "ShowDamage");
        lua_setfield(L, -2, "ClientRPC");

        lua_newtable(L);
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "Explode");
        lua_setfield(L, -2, "MulticastRPC");

        lua_pushcfunction(L, [](lua_State* Inner) -> int
        {
            lua_pushboolean(Inner, 1);
            lua_setglobal(Inner, "SpecNetPropsCalled");
            lua_newtable(Inner);
            lua_newtable(Inner);
            lua_pushstring(Inner, "HP");
            lua_setfield(Inner, -2, "Name");
            lua_pushstring(Inner, "Float");
            lua_setfield(Inner, -2, "Type");
            lua_rawseti(Inner, -2, 1);
            lua_newtable(Inner);
            lua_pushstring(Inner, "Name");
            lua_setfield(Inner, -2, "Name");
            lua_pushstring(Inner, "String");
            lua_setfield(Inner, -2, "Type");
            lua_pushstring(Inner, "None");
            lua_setfield(Inner, -2, "Condition");
            lua_rawseti(Inner, -2, 2);
            return 1;
        });
        lua_setfield(L, -2, "GetLifetimeReplicatedProps");

        return 1;
    }

    UFunction* FindOwn(UClass* Class, const TCHAR* Name)
    {
        return Class->FindFunctionByName(FName(Name), EIncludeSuperFlag::ExcludeSuper);
    }
}

BEGIN_DEFINE_SPEC(FNetSpec, "UnLuaNova.API.Net", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
    UObject* Bound;
END_DEFINE_SPEC(FNetSpec)

void FNetSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
        Env->AddBuiltInLoader(SpecNetModuleName, SpecNetModuleLoader);

        Bound = NewObject<AUnLuaBenchmarkProxy>();
        Bound->AddToRoot();
        Env->GetManager()->Bind(Bound, SpecNetModuleName);
    });

    AfterEach([this]
    {
        // Take the RPCs back off the class. Without this the UFunctions stay on
        // AUnLuaBenchmarkProxy for the rest of the process and the next case sees them.
        ULuaFunction::RestoreOverrides(Bound->GetClass());
        UnLuaNova::FUnLuaNovaNet::Cleanup(Env.Get());

        if (Bound)
        {
            Bound->RemoveFromRoot();
            Bound = nullptr;
        }
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("类型与条件解析"), [this]()
    {
        It(TEXT("参数类型名解析，且大小写不敏感"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::ELuaNetParamType Type = UnLuaNova::ELuaNetParamType::Int;
            TEST_TRUE(UnLuaNova::ParseLuaNetParamType(TEXT("vector"), Type));
            TEST_TRUE(Type == UnLuaNova::ELuaNetParamType::Vector);
            TEST_TRUE(UnLuaNova::ParseLuaNetParamType(TEXT("FString"), Type));
            TEST_TRUE(Type == UnLuaNova::ELuaNetParamType::String);
            TEST_FALSE(UnLuaNova::ParseLuaNetParamType(TEXT("notatype"), Type));
        });

        It(TEXT("复制条件用引擎自己的常量"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            uint8 Condition = COND_None;
            TEST_TRUE(UnLuaNova::ParseLifetimeCondition(TEXT("OwnerOnly"), Condition));
            TEST_EQUAL((int32)Condition, (int32)COND_OwnerOnly);
            TEST_FALSE(UnLuaNova::ParseLifetimeCondition(TEXT("Whatever"), Condition));
        });
    });

    Describe(TEXT("RPC 声明"), [this]()
    {
        It(TEXT("invalid RPC parameters reject the whole function"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            for (const char* Script : {
                "return {ServerRPC={Invalid={Params={'Int','Missing','String'}}}}",
                "return {ServerRPC={Invalid={Params={[2]='Int'}}}}",
                "return {ServerRPC={Invalid={Params={{Name='Same',Type='Int'},{Name='same',Type='Float'}}}}}",
                "return {ServerRPC={Invalid={Params='Int'}}}",
                "return {ServerRPC={Invalid={Params={{Name=7,Type='Int'}}}}}",
                "return {ServerRPC={Invalid={Param={'Int'}}}}",
                "return {ServerRPC={Invalid={Reliable='true'}}}",
                "return {ServerRPC={Invalid=false}}" })
            {
                TEST_TRUE(UnLuaNova::RunChunk(L, Script));
                const int32 Top = lua_gettop(L);
                AddExpectedError(TEXT("Invalid RPC declaration"), EAutomationExpectedErrorFlags::Contains, 1);
                UnLuaNova::FUnLuaNovaNet::DeclareRPCs(L, -1, Bound->GetClass());
                TEST_EQUAL(lua_gettop(L), Top);
                TEST_TRUE(FindOwn(Bound->GetClass(), TEXT("Invalid")) == nullptr);
                lua_pop(L, 1);
            }
        });

        It(TEXT("RPC registration is independent of table insertion order"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UClass* Class = Bound->GetClass();
            TArray<int32> Expected;
            for (const char* Script : {
                "return {ServerRPC={Zulu=true, Alpha=true, Middle=true}}",
                "local t={} t.Middle=true t.Alpha=true t.Zulu=true return {ServerRPC=t}" })
            {
                ULuaFunction::RestoreOverrides(Class);
                TEST_TRUE(UnLuaNova::RunChunk(L, Script));
                UnLuaNova::FUnLuaNovaNet::DeclareRPCs(L, -1, Class);
                lua_pop(L, 1);
                TArray<int32> Actual;
                for (const TCHAR* Name : { TEXT("Alpha"), TEXT("Middle"), TEXT("Zulu") })
                {
                    const UFunction* Function = FindOwn(Class, Name);
                    TEST_TRUE(Function != nullptr);
                    if (Function)
                        Actual.Add(Function->RPCId);
                }
                if (Expected.IsEmpty())
                    Expected = Actual;
                else
                    TEST_TRUE(Expected == Actual);
                TEST_TRUE(Actual.Num() == 3 && Actual[0] < Actual[1] && Actual[1] < Actual[2]);
            }
        });

        It(TEXT("三种 RPC 都注册上，并带上正确的 Net 标记"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Class = Bound->GetClass();

            UFunction* Ping = FindOwn(Class, TEXT("Ping"));
            TEST_TRUE(Ping != nullptr);
            if (Ping)
            {
                TEST_TRUE(Ping->HasAnyFunctionFlags(FUNC_Net));
                TEST_TRUE(Ping->HasAnyFunctionFlags(FUNC_NetServer));
                // `Ping = true` declares it; reliability stays opt-in, like UFUNCTION(Server).
                TEST_FALSE(Ping->HasAnyFunctionFlags(FUNC_NetReliable));
            }

            UFunction* ShowDamage = FindOwn(Class, TEXT("ShowDamage"));
            TEST_TRUE(ShowDamage != nullptr);
            if (ShowDamage)
                TEST_TRUE(ShowDamage->HasAnyFunctionFlags(FUNC_NetClient));

            UFunction* Explode = FindOwn(Class, TEXT("Explode"));
            TEST_TRUE(Explode != nullptr);
            if (Explode)
                TEST_TRUE(Explode->HasAnyFunctionFlags(FUNC_NetMulticast));
        });

        It(TEXT("Reliable 与参数表被采纳"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UFunction* Chat = FindOwn(Bound->GetClass(), TEXT("Chat"));
            TEST_TRUE(Chat != nullptr);
            if (!Chat)
                return;

            TEST_TRUE(Chat->HasAnyFunctionFlags(FUNC_NetReliable));
            TEST_EQUAL(Chat->NumParms, 2);

            // Params = { "Int", "Str" } -> FIntProperty then FStrProperty, in order.
            // ChildProperties (and Next) are FField*, which is the base of FProperty -
            // CastField, not a static_cast, so a non-property field cannot slip through.
            FProperty* First = CastField<FProperty>(Chat->ChildProperties);
            TEST_TRUE(First != nullptr);
            TEST_TRUE(First && First->IsA<FIntProperty>());
            FProperty* Second = First ? CastField<FProperty>(First->Next) : nullptr;
            TEST_TRUE(Second != nullptr);
            TEST_TRUE(Second && Second->IsA<FStrProperty>());
        });

        It(TEXT("带 Net 标记的函数不会被当成普通覆写"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The reason this feature exists at all: IsOverridable() rejects FUNC_Net, so
            // none of these could have been created by the ordinary override path.
            UFunction* Ping = FindOwn(Bound->GetClass(), TEXT("Ping"));
            TEST_TRUE(Ping != nullptr);
            TEST_FALSE(ULuaFunction::IsOverridable(Ping));
        });

        It(TEXT("RPCId 被分配，且重复声明时得到同一个值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // This is the one thing about RPCs that a single-process test can still settle,
            // and it is the one that decides whether the whole feature works across peers.
            //
            // An RPC travels as an id, not as a name. Two processes that build the same
            // UFunction from the same Lua module have to land on the same id or the receiver
            // resolves it to the wrong function. We cannot run two processes here, but
            // "declare, tear down, declare again" reproduces the interesting half of it:
            // the second declaration starts from exactly the class state the first one did,
            // so identical ids is the same property a second process would need.
            auto* Class = Bound->GetClass();

            UFunction* First = FindOwn(Class, TEXT("Ping"));
            TEST_TRUE(First != nullptr);
            if (!First)
                return;

            // 0 would mean nobody assigned one, and an RPC sent with id 0 is not an RPC.
            TEST_TRUE(First->RPCId != 0);
            const int32 FirstId = First->RPCId;

            TMap<FName, int32> Before;
            for (const TCHAR* Name : { TEXT("Ping"), TEXT("Chat"), TEXT("ShowDamage"), TEXT("Explode") })
                if (UFunction* F = FindOwn(Class, Name)) Before.Add(FName(Name), F->RPCId);

            ULuaFunction::RestoreOverrides(Class);

            // Re-declare from a fresh env, exactly as another process would.
            TSharedPtr<UnLuaNova::FLuaEnv> SecondEnv = MakeShared<UnLuaNova::FLuaEnv>();
            SecondEnv->AddBuiltInLoader(SpecNetModuleName, SpecNetModuleLoader);
            UObject* Second = NewObject<AUnLuaBenchmarkProxy>();
            Second->AddToRoot();
            SecondEnv->GetManager()->Bind(Second, SpecNetModuleName);

            for (const auto& Pair : Before)
            {
                UFunction* F = FindOwn(Class, *Pair.Key.ToString());
                TEST_TRUE(F != nullptr);
                if (F)
                    TEST_EQUAL(F->RPCId, Pair.Value);
            }

            Second->RemoveFromRoot();
            SecondEnv.Reset();
        });

        It(TEXT("RestoreOverrides 会把声明的 RPC 撤掉"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Class = Bound->GetClass();
            TEST_TRUE(FindOwn(Class, TEXT("Ping")) != nullptr);

            ULuaFunction::RestoreOverrides(Class);

            TEST_TRUE(FindOwn(Class, TEXT("Ping")) == nullptr);
        });
    });

    Describe(TEXT("复制属性声明"), [this]()
    {
        It(TEXT("GetLifetimeReplicatedProps 的返回值被读出来"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const TArray<UnLuaNova::FLuaRepDecl>& Decls =
                UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Bound->GetClass());

            TEST_EQUAL(Decls.Num(), 2);
            if (Decls.Num() != 2)
                return;

            TEST_TRUE(Decls[0].Name == FName(TEXT("HP")));
            TEST_TRUE(Decls[0].Type == UnLuaNova::ELuaNetParamType::Float);
            TEST_EQUAL((int32)Decls[0].Condition, (int32)COND_None);

            TEST_TRUE(Decls[1].Name == FName(TEXT("Name")));
            TEST_TRUE(Decls[1].Type == UnLuaNova::ELuaNetParamType::String);
            TEST_EQUAL((int32)Decls[1].Condition, (int32)COND_None);
        });

        It(TEXT("invalid schemas are rejected atomically"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Class = Bound->GetClass();
            const uint32 Before = UnLuaNova::ComputeSchemaVersion(UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Class));
            // OwnerOnly is deliberately not in this list any more: it is a supported condition
            // now and has storage of its own. What stays rejected is a condition that has no
            // storage - the engine refuses Custom/Dynamic on subobjects, and replay visibility
            // is not something this layer decides.
            for (const char* Script : {
                "return {ReplicatedProps={{Name='Private',Type='String',Condition='Custom'}}}",
                "return {ReplicatedProps={{Name='Private',Type='String',Condition='ReplayOnly'}}}",
                "return {ReplicatedProps={{Name='Private',Type='String',Condition='SkipReplay'}}}",
                "return {ReplicatedProps={{Name='HP',Type='Float',Condition='Typo'}}}",
                "return {ReplicatedProps={{Name='HP',Type='Float',Condition=7}}}",
                "return {ReplicatedProps={{Name='HP',Type='Float',Conditions='OwnerOnly'}}}",
                "return {ReplicatedProps={{Name='HP',Type='Float'},{Name='hp',Type='Int'}}}",
                "return {ReplicatedProps={{Name='HP',Type='Float'},false}}",
                "return {ReplicatedProps={[2]={Name='HP',Type='Float'}}}",
                "return {ReplicatedProps={{Name='HP',Type='Unknown'}}}",
                "return {GetLifetimeReplicatedProps=function() error('bad schema') end}",
                "return {ReplicatedProps={}}" })
            {
                TEST_TRUE(UnLuaNova::RunChunk(L, Script));
                const int32 Top = lua_gettop(L);
                AddExpectedError(TEXT("Rejected replication declaration"), EAutomationExpectedErrorFlags::Contains, 1);
                TEST_FALSE(UnLuaNova::FUnLuaNovaNet::DeclareReplicatedProps(L, -1, Class));
                TEST_EQUAL(lua_gettop(L), Top);
                TEST_EQUAL(UnLuaNova::ComputeSchemaVersion(UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Class)), Before);
                lua_pop(L, 1);
            }
        });

        It(TEXT("empty first schema is valid and repeated declarations are idempotent"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::FUnLuaNovaNet::Cleanup(Env.Get());
            TEST_TRUE(UnLuaNova::RunChunk(L, "return {ReplicatedProps={}}"));
            TEST_TRUE(UnLuaNova::FUnLuaNovaNet::DeclareReplicatedProps(L, -1, Bound->GetClass()));
            TEST_TRUE(UnLuaNova::FUnLuaNovaNet::DeclareReplicatedProps(L, -1, Bound->GetClass()));
            TEST_EQUAL(UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Bound->GetClass()).Num(), 0);
            lua_pop(L, 1);
        });

        It(TEXT("没有声明时返回空表而不是崩溃"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const TArray<UnLuaNova::FLuaRepDecl>& Decls =
                UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(UObject::StaticClass());
            TEST_EQUAL(Decls.Num(), 0);
        });
    });

    Describe(TEXT("Integer payloads"), [this]()
    {
        It(TEXT("int64 fields round-trip across 32-bit, double and signed limits"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Component = NewObject<UUnLuaNovaReplicationComponent>();
            Component->Int64s.SetNum(1);
            UnLuaNova::FLuaNetSlot Slot;
            Slot.Name = TEXT("Value");
            Slot.Bucket = UnLuaNova::ELuaNetBucket::Int64;
            Slot.Type = UnLuaNova::ELuaNetParamType::Int64;
            lua_newtable(L);
            const int32 Table = lua_absindex(L, -1);
            for (const int64 Value : { MIN_int64, -(int64(1) << 53) - 1, -(int64(1) << 31) - 1,
                int64(0), (int64(1) << 31), (int64(1) << 53), (int64(1) << 53) + 1, MAX_int64 })
            {
                Component->Int64s[0] = Value == 0 ? 1 : 0;
                UNLUANOVA_PUSHINT64(L, Value);
                lua_setfield(L, Table, "Value");
                TEST_TRUE(Component->SyncSlot(L, Table, Slot));
                TEST_EQUAL(Component->Int64s[0], Value);
                lua_pushnil(L);
                lua_setfield(L, Table, "Value");
                TEST_TRUE(Component->ApplySlot(L, Table, Slot));
                lua_getfield(L, Table, "Value");
                int64 RoundTrip = 0;
                TEST_TRUE(UnLuaNovaTryToInt64(L, -1, RoundTrip));
                TEST_EQUAL(RoundTrip, Value);
                lua_pop(L, 1);
                TEST_FALSE(Component->SyncSlot(L, Table, Slot));
            }
            lua_pop(L, 1);
        });

        It(TEXT("invalid integers preserve the previous wire value"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Component = NewObject<UUnLuaNovaReplicationComponent>();
            Component->Int64s.Add(42);
            UnLuaNova::FLuaNetSlot Slot;
            Slot.Name = TEXT("Value");
            Slot.Bucket = UnLuaNova::ELuaNetBucket::Int64;
            Slot.Type = UnLuaNova::ELuaNetParamType::Int64;
            for (const char* Script : { "return {Value=1.5}", "return {Value=math.huge}",
                "return {Value=-math.huge}", "return {Value=0/0}", "return {Value=2^63}",
                "return {Value=-(2^64)}", "return {Value='42'}", "return {Value=false}" })
            {
                TEST_TRUE(UnLuaNova::RunChunk(L, Script));
                TEST_FALSE(Component->SyncSlot(L, lua_absindex(L, -1), Slot));
                TEST_EQUAL(Component->Int64s[0], int64(42));
                lua_pop(L, 1);
            }
        });
    });

    Describe(TEXT("分桶布局"), [this]()
    {
        It(TEXT("Schema fingerprint includes replication conditions"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TArray<UnLuaNova::FLuaRepDecl> Decls;
            Decls.Add({ FName(TEXT("Secret")), UnLuaNova::ELuaNetParamType::String, COND_None });
            const uint32 PublicSchema = UnLuaNova::ComputeSchemaVersion(Decls);
            Decls[0].Condition = COND_OwnerOnly;
            TEST_TRUE(PublicSchema != UnLuaNova::ComputeSchemaVersion(Decls));
        });

        // The layout is what both peers derive locally to turn an array element back into a
        // Lua field, so it is the one part of the new payload a single process can settle
        // completely: same declarations in, same (bucket, index) addresses out.

        It(TEXT("字段按类型落进桶，同类型按声明顺序拿下标"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TArray<UnLuaNova::FLuaRepDecl> Decls;
            Decls.Add({ FName(TEXT("HP")),   UnLuaNova::ELuaNetParamType::Float,  COND_None });
            Decls.Add({ FName(TEXT("Gold")), UnLuaNova::ELuaNetParamType::Int,    COND_None });
            Decls.Add({ FName(TEXT("Mana")), UnLuaNova::ELuaNetParamType::Float,  COND_None });

            TArray<UnLuaNova::FLuaNetSlot> Slots;
            UnLuaNova::BuildSlots(Decls, Slots);

            TEST_EQUAL(Slots.Num(), 3);
            if (Slots.Num() != 3)
                return;

            TEST_TRUE(Slots[0].Bucket == UnLuaNova::ELuaNetBucket::Float);
            TEST_EQUAL(Slots[0].Index, 0);
            TEST_TRUE(Slots[1].Bucket == UnLuaNova::ELuaNetBucket::Int);
            TEST_EQUAL(Slots[1].Index, 0);
            // Second Float, so index 1 inside the Float bucket - not 2.
            TEST_TRUE(Slots[2].Bucket == UnLuaNova::ELuaNetBucket::Float);
            TEST_EQUAL(Slots[2].Index, 1);

            int32 Counts[int32(UnLuaNova::ELuaNetBucket::Max)];
            UnLuaNova::CountSlots(Slots, Counts);
            TEST_EQUAL(Counts[int32(UnLuaNova::ELuaNetBucket::Float)], 2);
            TEST_EQUAL(Counts[int32(UnLuaNova::ELuaNetBucket::Int)], 1);
            TEST_EQUAL(Counts[int32(UnLuaNova::ELuaNetBucket::String)], 0);
        });

        It(TEXT("SchemaVersion 一致当且仅当布局一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto Make = [](const TCHAR* First, const TCHAR* Second)
            {
                TArray<UnLuaNova::FLuaRepDecl> Decls;
                Decls.Add({ FName(First),  UnLuaNova::ELuaNetParamType::Float, COND_None });
                Decls.Add({ FName(Second), UnLuaNova::ELuaNetParamType::Int,   COND_None });
                return UnLuaNova::ComputeSchemaVersion(Decls);
            };

            // Same declarations, same fingerprint - this is what two peers running the same
            // module have to agree on before any value is interpreted.
            TEST_EQUAL(Make(TEXT("HP"), TEXT("Gold")), Make(TEXT("HP"), TEXT("Gold")));

            // A reordering is a different layout: the index inside a bucket is the position
            // in the declaration list, so swapping two fields would swap their values.
            TEST_TRUE(Make(TEXT("HP"), TEXT("Gold")) != Make(TEXT("Gold"), TEXT("HP")));
        });
    });

    Describe(TEXT("条件复制"), [this]()
    {
        It(TEXT("支持的条件被接受，落进各自的组并各自从下标 0 开始"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Class = Bound->GetClass();
            UnLuaNova::FUnLuaNovaNet::Cleanup(Env.Get());

            TEST_TRUE(UnLuaNova::RunChunk(L,
                "return {ReplicatedProps={"
                "{Name='Secret',Type='String',Condition='OwnerOnly'},"
                "{Name='Public',Type='String',Condition='None'}}}"));
            TEST_TRUE(UnLuaNova::FUnLuaNovaNet::DeclareReplicatedProps(L, -1, Class));

            const TArray<UnLuaNova::FLuaRepDecl>& Decls = UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Class);
            TEST_EQUAL(Decls.Num(), 2);
            if (Decls.Num() != 2)
                return;
            TEST_EQUAL((int32)Decls[0].Condition, (int32)COND_OwnerOnly);
            TEST_EQUAL((int32)Decls[1].Condition, (int32)COND_None);

            TArray<UnLuaNova::FLuaNetSlot> Slots;
            UnLuaNova::BuildSlots(Decls, Slots);
            TEST_EQUAL(Slots.Num(), 2);
            if (Slots.Num() != 2)
                return;

            // Two Strings, one per group, so both start at index 0 of their own group's array.
            // Sharing one array would put the second at index 1 and - far worse - send the
            // owner-only value on a property that reaches every connection.
            TEST_TRUE(Slots[0].Group == UnLuaNova::ELuaNetConditionGroup::OwnerOnly);
            TEST_EQUAL(Slots[0].Index, 0);
            TEST_TRUE(Slots[1].Group == UnLuaNova::ELuaNetConditionGroup::None);
            TEST_EQUAL(Slots[1].Index, 0);

            int32 Counts[int32(UnLuaNova::ELuaNetConditionGroup::Max)][int32(UnLuaNova::ELuaNetBucket::Max)];
            UnLuaNova::CountSlotsByGroup(Slots, Counts);
            TEST_EQUAL(Counts[int32(UnLuaNova::ELuaNetConditionGroup::OwnerOnly)][int32(UnLuaNova::ELuaNetBucket::String)], 1);
            TEST_EQUAL(Counts[int32(UnLuaNova::ELuaNetConditionGroup::None)][int32(UnLuaNova::ELuaNetBucket::String)], 1);
            lua_pop(L, 1);
        });

        It(TEXT("条件字段只写它自己那组的存储"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Component = NewObject<UUnLuaNovaReplicationComponent>();
            Component->GroupOwnerOnly.Strings.SetNum(1);

            UnLuaNova::FLuaNetSlot Slot;
            Slot.Name = TEXT("Secret");
            Slot.Bucket = UnLuaNova::ELuaNetBucket::String;
            Slot.Type = UnLuaNova::ELuaNetParamType::String;
            Slot.Condition = COND_OwnerOnly;
            Slot.Group = UnLuaNova::ELuaNetConditionGroup::OwnerOnly;

            TEST_TRUE(UnLuaNova::RunChunk(L, "return {Secret='owner-only'}"));
            const int32 Table = lua_absindex(L, -1);
            TEST_TRUE(Component->SyncSlot(L, Table, Slot));
            TEST_EQUAL(Component->GroupOwnerOnly.Strings[0], FString(TEXT("owner-only")));

            // Nothing about an owner-only field may reach the unconditional arrays, which are
            // registered COND_None and therefore sent to everyone.
            TEST_EQUAL(Component->Strings.Num(), 0);

            Component->GroupOwnerOnly.Strings[0] = TEXT("from the wire");
            lua_pushnil(L);
            lua_setfield(L, Table, "Secret");
            TEST_TRUE(Component->ApplySlot(L, Table, Slot));
            lua_getfield(L, Table, "Secret");
            TEST_EQUAL(FString(UTF8_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("from the wire")));
            lua_pop(L, 2);
        });

        It(TEXT("没有存储的条件在声明时就被拒绝，而不是降级成 None"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::ELuaNetConditionGroup Group = UnLuaNova::ELuaNetConditionGroup::None;

            TEST_TRUE(UnLuaNova::ConditionToGroup(COND_OwnerOnly, Group));
            TEST_TRUE(Group == UnLuaNova::ELuaNetConditionGroup::OwnerOnly);
            TEST_EQUAL((int32)UnLuaNova::GroupToCondition(UnLuaNova::ELuaNetConditionGroup::OwnerOnly), (int32)COND_OwnerOnly);

            // Custom is refused by the engine on subobjects and the Replay* family is not
            // decided here, so neither may quietly become COND_None.
            TEST_FALSE(UnLuaNova::ConditionToGroup(COND_Custom, Group));
            TEST_FALSE(UnLuaNova::ConditionToGroup(COND_ReplayOnly, Group));
            TEST_FALSE(UnLuaNova::ConditionToGroup(COND_SkipReplay, Group));
        });
    });

    Describe(TEXT("Reliability"), [this]()
    {
        It(TEXT("Reliable 在三种 RPC 上都被采纳，省略时默认不可靠"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Class = Bound->GetClass();

            TEST_TRUE(UnLuaNova::RunChunk(L,
                "return {ServerRPC={Given={Reliable=true}, Plain=true},"
                "ClientRPC={Notify={Reliable=true}},"
                "MulticastRPC={Boom={Reliable=true}}}"));
            UnLuaNova::FUnLuaNovaNet::DeclareRPCs(L, -1, Class);

            for (const TCHAR* Name : { TEXT("Given"), TEXT("Notify"), TEXT("Boom") })
            {
                UFunction* Function = FindOwn(Class, Name);
                TEST_TRUE(Function != nullptr);
                if (!Function)
                    continue;
                TEST_TRUE(Function->HasAnyFunctionFlags(FUNC_NetReliable));
            }

            // `Plain = true` is the shorthand and must stay unreliable, matching UFUNCTION(Server).
            UFunction* Plain = FindOwn(Class, TEXT("Plain"));
            TEST_TRUE(Plain != nullptr);
            if (Plain)
                TEST_FALSE(Plain->HasAnyFunctionFlags(FUNC_NetReliable));

            lua_pop(L, 1);
        });
    });

    Describe(TEXT("存储往返与边界"), [this]()
    {
        It(TEXT("object 字段在 Lua 与线值之间往返，缺失表示没有对象"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Component = NewObject<UUnLuaNovaReplicationComponent>();
            Component->Objects.SetNum(1);

            UObject* Target = NewObject<UObject>();
            Target->AddToRoot();

            UnLuaNova::FLuaNetSlot Slot;
            Slot.Name = TEXT("Target");
            Slot.Bucket = UnLuaNova::ELuaNetBucket::Object;
            Slot.Type = UnLuaNova::ELuaNetParamType::Object;

            lua_newtable(L);
            const int32 Table = lua_absindex(L, -1);

            UnLuaNova::PushUObject(L, Target);
            lua_setfield(L, Table, "Target");
            TEST_TRUE(Component->SyncSlot(L, Table, Slot));
            TEST_TRUE(Component->Objects[0].Get() == Target);

            // A missing Lua field is a legitimate "no object", not a read failure - but it
            // must not be reported as a change when the wire value is already null.
            lua_pushnil(L);
            lua_setfield(L, Table, "Target");
            Component->Objects[0] = nullptr;
            TEST_FALSE(Component->SyncSlot(L, Table, Slot));
            TEST_TRUE(Component->Objects[0].Get() == nullptr);

            Component->Objects[0] = Target;
            TEST_TRUE(Component->ApplySlot(L, Table, Slot));
            lua_getfield(L, Table, "Target");
            TEST_TRUE(UnLuaNova::GetUObject(L, -1) == Target);
            lua_pop(L, 2);

            Target->RemoveFromRoot();
        });

        It(TEXT("标量桶在 Lua 与线值之间双向往返"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Component = NewObject<UUnLuaNovaReplicationComponent>();
            Component->Bools.SetNum(1);
            Component->Ints.SetNum(1);
            Component->Int64s.SetNum(1);
            Component->Floats.SetNum(1);
            Component->Doubles.SetNum(1);
            Component->Strings.SetNum(1);
            Component->Names.SetNum(1);

            struct FCase { const TCHAR* Name; UnLuaNova::ELuaNetBucket Bucket; UnLuaNova::ELuaNetParamType Type; };
            const FCase Cases[] =
            {
                { TEXT("B"), UnLuaNova::ELuaNetBucket::Bool,   UnLuaNova::ELuaNetParamType::Bool },
                { TEXT("I"), UnLuaNova::ELuaNetBucket::Int,    UnLuaNova::ELuaNetParamType::Int },
                { TEXT("L"), UnLuaNova::ELuaNetBucket::Int64,  UnLuaNova::ELuaNetParamType::Int64 },
                { TEXT("F"), UnLuaNova::ELuaNetBucket::Float,  UnLuaNova::ELuaNetParamType::Float },
                { TEXT("D"), UnLuaNova::ELuaNetBucket::Double, UnLuaNova::ELuaNetParamType::Double },
                { TEXT("S"), UnLuaNova::ELuaNetBucket::String, UnLuaNova::ELuaNetParamType::String },
                { TEXT("N"), UnLuaNova::ELuaNetBucket::Name,   UnLuaNova::ELuaNetParamType::Name },
            };

            TEST_TRUE(UnLuaNova::RunChunk(L, "return {B=true,I=7,L=123456789,F=1.5,D=2.25,S='hi',N='Tag'}"));
            const int32 Table = lua_absindex(L, -1);

            auto MakeSlot = [](const FCase& Case)
            {
                UnLuaNova::FLuaNetSlot Slot;
                Slot.Name = Case.Name;
                Slot.Bucket = Case.Bucket;
                Slot.Type = Case.Type;
                return Slot;
            };

            for (const FCase& Case : Cases)
                TEST_TRUE(Component->SyncSlot(L, Table, MakeSlot(Case)));

            TEST_EQUAL(Component->Bools[0], uint8(1));
            TEST_EQUAL(Component->Ints[0], int32(7));
            TEST_EQUAL(Component->Int64s[0], int64(123456789));
            TEST_EQUAL(Component->Floats[0], 1.5f);
            TEST_EQUAL(Component->Doubles[0], 2.25);
            TEST_EQUAL(Component->Strings[0], FString(TEXT("hi")));
            TEST_EQUAL(Component->Names[0].ToString(), FString(TEXT("Tag")));

            // Erase the Lua side; ApplySlot has to put the wire values back.
            for (const FCase& Case : Cases)
            {
                const FTCHARToUTF8 Key(Case.Name);
                lua_pushnil(L);
                lua_setfield(L, Table, Key.Get());
            }
            for (const FCase& Case : Cases)
                TEST_TRUE(Component->ApplySlot(L, Table, MakeSlot(Case)));

            lua_getfield(L, Table, "I");
            TEST_EQUAL(lua_tointeger(L, -1), lua_Integer(7));
            lua_pop(L, 1);
            lua_getfield(L, Table, "S");
            TEST_EQUAL(FString(UTF8_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("hi")));
            lua_pop(L, 2);
        });

        It(TEXT("schema 不匹配时不写 Lua 字段，且只报一次"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Component = NewObject<UUnLuaNovaReplicationComponent>();
            Component->bDeclared = true;
            Component->LocalSchemaVersion = 0xA;
            Component->SchemaVersion = 0xB;
            Component->Ints.SetNum(1);
            Component->Ints[0] = 123;

            UnLuaNova::FLuaNetSlot Slot;
            Slot.Name = TEXT("HP");
            Slot.Bucket = UnLuaNova::ELuaNetBucket::Int;
            Slot.Type = UnLuaNova::ELuaNetParamType::Int;
            Component->Slots.Add(Slot);

            TEST_TRUE(UnLuaNova::RunChunk(L, "return {HP=1}"));
            const int32 Table = lua_absindex(L, -1);
            AddExpectedError(TEXT("received replicated fields laid out for a different declaration"), EAutomationExpectedErrorFlags::Contains, 1);

            Component->PushBucket(UnLuaNova::ELuaNetBucket::Int);

            lua_getfield(L, Table, "HP");
            TEST_EQUAL(lua_tointeger(L, -1), lua_Integer(1));
            lua_pop(L, 2);
            TEST_TRUE(Component->bSchemaMismatchReported);
        });

        It(TEXT("子类没有自己的声明时沿用父类的 schema"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::FUnLuaNovaNet::Cleanup(nullptr);
            TEST_TRUE(UnLuaNova::RunChunk(L, "return {ReplicatedProps={{Name='Inherited',Type='Int'}}}"));
            TEST_TRUE(UnLuaNova::FUnLuaNovaNet::DeclareReplicatedProps(L, -1, UObject::StaticClass()));

            const TArray<UnLuaNova::FLuaRepDecl>& Decls =
                UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Bound->GetClass());
            TEST_EQUAL(Decls.Num(), 1);
            if (Decls.Num() == 1)
                TEST_TRUE(Decls[0].Name == FName(TEXT("Inherited")));

            lua_pop(L, 1);
            UnLuaNova::FUnLuaNovaNet::Cleanup(nullptr);
        });
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS
