import pathlib, os

run = pathlib.Path(r"d:/learning/UnLua/Saved/TestReports/20261022-164445/luau")
print("=== files in run dir ===")
for p in sorted(run.rglob("*")):
    if p.is_file():
        print(f"  {p.relative_to(run)}  {p.stat().st_size}")

log = run / "Editor.log"
lines = log.read_text(encoding="utf-8", errors="replace").splitlines()

print("\n=== results for the new specs ===")
for l in lines:
    if "Test Completed. Result=" in l and any(k in l for k in ("ErrorHandling", "Lifetime", "FLuaEnv")):
        i = l.find("Name=")
        # decode is lossy in console; print only the ascii markers
        res = l.split("Result=")[1].split("}")[0]
        tail = l[i:i + 90]
        print(f"  {res:10s} {tail}")

print("\n=== any Error: lines mentioning my specs ===")
for l in lines:
    if "Error" in l and any(k in l for k in ("ErrorHandling", "Lifetime", "FLuaEnv")):
        s = l.strip()
        print("  ", s[:180])
        if len([x for x in lines if x == l]) > 0:
            pass
