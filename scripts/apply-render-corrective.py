from pathlib import Path

static_path = Path("web/src/presentation/staticTracer.ts")
static_text = static_path.read_text()
static_old = """    gl.activeTexture(gl.TEXTURE1);
    gl.bindTexture(gl.TEXTURE_2D, this.rayOriginsTexture);
    gl.uniform1i(this.rayOriginsUniform, 1);
    gl.uniform1i(this.heightUniform, height);
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);

    const started = performance.now();
"""
static_new = """    gl.activeTexture(gl.TEXTURE1);
    gl.bindTexture(gl.TEXTURE_2D, this.rayOriginsTexture);
    gl.uniform1i(this.rayOriginsUniform, 1);
    gl.uniform1i(this.heightUniform, height);
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);

    const outputFloatCount = width * height * 16;
    // StaticSample is four float32 values. Preserve the GPU-computed IEEE-754
    // bit patterns exactly through a Uint32 view of WASM memory. Ordinary
    // ArrayBuffer-backed WASM can receive readPixels directly; pthread shared
    // memory keeps the browser-owned staging buffer for compatibility.
    const target = new Uint32Array(
      heap.buffer,
      heap.byteOffset + outputPointer,
      outputFloatCount
    );
    const readback = usesSharedMemory ? this.resultBuffer : target;

    const started = performance.now();
"""
count = static_text.count(static_old)
if count != 1:
    raise SystemExit(f"static tracer replacement count={count}")
static_path.write_text(static_text.replace(static_old, static_new, 1))

frame_path = Path("web/src/presentation/frameBackend.ts")
frame_text = frame_path.read_text()
frame_old = """    // WebGL2 allows shared ArrayBuffer views for pixel upload. Probe once and
    // remove a full-frame CPU copy when the implementation accepts it; retain
    // the existing reusable ordinary ArrayBuffer fallback for older bindings.
    if (usesSharedMemory && this.sharedUploadSupported !== false) {
      try {
        uploadPixels(source);
        this.sharedUploadSupported = true;
      } catch {
        this.sharedUploadSupported = false;
        if (!this.sharedCopy || this.sharedCopy.byteLength !== byteLength) {
          this.sharedCopy = new Uint8Array(byteLength);
        }
        this.sharedCopy.set(source);
        uploadPixels(this.sharedCopy);
      }
    } else if (usesSharedMemory) {
      if (!this.sharedCopy || this.sharedCopy.byteLength !== byteLength) {
        this.sharedCopy = new Uint8Array(byteLength);
      }
      this.sharedCopy.set(source);
      uploadPixels(this.sharedCopy);
    } else {
      uploadPixels(source);
    }
"""
frame_new = """    // WebGL2 allows shared ArrayBuffer views for pixel upload. Probe once and
    // remove a full-frame CPU copy when the implementation accepts it; retain
    // the existing reusable ordinary ArrayBuffer fallback for older bindings.
    if (usesSharedMemory) {
      let directUploadSucceeded = false;
      if (this.sharedUploadSupported !== false) {
        try {
          // WebGL reports many invalid BufferSource combinations through the
          // error flag rather than by throwing. Clear stale errors only for the
          // one-time probe, then verify that the direct shared upload really
          // succeeded before remembering support.
          if (this.sharedUploadSupported === null) {
            while (gl.getError() !== gl.NO_ERROR) { /* drain */ }
          }
          uploadPixels(source);
          if (this.sharedUploadSupported === null) {
            this.sharedUploadSupported = gl.getError() === gl.NO_ERROR;
          }
          directUploadSucceeded = this.sharedUploadSupported === true;
        } catch {
          this.sharedUploadSupported = false;
        }
      }

      if (!directUploadSucceeded) {
        // A failed texImage2D probe may have advanced our bookkeeping even
        // though WebGL rejected the source. Force the fallback to allocate the
        // texture again with an ordinary ArrayBuffer-backed view.
        this.textureWidth = 0;
        this.textureHeight = 0;
        if (!this.sharedCopy || this.sharedCopy.byteLength !== byteLength) {
          this.sharedCopy = new Uint8Array(byteLength);
        }
        this.sharedCopy.set(source);
        uploadPixels(this.sharedCopy);
      }
    } else {
      uploadPixels(source);
    }
"""
count = frame_text.count(frame_old)
if count != 1:
    raise SystemExit(f"frame backend replacement count={count}")
frame_path.write_text(frame_text.replace(frame_old, frame_new, 1))
