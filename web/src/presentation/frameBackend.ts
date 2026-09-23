import { WebGlStaticTracer } from './staticTracer';
import { browserMode } from '../browserMode';

export type FrameBackendName = 'webgl2' | 'canvas2d';

export interface FrameBackend {
  readonly name: FrameBackendName;
  present(
    heap: Uint8Array,
    pointer: number,
    width: number,
    height: number
  ): void;
}

function compileShader(
  gl: WebGL2RenderingContext,
  type: number,
  source: string
): WebGLShader {
  const shader = gl.createShader(type);
  if (!shader) throw new Error('WebGL2 could not allocate a shader.');
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    const log = gl.getShaderInfoLog(shader) || 'unknown shader compile failure';
    gl.deleteShader(shader);
    throw new Error(log);
  }
  return shader;
}

function createProgram(gl: WebGL2RenderingContext): WebGLProgram {
  const vertex = compileShader(
    gl,
    gl.VERTEX_SHADER,
    `#version 300 es
    precision highp float;
    out vec2 vUv;

    void main() {
      vec2 position;
      if (gl_VertexID == 0) {
        position = vec2(-1.0, -1.0);
      } else if (gl_VertexID == 1) {
        position = vec2(3.0, -1.0);
      } else {
        position = vec2(-1.0, 3.0);
      }
      vUv = position * 0.5 + 0.5;
      gl_Position = vec4(position, 0.0, 1.0);
    }`
  );

  const fragment = compileShader(
    gl,
    gl.FRAGMENT_SHADER,
    `#version 300 es
    precision highp float;
    uniform sampler2D uFrame;
    in vec2 vUv;
    out vec4 outColour;

    void main() {
      // WASM row zero is the top of the image, while WebGL texture row zero
      // is addressed from the bottom of the viewport.
      outColour = texture(uFrame, vec2(vUv.x, 1.0 - vUv.y));
    }`
  );

  const program = gl.createProgram();
  if (!program) throw new Error('WebGL2 could not allocate a program.');
  gl.attachShader(program, vertex);
  gl.attachShader(program, fragment);
  gl.linkProgram(program);
  gl.deleteShader(vertex);
  gl.deleteShader(fragment);

  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    const log = gl.getProgramInfoLog(program) || 'unknown program link failure';
    gl.deleteProgram(program);
    throw new Error(log);
  }
  return program;
}

class WebGl2FrameBackend implements FrameBackend {
  readonly name = 'webgl2' as const;

  private readonly gl: WebGL2RenderingContext;
  private readonly program: WebGLProgram;
  private readonly texture: WebGLTexture;
  private readonly vao: WebGLVertexArrayObject;
  private textureWidth = 0;
  private textureHeight = 0;
  private sharedCopy: Uint8Array | null = null;
  private sharedUploadSupported: boolean | null = null;
  private readonly timerExtension: any;
  private pendingTimerQuery: WebGLQuery | null = null;

  constructor(canvas: HTMLCanvasElement) {
    // Do not vendor-filter adapters. WebGL2 must work with whatever GPU the
    // browser exposes, whether integrated/unified or discrete: Apple Silicon,
    // Intel/AMD integrated graphics, and discrete AMD/NVIDIA are all valid.
    const gl = canvas.getContext('webgl2', {
      alpha: false,
      antialias: false,
      depth: false,
      stencil: false,
      premultipliedAlpha: false,
      preserveDrawingBuffer: false,
      powerPreference: 'high-performance'
    });
    if (!gl) throw new Error('WebGL2 is unavailable.');

    const program = createProgram(gl);
    const texture = gl.createTexture();
    const vao = gl.createVertexArray();
    if (!texture || !vao) {
      throw new Error('WebGL2 could not allocate frame presentation resources.');
    }

    this.gl = gl;
    this.program = program;
    this.texture = texture;
    this.vao = vao;
    this.timerExtension = gl.getExtension('EXT_disjoint_timer_query_webgl2');

    const params = new URLSearchParams(location.search);
    const gpuStaticOverride = params.get('gpuStatic');
    const allowGpuStatic =
      gpuStaticOverride === '1' ||
      (gpuStaticOverride !== '0' && browserMode().webgl);

    if (allowGpuStatic) {
      try {
        const staticTracer = new WebGlStaticTracer(gl);
        (globalThis as any).isowebTraceStaticWebGl = staticTracer.trace;
        window.isowebGpuStaticAvailable = true;
      } catch (error) {
        window.isowebGpuStaticAvailable = false;
        window.isowebFrameGpuBytes = 0;
        window.isowebStaticGpuBytes = 0;
        window.isowebGpuStaticError = error instanceof Error ? error.message : String(error);
        console.warn('WebGL2 static ray tracing unavailable; using CPU static renderer.', error);
      }
    }

    gl.bindVertexArray(vao);
    gl.useProgram(program);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    const sampler = gl.getUniformLocation(program, 'uFrame');
    gl.uniform1i(sampler, 0);
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);
  }

  present(heap: Uint8Array, pointer: number, width: number, height: number): void {
    const gl = this.gl;

    if (this.pendingTimerQuery && this.timerExtension) {
      const available = gl.getQueryParameter(
        this.pendingTimerQuery,
        gl.QUERY_RESULT_AVAILABLE
      );
      if (available) {
        const disjoint = gl.getParameter(this.timerExtension.GPU_DISJOINT_EXT);
        if (!disjoint) {
          const nanoseconds = gl.getQueryParameter(
            this.pendingTimerQuery,
            gl.QUERY_RESULT
          ) as number;
          window.isowebLastGpuMilliseconds = nanoseconds / 1_000_000;
        }
        gl.deleteQuery(this.pendingTimerQuery);
        this.pendingTimerQuery = null;
      }
    }

    let timerQuery: WebGLQuery | null = null;
    if (this.timerExtension && !this.pendingTimerQuery) {
      timerQuery = gl.createQuery();
      if (timerQuery) {
        gl.beginQuery(this.timerExtension.TIME_ELAPSED_EXT, timerQuery);
      }
    }

    const byteLength = width * height * 4;
    const source = new Uint8Array(
      heap.buffer,
      heap.byteOffset + pointer,
      byteLength
    );

    const usesSharedMemory =
      typeof SharedArrayBuffer === 'function' &&
      heap.buffer instanceof SharedArrayBuffer;

    gl.viewport(0, 0, width, height);
    gl.bindVertexArray(this.vao);
    gl.useProgram(this.program);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.texture);

    const uploadPixels = (pixels: Uint8Array): void => {
      if (this.textureWidth !== width || this.textureHeight !== height) {
        gl.texImage2D(
          gl.TEXTURE_2D,
          0,
          gl.RGBA8,
          width,
          height,
          0,
          gl.RGBA,
          gl.UNSIGNED_BYTE,
          pixels
        );
        this.textureWidth = width;
        this.textureHeight = height;
        window.isowebFrameGpuBytes = byteLength;
      } else {
        gl.texSubImage2D(
          gl.TEXTURE_2D,
          0,
          0,
          0,
          width,
          height,
          gl.RGBA,
          gl.UNSIGNED_BYTE,
          pixels
        );
      }
    };

    // WebGL2 allows shared ArrayBuffer views for pixel upload. Probe once and
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

    gl.drawArrays(gl.TRIANGLES, 0, 3);

    if (timerQuery && this.timerExtension) {
      gl.endQuery(this.timerExtension.TIME_ELAPSED_EXT);
      this.pendingTimerQuery = timerQuery;
    }
  }
}

class Canvas2dFrameBackend implements FrameBackend {
  readonly name = 'canvas2d' as const;

  private readonly context: CanvasRenderingContext2D;
  private frameImage: ImageData | null = null;
  private frameBuffer: ArrayBufferLike | null = null;
  private framePointer = -1;
  private frameWidth = 0;
  private frameHeight = 0;
  private frameUsesSharedMemory = false;

  constructor(canvas: HTMLCanvasElement) {
    const context = canvas.getContext('2d', { alpha: false });
    if (!context) throw new Error('Canvas 2D is unavailable.');
    this.context = context;
  }

  present(heap: Uint8Array, pointer: number, width: number, height: number): void {
    const byteLength = width * height * 4;
    const usesSharedMemory =
      typeof SharedArrayBuffer === 'function' &&
      heap.buffer instanceof SharedArrayBuffer;

    if (
      !this.frameImage ||
      this.frameBuffer !== heap.buffer ||
      this.framePointer !== pointer ||
      this.frameWidth !== width ||
      this.frameHeight !== height ||
      this.frameUsesSharedMemory !== usesSharedMemory
    ) {
      if (usesSharedMemory) {
        this.frameImage = this.context.createImageData(width, height);
      } else {
        const view = new Uint8ClampedArray(
          heap.buffer,
          heap.byteOffset + pointer,
          byteLength
        );
        this.frameImage = new ImageData(view, width, height);
      }
      this.frameBuffer = heap.buffer;
      this.framePointer = pointer;
      this.frameWidth = width;
      this.frameHeight = height;
      this.frameUsesSharedMemory = usesSharedMemory;
    }

    if (usesSharedMemory) {
      const sharedView = new Uint8ClampedArray(
        heap.buffer,
        heap.byteOffset + pointer,
        byteLength
      );
      this.frameImage.data.set(sharedView);
    }

    this.context.putImageData(this.frameImage, 0, 0);
  }
}

export function createFrameBackend(canvas: HTMLCanvasElement): FrameBackend {
  (globalThis as any).isowebTraceStaticWebGl = undefined;
  window.isowebGpuStaticAvailable = false;
  const params = new URLSearchParams(location.search);
  const forceCanvas2d = params.get('presentation') === 'canvas2d';

  if (!forceCanvas2d) {
    try {
      return new WebGl2FrameBackend(canvas);
    } catch (error) {
      console.warn('WebGL2 presentation unavailable; using Canvas2D.', error);
    }
  }
  return new Canvas2dFrameBackend(canvas);
}
