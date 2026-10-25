# Analyzer Test Fixture Patches

`Build-LuaLS.ps1` applies `luals-relative-library-initialize.patch` to the pinned
LuaLS revision `a58fc7b01e1746c46275c71b0e61bfed40a6296d`. Application is idempotent
and fails if neither the original nor patched context matches.

The upstream relative-library fixture initialized the same client twice, first
before registering its configuration handler and then without the workspace root.
This allowed definition lookup to race the second asynchronous configuration load.
Release25 and Release27 preserve failures at the original definition assertion.

The patch creates the library and registers configuration before a single
initialization with the intended root. It changes no analyzer runtime code,
assertions, expected paths, test inventory or timeout. Full upstream tests still
run, with this explicit fixture correction. The unrelated two bee stack-overflow
tests remain disabled upstream.
