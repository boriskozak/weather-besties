#!/usr/bin/env python3
"""Turns wasm2js output into a plain script for browsers that refuse WebAssembly.
Usage: make_fallback.py in.mjs out.js

Making in.mjs with binaryen (npm install binaryen): wasi-sdk 33 uses newer WebAssembly
features that wasm2js can't read, so lower them first:
  wasm-opt weather-besties.wasm --enable-bulk-memory --enable-bulk-memory-opt \
      --enable-nontrapping-float-to-int --enable-sign-ext --enable-mutable-globals \
      --enable-multivalue --enable-reference-types --llvm-memory-copy-fill-lowering \
      --llvm-nontrapping-fptoint-lowering --signext-lowering -o low.wasm
  wasm2js -O2 low.wasm -o in.mjs
"""
import sys
src = open(sys.argv[1]).read()
src = src.replace("import * as wasi_snapshot_preview1 from 'wasi_snapshot_preview1';\n", "", 1)
cut = src.index("var retasmFunc = asmFunc(")
src = "(function(){\n" + src[:cut] + "globalThis.WeatherBestiesAsm = asmFunc;\n})();\n"
open(sys.argv[2], "w").write(src)
print("wrote", sys.argv[2], len(src), "bytes")
