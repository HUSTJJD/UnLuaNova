#!/usr/bin/env python3
"""Run the profiler renderer from the repository's pinned Luau checkout."""

import runpy
import sys
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[2]
    upstream = root / "Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau/tools"
    script = upstream / "perfgraph.py"
    if not script.is_file() or not (upstream / "svg.py").is_file():
        raise SystemExit("Initialize the pinned Luau submodule; perfgraph.py and svg.py are required.")
    sys.path.insert(0, str(upstream))
    runpy.run_path(str(script), run_name="__main__")


if __name__ == "__main__":
    main()
