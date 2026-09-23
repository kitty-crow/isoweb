from pathlib import Path

p = Path('web/src/presentation/staticTracer.ts')
text = p.read_text()
stray = '''    const outputFloatCount = width * height * 16;\n    // StaticSample is four float32 values. Preserve the GPU-computed IEEE-754\n    // bit patterns exactly through a Uint32 view of WASM memory. Ordinary\n    // ArrayBuffer-backed WASM can receive readPixels directly; pthread shared\n    // memory keeps the browser-owned staging buffer for compatibility.\n    const target = new Uint32Array(\n      heap.buffer,\n      heap.byteOffset + outputPointer,\n      outputFloatCount\n    );\n    const readback = usesSharedMemory ? this.resultBuffer : target;\n\n'''
if not text.startswith(stray):
    raise SystemExit('unexpected staticTracer.ts prefix; refusing fuzzy edit')
p.write_text(text[len(stray):])
print('Removed accidentally hoisted WebGL readback block; in-method copy remains intact.')
