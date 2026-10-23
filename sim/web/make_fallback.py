#!/usr/bin/env python3
"""Turns wasm2js output into a plain script for browsers that refuse WebAssembly.
Usage: make_fallback.py in.mjs out.js"""
import sys
src = open(sys.argv[1]).read()
src = src.replace("import * as wasi_snapshot_preview1 from 'wasi_snapshot_preview1';\n", "", 1)
cut = src.index("var retasmFunc = asmFunc(")
src = "(function(){\n" + src[:cut] + "globalThis.WeatherBestiesAsm = asmFunc;\n})();\n"
open(sys.argv[2], "w").write(src)
print("wrote", sys.argv[2], len(src), "bytes")
