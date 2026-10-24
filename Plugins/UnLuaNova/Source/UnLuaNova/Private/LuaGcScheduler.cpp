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

#include "LuaGcScheduler.h"
#include "LuaBackend.h"
#include "HAL/PlatformTime.h"

namespace UnLuaNova
{
    FLuaGcScheduler::FLuaGcScheduler(lua_State* InL)
        : L(InL)
    {
        BaselineBytes = GetHeapBytes();
        Stats.HeapBytes = BaselineBytes;
        Stats.PeakHeapBytes = BaselineBytes;
    }

    int64 FLuaGcScheduler::GetHeapBytes() const
    {
        return L ? UNLUANOVA_GC_HEAP_BYTES(L) : 0;
    }

    void FLuaGcScheduler::SetMode(ELuaGcMode InMode)
    {
        if (!L || Mode == InMode)
            return;

        Mode = InMode;

        if (Mode == ELuaGcMode::TimeSliced)
        {
            // From here the heap is our responsibility. Stopping the VM's own pacing is
            // what makes the per-frame cost predictable; forgetting to restart it when
            // leaving this mode is what would make it grow forever, hence the else.
            lua_gc(L, LUA_GCSTOP);
            DebtBytes = 0;
            BaselineBytes = GetHeapBytes();
        }
        else
        {
            lua_gc(L, LUA_GCRESTART);
            DebtBytes = 0;
            BaselineBytes = GetHeapBytes();
        }
    }

    void FLuaGcScheduler::SetStepMultiplier(int32 InPercent)
    {
        StepMultiplier = FMath::Max(0, InPercent);
    }

    void FLuaGcScheduler::SetMaxStepBytesPerFrame(int64 InBytes)
    {
        MaxStepBytesPerFrame = FMath::Max<int64>(0, InBytes);
    }

    void FLuaGcScheduler::SetHardLimitBytes(int64 InBytes)
    {
        HardLimitBytes = FMath::Max<int64>(0, InBytes);
    }

    void FLuaGcScheduler::Tick(float DeltaSeconds)
    {
        if (!L)
            return;

        Stats.Frames++;

        const int64 Heap = GetHeapBytes();
        Stats.HeapBytes = Heap;
        Stats.PeakHeapBytes = FMath::Max(Stats.PeakHeapBytes, Heap);

        if (Mode != ELuaGcMode::TimeSliced)
        {
            // Automatic: the VM did whatever it did. Keep the baseline current so that
            // switching to TimeSliced later does not book the entire session as debt.
            BaselineBytes = Heap;
            return;
        }

        // A mis-tuned budget is recoverable; an unbounded heap is not. Take the spike.
        if (HardLimitBytes > 0 && Heap >= HardLimitBytes)
        {
            Collect();
            return;
        }

        // Debt is "how much has piled up since the last completed cycle", not "how much
        // was allocated this frame". The distinction matters: Lua does not free anything
        // until a cycle runs, so a script that builds 4 MB and drops it leaves the heap
        // at 4 MB having allocated 0 bytes this frame. Measuring per-frame allocation
        // would book that as zero work owed and never collect the 4 MB at all.
        const int64 Grown = Heap > BaselineBytes ? Heap - BaselineBytes : 0;
        DebtBytes = Grown * int64(StepMultiplier) / 100;
        Stats.PendingDebtBytes = DebtBytes;

        if (DebtBytes <= 0)
            return;

        int64 Budget = DebtBytes;
        if (MaxStepBytesPerFrame > 0)
            Budget = FMath::Min(Budget, MaxStepBytesPerFrame);

        const double Start = FPlatformTime::Seconds();
        Step(Budget);
        const double ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000.0;

        Stats.SteppedFrames++;
        Stats.TotalStepMs += ElapsedMs;
        Stats.WorstFrameMs = FMath::Max(Stats.WorstFrameMs, ElapsedMs);
    }

    bool FLuaGcScheduler::Step(int64 Bytes)
    {
        if (!L || Bytes <= 0)
            return false;

        const int64 Before = GetHeapBytes();
        bCycleComplete = UNLUANOVA_GC_STEP_BYTES(L, Bytes) != 0;
        const int64 After = GetHeapBytes();

        // The VMs report "did a cycle finish", never "how much work did you just do", so
        // spending the request is the only honest accounting available while a cycle is
        // still in progress. Over-spending is safe: it just means the collector runs
        // ahead of the allocator for a while.
        Stats.BytesSteppedTotal += Bytes;
        Stats.HeapBytes = After;

        if (bCycleComplete)
        {
            // Everything reachable has been walked, so what is left is live data. Anything
            // allocated from here is new garbage, which is what debt should measure.
            BaselineBytes = After;
            DebtBytes = 0;
        }
        else
        {
            DebtBytes = FMath::Max<int64>(0, DebtBytes - Bytes);
        }

        Stats.PendingDebtBytes = DebtBytes;
        return After < Before;
    }

    void FLuaGcScheduler::Collect()
    {
        if (!L)
            return;

        lua_gc(L, LUA_GCCOLLECT);
        Stats.FullCollects++;
        DebtBytes = 0;
        Stats.PendingDebtBytes = 0;
        BaselineBytes = GetHeapBytes();
        Stats.HeapBytes = BaselineBytes;
        bCycleComplete = true;
    }

    void FLuaGcScheduler::ResetStats()
    {
        Stats = FStats();
        Stats.HeapBytes = GetHeapBytes();
        Stats.PeakHeapBytes = Stats.HeapBytes;
    }
}
