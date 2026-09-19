export type StaticTraceCallback = (
  heap: Uint8Array,
  scenePointer: number,
  sceneFloatCount: number,
  outputPointer: number,
  width: number,
  height: number
) => boolean;

function compile(
  gl: WebGL2RenderingContext,
  type: number,
  source: string
): WebGLShader {
  const shader = gl.createShader(type);
  if (!shader) throw new Error('Could not allocate WebGL static-trace shader.');
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    const log = gl.getShaderInfoLog(shader) || 'unknown compile error';
    gl.deleteShader(shader);
    throw new Error(log);
  }
  return shader;
}

function link(
  gl: WebGL2RenderingContext,
  vertexSource: string,
  fragmentSource: string
): WebGLProgram {
  const vertex = compile(gl, gl.VERTEX_SHADER, vertexSource);
  const fragment = compile(gl, gl.FRAGMENT_SHADER, fragmentSource);
  const program = gl.createProgram();
  if (!program) throw new Error('Could not allocate WebGL static-trace program.');
  gl.attachShader(program, vertex);
  gl.attachShader(program, fragment);
  gl.linkProgram(program);
  gl.deleteShader(vertex);
  gl.deleteShader(fragment);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    const log = gl.getProgramInfoLog(program) || 'unknown link error';
    gl.deleteProgram(program);
    throw new Error(log);
  }
  return program;
}

const VERTEX_SOURCE = `#version 300 es
precision highp float;

void main() {
  vec2 p;
  if (gl_VertexID == 0) p = vec2(-1.0, -1.0);
  else if (gl_VertexID == 1) p = vec2(3.0, -1.0);
  else p = vec2(-1.0, 3.0);
  gl_Position = vec4(p, 0.0, 1.0);
}
`;

const FRAGMENT_SOURCE = `#version 300 es
precision highp float;
precision highp int;

uniform sampler2D uScene;
uniform int uFrameWidth;
uniform int uFrameHeight;

out vec4 outSample;

const float EPSILON = 0.0015;
const float FAR_DISTANCE = 1000.0;
const float NO_HIT_DISTANCE = 1.0e30;
const int MAX_VISUAL_BOXES = 128;
const int MAX_SHADOW_BOXES = 128;
const int MAX_SPHERES = 8;
const int MAX_ROOMS = 16;
const int MAX_HOLES = 8;

vec4 recordAt(int index) {
  return texelFetch(uScene, ivec2(index, 0), 0);
}

float roundCpp(float value) {
  return value >= 0.0 ? floor(value + 0.5) : ceil(value - 0.5);
}

vec3 floorColour(vec3 point, vec3 darkColour, vec3 lightColour) {
  int gx = int(floor(point.x + 20.0));
  int gy = int(floor(point.y + 20.0));
  vec3 colour = ((gx + gy) & 1) != 0 ? darkColour : lightColour;
  float edgeX = abs(point.x - roundCpp(point.x));
  float edgeY = abs(point.y - roundCpp(point.y));
  if (min(edgeX, edgeY) < 0.022) colour *= 0.78;
  return colour;
}

bool intersectBox(
  vec3 origin,
  vec3 direction,
  vec3 centre,
  vec3 halfExtent,
  float minimumT,
  float maximumT,
  out float hitT,
  out vec3 hitNormal
) {
  vec3 localOrigin = origin - centre;
  float nearT = minimumT;
  float farT = maximumT;
  int normalAxis = -1;
  float normalSign = 0.0;

  for (int axis = 0; axis < 3; ++axis) {
    float o = axis == 0 ? localOrigin.x : (axis == 1 ? localOrigin.y : localOrigin.z);
    float d = axis == 0 ? direction.x : (axis == 1 ? direction.y : direction.z);
    float h = axis == 0 ? halfExtent.x : (axis == 1 ? halfExtent.y : halfExtent.z);

    if (abs(d) < 1.0e-7) {
      if (o < -h || o > h) return false;
      continue;
    }

    float inverse = 1.0 / d;
    float t0 = (-h - o) * inverse;
    float t1 = (h - o) * inverse;
    float signValue = -1.0;
    if (t0 > t1) {
      float swapValue = t0;
      t0 = t1;
      t1 = swapValue;
      signValue = 1.0;
    }
    if (t0 > nearT) {
      nearT = t0;
      normalAxis = axis;
      normalSign = signValue;
    }
    farT = min(farT, t1);
    if (farT < nearT) return false;
  }

  if (normalAxis < 0) return false;
  hitT = nearT;
  hitNormal = normalAxis == 0
    ? vec3(normalSign, 0.0, 0.0)
    : normalAxis == 1
      ? vec3(0.0, normalSign, 0.0)
      : vec3(0.0, 0.0, normalSign);
  return true;
}

bool intersectSphere(
  vec3 origin,
  vec3 direction,
  vec3 centre,
  float radius,
  float minimumT,
  float maximumT,
  out float hitT,
  out vec3 hitNormal
) {
  vec3 offset = origin - centre;
  float a = dot(direction, direction);
  float halfB = dot(offset, direction);
  float c = dot(offset, offset) - radius * radius;
  float discriminant = halfB * halfB - a * c;
  if (discriminant < 0.0) return false;

  float root = sqrt(discriminant);
  float t = (-halfB - root) / a;
  if (t < minimumT || t > maximumT) {
    t = (-halfB + root) / a;
    if (t < minimumT || t > maximumT) return false;
  }
  hitT = t;
  hitNormal = normalize(origin + direction * t - centre);
  return true;
}

bool insideHole(vec3 point, int holeStart, int holeCount) {
  for (int i = 0; i < MAX_HOLES; ++i) {
    if (i >= holeCount) break;
    vec4 hole = recordAt(holeStart + i);
    if (
      point.x >= hole.x && point.x <= hole.y &&
      point.y >= hole.z && point.y <= hole.w
    ) {
      return true;
    }
  }
  return false;
}

bool intersectGround(
  vec3 origin,
  vec3 direction,
  float minimumT,
  float maximumT,
  int roomStart,
  int roomCount,
  int holeStart,
  int holeCount,
  out float hitT,
  out vec3 hitNormal
) {
  if (abs(direction.z) < 1.0e-7) return false;
  bool found = false;
  float closest = maximumT;

  for (int i = 0; i < MAX_ROOMS; ++i) {
    if (i >= roomCount) break;
    vec4 room = recordAt(roomStart + i * 2);
    float floorZ = recordAt(roomStart + i * 2 + 1).x;
    float t = (floorZ - origin.z) / direction.z;
    if (t < minimumT || t > closest) continue;

    vec3 point = origin + direction * t;
    if (
      point.x < room.x - room.z - EPSILON ||
      point.x > room.x + room.z + EPSILON ||
      point.y < room.y - room.w - EPSILON ||
      point.y > room.y + room.w + EPSILON
    ) {
      continue;
    }
    if (insideHole(point, holeStart, holeCount)) continue;

    found = true;
    closest = t;
  }

  if (!found) return false;
  hitT = closest;
  hitNormal = vec3(0.0, 0.0, 1.0);
  return true;
}

bool shadowed(
  vec3 origin,
  vec3 direction,
  float maximumT,
  int shadowStart,
  int shadowCount,
  int sphereStart,
  int sphereCount,
  int roomStart,
  int roomCount,
  int holeStart,
  int holeCount
) {
  float t;
  vec3 normalValue;

  for (int i = 0; i < MAX_SHADOW_BOXES; ++i) {
    if (i >= shadowCount) break;
    vec4 centre = recordAt(shadowStart + i * 2);
    vec4 halfExtent = recordAt(shadowStart + i * 2 + 1);
    if (
      intersectBox(
        origin,
        direction,
        centre.xyz,
        halfExtent.xyz,
        EPSILON,
        maximumT,
        t,
        normalValue
      )
    ) {
      return true;
    }
  }

  for (int i = 0; i < MAX_SPHERES; ++i) {
    if (i >= sphereCount) break;
    vec4 sphere = recordAt(sphereStart + i * 2);
    if (
      intersectSphere(
        origin,
        direction,
        sphere.xyz,
        sphere.w,
        EPSILON,
        maximumT,
        t,
        normalValue
      )
    ) {
      return true;
    }
  }

  return intersectGround(
    origin,
    direction,
    EPSILON,
    maximumT,
    roomStart,
    roomCount,
    holeStart,
    holeCount,
    t,
    normalValue
  );
}

void main() {
  vec4 header0 = recordAt(0);
  vec4 header1 = recordAt(1);
  vec4 header2 = recordAt(2);
  vec4 header3 = recordAt(3);

  int visualCount = int(header0.y + 0.5);
  int shadowCount = int(header0.z + 0.5);
  int sphereCount = int(header0.w + 0.5);
  int roomCount = int(header1.x + 0.5);
  int holeCount = int(header1.y + 0.5);

  int visualStart = 8;
  int shadowStart = visualStart + visualCount * 3;
  int sphereStart = shadowStart + shadowCount * 2;
  int roomStart = sphereStart + sphereCount * 2;
  int holeStart = roomStart + roomCount * 2;

  vec3 lightPosition = vec3(header1.z, header1.w, header2.x);
  vec3 floorDark = header2.yzw;
  vec3 floorLight = header3.xyz;
  vec3 cornerOrigin = recordAt(4).xyz;
  vec3 rayDirection = recordAt(5).xyz;
  vec3 rightStep = recordAt(6).xyz;
  vec3 downStep = recordAt(7).xyz;

  int outputX = int(floor(gl_FragCoord.x));
  int sampleIndex = outputX & 3;
  int pixelX = outputX >> 2;
  int pixelY = uFrameHeight - 1 - int(floor(gl_FragCoord.y));

  float sampleX = (sampleIndex == 0 || sampleIndex == 2) ? 0.25 : 0.75;
  float sampleY = sampleIndex < 2 ? 0.25 : 0.75;
  vec3 rayOrigin =
    cornerOrigin +
    rightStep * (float(pixelX) + sampleX) +
    downStep * (float(pixelY) + sampleY);

  float closest = FAR_DISTANCE;
  bool found = false;
  vec3 hitPoint = vec3(0.0);
  vec3 hitNormal = vec3(0.0, 0.0, 1.0);
  vec3 hitColour = vec3(0.0);

  for (int i = 0; i < MAX_VISUAL_BOXES; ++i) {
    if (i >= visualCount) break;
    vec4 centre = recordAt(visualStart + i * 3);
    vec4 halfExtent = recordAt(visualStart + i * 3 + 1);
    vec4 authoredColour = recordAt(visualStart + i * 3 + 2);

    float t;
    vec3 normalValue;
    if (
      intersectBox(
        rayOrigin,
        rayDirection,
        centre.xyz,
        halfExtent.xyz,
        EPSILON,
        closest,
        t,
        normalValue
      )
    ) {
      found = true;
      closest = t;
      hitPoint = rayOrigin + rayDirection * t;
      hitNormal = normalValue;
      hitColour = centre.w > 0.5
        ? floorColour(hitPoint, floorDark, floorLight)
        : authoredColour.xyz;
    }
  }

  for (int i = 0; i < MAX_SPHERES; ++i) {
    if (i >= sphereCount) break;
    vec4 sphere = recordAt(sphereStart + i * 2);
    vec3 authoredColour = recordAt(sphereStart + i * 2 + 1).xyz;
    float t;
    vec3 normalValue;
    if (
      intersectSphere(
        rayOrigin,
        rayDirection,
        sphere.xyz,
        sphere.w,
        EPSILON,
        closest,
        t,
        normalValue
      )
    ) {
      found = true;
      closest = t;
      hitPoint = rayOrigin + rayDirection * t;
      hitNormal = normalValue;
      hitColour = authoredColour;
    }
  }

  float groundT;
  vec3 groundNormal;
  if (
    intersectGround(
      rayOrigin,
      rayDirection,
      EPSILON,
      closest,
      roomStart,
      roomCount,
      holeStart,
      holeCount,
      groundT,
      groundNormal
    )
  ) {
    found = true;
    closest = groundT;
    hitPoint = rayOrigin + rayDirection * groundT;
    hitNormal = groundNormal;
    hitColour = floorColour(hitPoint, floorDark, floorLight);
  }

  if (!found) {
    float backgroundY = (float(pixelY) + sampleY) / float(uFrameHeight);
    float t = clamp(backgroundY, 0.0, 1.0);
    vec3 colour =
      vec3(0.075, 0.12, 0.18) * (1.0 - t) +
      vec3(0.20, 0.28, 0.34) * t;
    outSample = vec4(colour, NO_HIT_DISTANCE);
    return;
  }

  vec3 toLight = lightPosition - hitPoint;
  float lightDistanceSquared = dot(toLight, toLight);
  vec3 shaded = hitColour;

  if (lightDistanceSquared >= 1.0e-12) {
    float lightDistance = sqrt(lightDistanceSquared);
    vec3 lightDirection = toLight / lightDistance;
    float diffuse = max(0.0, dot(hitNormal, lightDirection));
    float attenuation = 1.0 / (1.0 + 0.018 * lightDistanceSquared);
    float visibility = shadowed(
      hitPoint + hitNormal * EPSILON,
      lightDirection,
      lightDistance - EPSILON,
      shadowStart,
      shadowCount,
      sphereStart,
      sphereCount,
      roomStart,
      roomCount,
      holeStart,
      holeCount
    ) ? 0.0 : 1.0;

    shaded = min(
      hitColour * (0.19 + visibility * diffuse * attenuation * 1.18),
      vec3(1.0)
    );
  }

  outSample = vec4(shaded, closest);
}
`;

export class WebGlStaticTracer {
  private readonly program: WebGLProgram;
  private readonly vao: WebGLVertexArrayObject;
  private readonly sceneTexture: WebGLTexture;
  private readonly resultTexture: WebGLTexture;
  private readonly framebuffer: WebGLFramebuffer;
  private readonly sceneUniform: WebGLUniformLocation;
  private readonly widthUniform: WebGLUniformLocation;
  private readonly heightUniform: WebGLUniformLocation;
  private resultWidth = 0;
  private resultHeight = 0;
  private resultBuffer = new Float32Array(0);

  constructor(private readonly gl: WebGL2RenderingContext) {
    if (!gl.getExtension('EXT_color_buffer_float')) {
      throw new Error('EXT_color_buffer_float is unavailable.');
    }

    this.program = link(gl, VERTEX_SOURCE, FRAGMENT_SOURCE);
    const vao = gl.createVertexArray();
    const sceneTexture = gl.createTexture();
    const resultTexture = gl.createTexture();
    const framebuffer = gl.createFramebuffer();
    const sceneUniform = gl.getUniformLocation(this.program, 'uScene');
    const widthUniform = gl.getUniformLocation(this.program, 'uFrameWidth');
    const heightUniform = gl.getUniformLocation(this.program, 'uFrameHeight');

    if (
      !vao ||
      !sceneTexture ||
      !resultTexture ||
      !framebuffer ||
      sceneUniform === null ||
      widthUniform === null ||
      heightUniform === null
    ) {
      throw new Error('Could not allocate WebGL static-trace resources.');
    }

    this.vao = vao;
    this.sceneTexture = sceneTexture;
    this.resultTexture = resultTexture;
    this.framebuffer = framebuffer;
    this.sceneUniform = sceneUniform;
    this.widthUniform = widthUniform;
    this.heightUniform = heightUniform;

    gl.bindTexture(gl.TEXTURE_2D, sceneTexture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    gl.bindTexture(gl.TEXTURE_2D, resultTexture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  }

  trace: StaticTraceCallback = (
    heap,
    scenePointer,
    sceneFloatCount,
    outputPointer,
    width,
    height
  ) => {
    if (width <= 0 || height <= 0 || sceneFloatCount < 32 || (sceneFloatCount & 3) !== 0) {
      return false;
    }

    const gl = this.gl;
    const outputWidth = width * 4;
    const maximumTextureSize = gl.getParameter(gl.MAX_TEXTURE_SIZE) as number;
    const sceneRecordCount = sceneFloatCount / 4;
    if (
      outputWidth > maximumTextureSize ||
      height > maximumTextureSize ||
      sceneRecordCount > maximumTextureSize
    ) {
      return false;
    }

    const rawScene = new Float32Array(
      heap.buffer,
      heap.byteOffset + scenePointer,
      sceneFloatCount
    );
    // WebGL implementations are not required to accept SharedArrayBuffer
    // views. The descriptor is tiny compared with the framebuffer, so keep
    // this upload browser-owned on every static rebuild.
    const scene = new Float32Array(rawScene);

    const header = scene.subarray(0, 8);
    const visualCount = Math.round(header[1]);
    const shadowCount = Math.round(header[2]);
    const sphereCount = Math.round(header[3]);
    const roomCount = Math.round(header[4]);
    const holeCount = Math.round(header[5]);
    if (
      visualCount > 128 ||
      shadowCount > 128 ||
      sphereCount > 8 ||
      roomCount > 16 ||
      holeCount > 8
    ) {
      return false;
    }

    gl.bindTexture(gl.TEXTURE_2D, this.sceneTexture);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA32F,
      sceneRecordCount,
      1,
      0,
      gl.RGBA,
      gl.FLOAT,
      scene
    );

    if (this.resultWidth !== outputWidth || this.resultHeight !== height) {
      gl.bindTexture(gl.TEXTURE_2D, this.resultTexture);
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RGBA32F,
        outputWidth,
        height,
        0,
        gl.RGBA,
        gl.FLOAT,
        null
      );
      this.resultWidth = outputWidth;
      this.resultHeight = height;
      this.resultBuffer = new Float32Array(outputWidth * height * 4);
    }

    gl.bindFramebuffer(gl.FRAMEBUFFER, this.framebuffer);
    gl.framebufferTexture2D(
      gl.FRAMEBUFFER,
      gl.COLOR_ATTACHMENT0,
      gl.TEXTURE_2D,
      this.resultTexture,
      0
    );
    if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) !== gl.FRAMEBUFFER_COMPLETE) {
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      return false;
    }

    gl.viewport(0, 0, outputWidth, height);
    gl.bindVertexArray(this.vao);
    gl.useProgram(this.program);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.sceneTexture);
    gl.uniform1i(this.sceneUniform, 0);
    gl.uniform1i(this.widthUniform, width);
    gl.uniform1i(this.heightUniform, height);
    gl.disable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);

    const started = performance.now();
    gl.drawArrays(gl.TRIANGLES, 0, 3);
    gl.readPixels(
      0,
      0,
      outputWidth,
      height,
      gl.RGBA,
      gl.FLOAT,
      this.resultBuffer
    );
    window.isowebLastGpuStaticMilliseconds = performance.now() - started;

    const outputFloatCount = width * height * 16;
    const target = new Float32Array(
      heap.buffer,
      heap.byteOffset + outputPointer,
      outputFloatCount
    );
    const rowFloatCount = outputWidth * 4;
    for (let y = 0; y < height; ++y) {
      const sourceStart = (height - 1 - y) * rowFloatCount;
      target.set(
        this.resultBuffer.subarray(sourceStart, sourceStart + rowFloatCount),
        y * rowFloatCount
      );
    }

    window.isowebGpuStaticTraceCount = (window.isowebGpuStaticTraceCount ?? 0) + 1;
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    return true;
  };
}
