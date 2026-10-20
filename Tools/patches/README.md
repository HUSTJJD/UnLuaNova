# Analyzer Test Fixture Patches

`luals-relative-library-initialize.patch` is **no longer applied** by any script.
`Build-LuaLS.ps1` used to apply it to `test/tclient/tests/load-relative-library.lua`
inside the `Tools/lua-language-server` submodule, which left the submodule working
tree dirty after every build - the pointer was pinned to upstream `a58fc7b0`, but the
tree was not upstream's. Local edits to upstream source are out: an upstream fixture
problem goes upstream, or the pin moves.

The file is kept only as a record of the upstream issue it worked around: the
relative-library fixture initialized the same client twice, first before registering
its configuration handler and then without the workspace root, so definition lookup
could race the second asynchronous configuration load (reproduced on Release25 and
Release27 at the original definition assertion). If the pinned revision is ever
bumped and the upstream test suite starts failing on that fixture again, this is the
change that made it deterministic - upstream it rather than re-applying it here.
