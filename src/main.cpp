#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <emscripten/emscripten.h>
#include "DFPSR/api/drawAPI.h"
#include "DFPSR/api/imageAPI.h"

namespace {

constexpr int FRAME_WIDTH = 512;
constexpr int FRAME_HEIGHT = 288;

constexpr int ROTATE_ARROW_WIDTH = 56;
constexpr int ROTATE_ARROW_HEIGHT = 44;
constexpr int ROTATE_LEFT_X = 18;
constexpr int ROTATE_GAP = 10;
constexpr int CONTROL_BOTTOM = 18;

constexpr int PAN_ARROW_SIZE = 38;
constexpr int PAN_PAD_STEP = 40;
constexpr int PAN_PAD_RIGHT = 18;
constexpr int PAN_PAD_BOTTOM = 16;

constexpr float EPSILON = 0.0015f;
constexpr float FAR_DISTANCE = 1000.0f;
constexpr float PI = 3.14159265358979323846f;
constexpr float GROUND_LIMIT = 4.40f;
constexpr float PAN_EDGE_INSET = 1.15f;
constexpr float PAN_LIMIT = GROUND_LIMIT - PAN_EDGE_INSET;

struct Vec3 {
    float x;
    float y;
    float z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float scale) const { return Vec3(x * scale, y * scale, z * scale); }
    Vec3 operator/(float scale) const { return Vec3(x / scale, y / scale, z / scale); }
};

Vec3 operator*(float scale, const Vec3& value) {
    return value * scale;
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float length(const Vec3& value) {
    return std::sqrt(dot(value, value));
}

Vec3 normalise(const Vec3& value) {
    const float magnitude = length(value);
    return magnitude > 0.0f ? value / magnitude : Vec3();
}

Vec3 rotateQuarterAroundZ(const Vec3& value, int clockwiseQuarterTurns) {
    switch ((clockwiseQuarterTurns % 4 + 4) % 4) {
        case 1: return Vec3(value.y, -value.x, value.z);
        case 2: return Vec3(-value.x, -value.y, value.z);
        case 3: return Vec3(-value.y, value.x, value.z);
        default: return value;
    }
}

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

enum class Surface {
    None,
    Ground,
    Cube,
    Sphere
};

struct Hit {
    bool found;
    float t;
    Vec3 point;
    Vec3 normal;
    Surface surface;

    Hit() : found(false), t(FAR_DISTANCE), point(), normal(), surface(Surface::None) {}
};

// Z is world-up. The world, objects and light never rotate or pan.
// Only the camera focus moves in X/Y and yaws in quarter turns around Z.
const Vec3 CUBE_MIN(-1.85f, -0.15f, 0.0f);
const Vec3 CUBE_MAX(-0.25f, 1.45f, 1.55f);
const Vec3 SPHERE_CENTRE(1.05f, -0.25f, 0.90f);
constexpr float SPHERE_RADIUS = 0.90f;
const Vec3 LIGHT_POSITION(-3.60f, -4.20f, 6.50f);
const Vec3 BASE_FOCUS(0.0f, 0.15f, 0.55f);

int cameraQuarterTurns = 0;
float cameraPanX = 0.0f;
float cameraPanY = 0.0f;

Vec3 cameraForward() {
    return rotateQuarterAroundZ(normalise(Vec3(1.0f, 1.0f, -1.0f)), cameraQuarterTurns);
}

Vec3 cameraGroundRight() {
    return normalise(cross(cameraForward(), Vec3(0.0f, 0.0f, 1.0f)));
}

Vec3 cameraGroundDown() {
    const Vec3 forward = cameraForward();
    return normalise(Vec3(forward.x, forward.y, 0.0f));
}

void panCameraOnGround(float screenRight, float screenDown) {
    const Vec3 groundRight = cameraGroundRight();
    const Vec3 groundDown = cameraGroundDown();
    const Vec3 delta = groundRight * screenRight + groundDown * screenDown;

    cameraPanX = std::max(-PAN_LIMIT, std::min(PAN_LIMIT, cameraPanX + delta.x));
    cameraPanY = std::max(-PAN_LIMIT, std::min(PAN_LIMIT, cameraPanY + delta.y));
}

bool intersectSphere(const Ray& ray, float tMin, float tMax, Hit& hit) {
    const Vec3 oc = ray.origin - SPHERE_CENTRE;
    const float a = dot(ray.direction, ray.direction);
    const float halfB = dot(oc, ray.direction);
    const float c = dot(oc, oc) - SPHERE_RADIUS * SPHERE_RADIUS;
    const float discriminant = halfB * halfB - a * c;
    if (discriminant < 0.0f) return false;

    const float rootPart = std::sqrt(discriminant);
    float root = (-halfB - rootPart) / a;
    if (root < tMin || root > tMax) {
        root = (-halfB + rootPart) / a;
        if (root < tMin || root > tMax) return false;
    }

    hit.found = true;
    hit.t = root;
    hit.point = ray.origin + ray.direction * root;
    hit.normal = normalise(hit.point - SPHERE_CENTRE);
    hit.surface = Surface::Sphere;
    return true;
}

bool intersectCube(const Ray& ray, float tMin, float tMax, Hit& hit) {
    float nearT = tMin;
    float farT = tMax;

    const float origins[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float directions[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float mins[3] = {CUBE_MIN.x, CUBE_MIN.y, CUBE_MIN.z};
    const float maxs[3] = {CUBE_MAX.x, CUBE_MAX.y, CUBE_MAX.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(directions[axis]) < 1.0e-7f) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return false;
            continue;
        }

        const float invD = 1.0f / directions[axis];
        float t0 = (mins[axis] - origins[axis]) * invD;
        float t1 = (maxs[axis] - origins[axis]) * invD;
        if (t0 > t1) std::swap(t0, t1);
        nearT = std::max(nearT, t0);
        farT = std::min(farT, t1);
        if (farT < nearT) return false;
    }

    if (nearT < tMin || nearT > tMax) return false;

    hit.found = true;
    hit.t = nearT;
    hit.point = ray.origin + ray.direction * nearT;
    hit.surface = Surface::Cube;

    const float dxMin = std::fabs(hit.point.x - CUBE_MIN.x);
    const float dxMax = std::fabs(hit.point.x - CUBE_MAX.x);
    const float dyMin = std::fabs(hit.point.y - CUBE_MIN.y);
    const float dyMax = std::fabs(hit.point.y - CUBE_MAX.y);
    const float dzMin = std::fabs(hit.point.z - CUBE_MIN.z);
    const float dzMax = std::fabs(hit.point.z - CUBE_MAX.z);

    float closest = dxMin;
    hit.normal = Vec3(-1.0f, 0.0f, 0.0f);
    if (dxMax < closest) { closest = dxMax; hit.normal = Vec3(1.0f, 0.0f, 0.0f); }
    if (dyMin < closest) { closest = dyMin; hit.normal = Vec3(0.0f, -1.0f, 0.0f); }
    if (dyMax < closest) { closest = dyMax; hit.normal = Vec3(0.0f, 1.0f, 0.0f); }
    if (dzMin < closest) { closest = dzMin; hit.normal = Vec3(0.0f, 0.0f, -1.0f); }
    if (dzMax < closest) { hit.normal = Vec3(0.0f, 0.0f, 1.0f); }
    return true;
}

bool intersectGround(const Ray& ray, float tMin, float tMax, Hit& hit) {
    if (std::fabs(ray.direction.z) < 1.0e-7f) return false;
    const float t = -ray.origin.z / ray.direction.z;
    if (t < tMin || t > tMax) return false;

    const Vec3 point = ray.origin + ray.direction * t;
    if (std::fabs(point.x) > GROUND_LIMIT || std::fabs(point.y) > GROUND_LIMIT) return false;

    hit.found = true;
    hit.t = t;
    hit.point = point;
    hit.normal = Vec3(0.0f, 0.0f, 1.0f);
    hit.surface = Surface::Ground;
    return true;
}

Hit traceClosest(const Ray& ray, float tMin, float tMax) {
    Hit closest;
    Hit candidate;

    if (intersectCube(ray, tMin, tMax, candidate)) {
        closest = candidate;
        tMax = candidate.t;
    }

    candidate = Hit();
    if (intersectSphere(ray, tMin, tMax, candidate)) {
        closest = candidate;
        tMax = candidate.t;
    }

    candidate = Hit();
    if (intersectGround(ray, tMin, tMax, candidate)) {
        closest = candidate;
    }

    return closest;
}

bool isOccluded(const Vec3& point, const Vec3& normal) {
    const Vec3 toLight = LIGHT_POSITION - point;
    const float lightDistance = length(toLight);
    const Vec3 lightDirection = toLight / lightDistance;
    const Ray shadowRay{point + normal * EPSILON, lightDirection};
    const Hit blocker = traceClosest(shadowRay, EPSILON, lightDistance - EPSILON);
    return blocker.found;
}

Vec3 materialColour(const Hit& hit) {
    if (hit.surface == Surface::Cube) {
        return Vec3(0.18f, 0.48f, 0.88f);
    }
    if (hit.surface == Surface::Sphere) {
        return Vec3(0.95f, 0.43f, 0.12f);
    }

    const int tileX = static_cast<int>(std::floor(hit.point.x + 20.0f));
    const int tileY = static_cast<int>(std::floor(hit.point.y + 20.0f));
    const bool alternate = ((tileX + tileY) & 1) != 0;
    const float base = alternate ? 0.63f : 0.69f;
    return Vec3(base * 0.90f, base * 0.96f, base);
}

Vec3 shade(const Hit& hit) {
    const Vec3 base = materialColour(hit);
    const Vec3 toLight = LIGHT_POSITION - hit.point;
    const float distanceToLight = length(toLight);
    const Vec3 lightDirection = toLight / distanceToLight;
    const float diffuse = std::max(0.0f, dot(hit.normal, lightDirection));
    const float attenuation = 1.0f / (1.0f + 0.018f * distanceToLight * distanceToLight);
    const float visibility = isOccluded(hit.point, hit.normal) ? 0.0f : 1.0f;
    const float brightness = 0.19f + visibility * diffuse * attenuation * 1.18f;

    Vec3 colour = base * brightness;

    if (hit.surface == Surface::Ground) {
        const float edgeX = std::fabs(hit.point.x - std::round(hit.point.x));
        const float edgeY = std::fabs(hit.point.y - std::round(hit.point.y));
        if (std::min(edgeX, edgeY) < 0.022f) colour = colour * 0.78f;
    }

    return Vec3(
        std::min(colour.x, 1.0f),
        std::min(colour.y, 1.0f),
        std::min(colour.z, 1.0f)
    );
}

Vec3 backgroundColour(float normalisedY) {
    const float t = std::max(0.0f, std::min(1.0f, normalisedY));
    const Vec3 top(0.075f, 0.12f, 0.18f);
    const Vec3 bottom(0.20f, 0.28f, 0.34f);
    return top * (1.0f - t) + bottom * t;
}

Vec3 traceSample(float pixelX, float pixelY) {
    const Vec3 forward = cameraForward();
    const Vec3 worldUp(0.0f, 0.0f, 1.0f);
    const Vec3 right = normalise(cross(forward, worldUp));
    const Vec3 up = normalise(cross(right, forward));

    const float aspect = static_cast<float>(FRAME_WIDTH) / static_cast<float>(FRAME_HEIGHT);
    const float viewHeight = 6.15f;
    const float viewWidth = viewHeight * aspect;
    const float screenX = ((pixelX / static_cast<float>(FRAME_WIDTH)) - 0.5f) * viewWidth;
    const float screenY = (0.5f - (pixelY / static_cast<float>(FRAME_HEIGHT))) * viewHeight;

    const Vec3 focus = BASE_FOCUS + Vec3(cameraPanX, cameraPanY, 0.0f);
    const Vec3 rayOrigin = focus - forward * 9.0f + right * screenX + up * screenY;
    const Ray ray{rayOrigin, forward};
    const Hit hit = traceClosest(ray, EPSILON, FAR_DISTANCE);

    if (hit.found) return shade(hit);
    return backgroundColour(pixelY / static_cast<float>(FRAME_HEIGHT));
}

std::uint8_t toByte(float value) {
    value = std::max(0.0f, std::min(1.0f, value));
    value = std::pow(value, 1.0f / 2.2f);
    return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
}

float distanceToSegment(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float denominator = abx * abx + aby * aby;
    const float t = denominator > 0.0f
        ? std::max(0.0f, std::min(1.0f, (apx * abx + apy * aby) / denominator))
        : 0.0f;
    const float dx = px - (ax + abx * t);
    const float dy = py - (ay + aby * t);
    return std::sqrt(dx * dx + dy * dy);
}

float clockwiseArrowDistance(float px, float py) {
    constexpr int ARC_SEGMENTS = 28;
    const float cx = 27.5f;
    const float cy = 25.0f;
    const float radiusX = 19.0f;
    const float radiusY = 12.0f;
    const float startAngle = PI * 0.89f;
    const float endAngle = PI * 0.10f;

    float minimum = 1000.0f;
    float previousX = cx + radiusX * std::cos(startAngle);
    float previousY = cy - radiusY * std::sin(startAngle);

    for (int index = 1; index <= ARC_SEGMENTS; ++index) {
        const float amount = static_cast<float>(index) / static_cast<float>(ARC_SEGMENTS);
        const float angle = startAngle + (endAngle - startAngle) * amount;
        const float nextX = cx + radiusX * std::cos(angle);
        const float nextY = cy - radiusY * std::sin(angle);
        minimum = std::min(minimum, distanceToSegment(px, py, previousX, previousY, nextX, nextY));
        previousX = nextX;
        previousY = nextY;
    }

    const float tipX = previousX;
    const float tipY = previousY;
    const float tangentX = std::sin(endAngle) * radiusX;
    const float tangentY = std::cos(endAngle) * radiusY;
    const float tangentLength = std::sqrt(tangentX * tangentX + tangentY * tangentY);
    const float directionX = tangentX / tangentLength;
    const float directionY = tangentY / tangentLength;
    const float normalX = -directionY;
    const float normalY = directionX;
    const float baseX = tipX - directionX * 9.0f;
    const float baseY = tipY - directionY * 9.0f;

    minimum = std::min(minimum, distanceToSegment(px, py, tipX, tipY, baseX + normalX * 5.5f, baseY + normalY * 5.5f));
    minimum = std::min(minimum, distanceToSegment(px, py, tipX, tipY, baseX - normalX * 5.5f, baseY - normalY * 5.5f));
    return minimum;
}

float directionalArrowDistance(float px, float py, float dx, float dy) {
    const float cx = PAN_ARROW_SIZE * 0.5f;
    const float cy = PAN_ARROW_SIZE * 0.5f;
    const float pxNormal = -dy;
    const float pyNormal = dx;

    const float tailX = cx - dx * 10.0f;
    const float tailY = cy - dy * 10.0f;
    const float neckX = cx + dx * 5.0f;
    const float neckY = cy + dy * 5.0f;
    const float tipX = cx + dx * 13.0f;
    const float tipY = cy + dy * 13.0f;

    float minimum = distanceToSegment(px, py, tailX, tailY, neckX, neckY);
    minimum = std::min(
        minimum,
        distanceToSegment(px, py, tipX, tipY, neckX + pxNormal * 6.0f, neckY + pyNormal * 6.0f)
    );
    minimum = std::min(
        minimum,
        distanceToSegment(px, py, tipX, tipY, neckX - pxNormal * 6.0f, neckY - pyNormal * 6.0f)
    );
    return minimum;
}

dsr::OrderedImageRgbaU8 frame;
dsr::OrderedImageRgbaU8 clockwiseArrowSprite;
dsr::OrderedImageRgbaU8 counterClockwiseArrowSprite;
dsr::OrderedImageRgbaU8 panUpSprite;
dsr::OrderedImageRgbaU8 panDownSprite;
dsr::OrderedImageRgbaU8 panLeftSprite;
dsr::OrderedImageRgbaU8 panRightSprite;
std::vector<std::uint8_t> rgba;

void writeSpritePixel(dsr::OrderedImageRgbaU8& sprite, int x, int y, float distance) {
    const float outerCoverage = std::max(0.0f, std::min(1.0f, 3.6f - distance));
    if (outerCoverage <= 0.0f) return;

    const float coreCoverage = std::max(0.0f, std::min(1.0f, 2.25f - distance));
    const int alpha = static_cast<int>(outerCoverage * 235.0f + 0.5f);
    const int shade = static_cast<int>(72.0f + coreCoverage * 178.0f + 0.5f);
    dsr::image_writePixel(sprite, x, y, dsr::ColorRgbaI32(shade, shade, shade, alpha));
}

void buildRotateArrowSprite(dsr::OrderedImageRgbaU8& sprite, bool mirrorHorizontally) {
    sprite = dsr::image_create_RgbaU8(ROTATE_ARROW_WIDTH, ROTATE_ARROW_HEIGHT, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));

    for (int y = 0; y < ROTATE_ARROW_HEIGHT; ++y) {
        for (int x = 0; x < ROTATE_ARROW_WIDTH; ++x) {
            const float sampleX = mirrorHorizontally
                ? static_cast<float>(ROTATE_ARROW_WIDTH - 1 - x) + 0.5f
                : static_cast<float>(x) + 0.5f;
            const float sampleY = static_cast<float>(y) + 0.5f;
            writeSpritePixel(sprite, x, y, clockwiseArrowDistance(sampleX, sampleY));
        }
    }
}

void buildPanArrowSprite(dsr::OrderedImageRgbaU8& sprite, float dx, float dy) {
    sprite = dsr::image_create_RgbaU8(PAN_ARROW_SIZE, PAN_ARROW_SIZE, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));

    for (int y = 0; y < PAN_ARROW_SIZE; ++y) {
        for (int x = 0; x < PAN_ARROW_SIZE; ++x) {
            const float sampleX = static_cast<float>(x) + 0.5f;
            const float sampleY = static_cast<float>(y) + 0.5f;
            writeSpritePixel(sprite, x, y, directionalArrowDistance(sampleX, sampleY, dx, dy));
        }
    }
}

void ensureFrame() {
    if (!dsr::image_exists(frame)) {
        frame = dsr::image_create_RgbaU8(FRAME_WIDTH, FRAME_HEIGHT, false);
    }
    if (!dsr::image_exists(clockwiseArrowSprite)) {
        buildRotateArrowSprite(clockwiseArrowSprite, false);
    }
    if (!dsr::image_exists(counterClockwiseArrowSprite)) {
        buildRotateArrowSprite(counterClockwiseArrowSprite, true);
    }
    if (!dsr::image_exists(panUpSprite)) {
        buildPanArrowSprite(panUpSprite, 0.0f, -1.0f);
        buildPanArrowSprite(panDownSprite, 0.0f, 1.0f);
        buildPanArrowSprite(panLeftSprite, -1.0f, 0.0f);
        buildPanArrowSprite(panRightSprite, 1.0f, 0.0f);
    }
    if (rgba.size() != static_cast<std::size_t>(FRAME_WIDTH * FRAME_HEIGHT * 4)) {
        rgba.resize(static_cast<std::size_t>(FRAME_WIDTH * FRAME_HEIGHT * 4));
    }
}

void renderScreenSpaceControls() {
    const int rotateTop = FRAME_HEIGHT - CONTROL_BOTTOM - ROTATE_ARROW_HEIGHT;
    const int counterClockwiseX = ROTATE_LEFT_X;
    const int clockwiseX = ROTATE_LEFT_X + ROTATE_ARROW_WIDTH + ROTATE_GAP;

    dsr::draw_alphaFilter(frame, counterClockwiseArrowSprite, counterClockwiseX, rotateTop);
    dsr::draw_alphaFilter(frame, clockwiseArrowSprite, clockwiseX, rotateTop);

    const int padSpan = PAN_ARROW_SIZE + PAN_PAD_STEP * 2;
    const int padLeft = FRAME_WIDTH - PAN_PAD_RIGHT - padSpan;
    const int padTop = FRAME_HEIGHT - PAN_PAD_BOTTOM - padSpan;

    dsr::draw_alphaFilter(frame, panUpSprite, padLeft + PAN_PAD_STEP, padTop);
    dsr::draw_alphaFilter(frame, panLeftSprite, padLeft, padTop + PAN_PAD_STEP);
    dsr::draw_alphaFilter(frame, panRightSprite, padLeft + PAN_PAD_STEP * 2, padTop + PAN_PAD_STEP);
    dsr::draw_alphaFilter(frame, panDownSprite, padLeft + PAN_PAD_STEP, padTop + PAN_PAD_STEP * 2);
}

void renderScene() {
    ensureFrame();

    const float offsets[2] = {0.25f, 0.75f};
    for (int y = 0; y < FRAME_HEIGHT; ++y) {
        for (int x = 0; x < FRAME_WIDTH; ++x) {
            Vec3 colour;
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    colour = colour + traceSample(
                        static_cast<float>(x) + offsets[sx],
                        static_cast<float>(y) + offsets[sy]
                    );
                }
            }
            colour = colour * 0.25f;

            dsr::image_writePixel(
                frame,
                x,
                y,
                dsr::ColorRgbaI32(toByte(colour.x), toByte(colour.y), toByte(colour.z), 255)
            );
        }
    }

    // Visible controls are engine-rendered sprites. DOM controls are transparent hitboxes only.
    renderScreenSpaceControls();

    for (int y = 0; y < FRAME_HEIGHT; ++y) {
        for (int x = 0; x < FRAME_WIDTH; ++x) {
            const dsr::ColorRgbaI32 colour = dsr::image_readPixel_border(frame, x, y);
            const std::size_t index = static_cast<std::size_t>((y * FRAME_WIDTH + x) * 4);
            rgba[index + 0] = static_cast<std::uint8_t>(colour.red);
            rgba[index + 1] = static_cast<std::uint8_t>(colour.green);
            rgba[index + 2] = static_cast<std::uint8_t>(colour.blue);
            rgba[index + 3] = 255;
        }
    }
}

void presentScene() {
    const std::uintptr_t pointer = reinterpret_cast<std::uintptr_t>(rgba.data());
    EM_ASM({
        const pointer = $0;
        const width = $1;
        const height = $2;
        const quarterTurns = $3;
        const panX = $4;
        const panY = $5;
        const canvas = document.getElementById('canvas');
        if (!canvas) return;

        canvas.width = width;
        canvas.height = height;
        const context = canvas.getContext('2d', { alpha: false });
        const imageData = context.createImageData(width, height);
        imageData.data.set(HEAPU8.subarray(pointer, pointer + width * height * 4));
        context.putImageData(imageData, 0, 0);

        document.documentElement.classList.add('wasm-ready');
        const loading = document.getElementById('loading');
        if (loading) loading.hidden = true;

        const status = document.getElementById('view-status');
        if (status) {
            status.textContent =
                'Camera ' + (quarterTurns * 90) + ' degrees around Z; pan X ' +
                panX.toFixed(2) + '; Y ' + panY.toFixed(2);
        }
    }, pointer, FRAME_WIDTH, FRAME_HEIGHT, cameraQuarterTurns, cameraPanX, cameraPanY);
}

void redraw() {
    renderScene();
    presentScene();
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_render() {
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_clockwise() {
    cameraQuarterTurns = (cameraQuarterTurns + 1) & 3;
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_counterclockwise() {
    cameraQuarterTurns = (cameraQuarterTurns + 3) & 3;
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_pan(float screenRight, float screenDown) {
    panCameraOnGround(screenRight, screenDown);
    redraw();
}
