#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <emscripten/emscripten.h>
#include "DFPSR/api/drawAPI.h"
#include "DFPSR/api/imageAPI.h"

namespace {

int frameWidth = 512;
int frameHeight = 288;
int allocatedFrameWidth = 0;
int allocatedFrameHeight = 0;

constexpr int ROTATE_ARROW_WIDTH = 56;
constexpr int ROTATE_ARROW_HEIGHT = 44;
constexpr int RESET_DISK_SIZE = 38;
constexpr int ROTATE_LEFT_X = 18;
constexpr int ROTATE_ROW_GAP = 8;
constexpr int ZOOM_CONTROL_SIZE = 32;
constexpr int LEFT_CONTROL_GAP = 8;
constexpr int CONTROL_BOTTOM = 18;

constexpr int PAN_ARROW_SIZE = 38;
constexpr int PAN_X_STEP = 48;
constexpr int PAN_Y_STEP = 36;
constexpr int PAN_PAD_RIGHT = 18;
constexpr int PAN_PAD_BOTTOM = 16;

constexpr float EPSILON = 0.0015f;
constexpr float FAR_DISTANCE = 1000.0f;
constexpr float PI = 3.14159265358979323846f;
constexpr float INV_SQRT_TWO = 0.7071067811865476f;
constexpr float GROUND_LIMIT = 4.40f;
constexpr float PAN_LIMIT = GROUND_LIMIT - 1.15f;
constexpr float BASE_VIEW_HEIGHT = 6.15f;
constexpr float MIN_VIEW_WIDTH = 5.50f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
};

float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
Vec3 normalise(const Vec3& v) {
    const float m = length(v);
    return m > 0.0f ? v / m : Vec3();
}
Vec3 rotateAroundZ(const Vec3& v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vec3(v.x * c - v.y * s, v.x * s + v.y * c, v.z);
}

struct Ray { Vec3 origin; Vec3 direction; };
enum class Surface { None, Ground, Cube, Sphere };
struct Hit {
    bool found = false;
    float t = FAR_DISTANCE;
    Vec3 point;
    Vec3 normal;
    Surface surface = Surface::None;
};

const Vec3 CUBE_MIN(-1.85f, -0.15f, 0.0f);
const Vec3 CUBE_MAX(-0.25f, 1.45f, 1.55f);
const Vec3 SPHERE_CENTRE(1.05f, -0.25f, 0.90f);
constexpr float SPHERE_RADIUS = 0.90f;
const Vec3 LIGHT_POSITION(-3.60f, -4.20f, 6.50f);
const Vec3 BASE_FOCUS(0.0f, 0.15f, 0.55f);

int cameraYawStep = 0;
float cameraPanX = 0.0f;
float cameraPanY = 0.0f;
bool detailedZoomMode = false;
int zoomPresetIndex = 3;

float yawRadians() { return -static_cast<float>(cameraYawStep) * PI * 0.25f; }
Vec3 cameraForward() { return rotateAroundZ(normalise(Vec3(1.0f, 1.0f, -1.0f)), yawRadians()); }
Vec3 cameraGroundRight() { return normalise(cross(cameraForward(), Vec3(0.0f, 0.0f, 1.0f))); }
Vec3 cameraGroundDown() {
    const Vec3 f = cameraForward();
    return normalise(Vec3(f.x, f.y, 0.0f));
}

int minimumZoomPresetIndex() { return detailedZoomMode ? 0 : 2; }
int maximumZoomPresetIndex() { return detailedZoomMode ? 5 : 4; }
bool wholeWorldZoom() { return zoomPresetIndex == 0; }
float currentZoomScale() {
    switch (zoomPresetIndex) {
        case 5: return 4.0f;
        case 4: return 2.0f;
        case 3: return 1.0f;
        case 2: return 0.5f;
        case 1: return 0.25f;
        default: return 0.25f;
    }
}

void panCameraOnGround(float screenRight, float screenDown) {
    if (wholeWorldZoom()) return;
    const Vec3 delta = cameraGroundRight() * screenRight + cameraGroundDown() * screenDown;
    cameraPanX = std::max(-PAN_LIMIT, std::min(PAN_LIMIT, cameraPanX + delta.x));
    cameraPanY = std::max(-PAN_LIMIT, std::min(PAN_LIMIT, cameraPanY + delta.y));
}

float wholeViewHeightWorld(float aspect) {
    const Vec3 forward = cameraForward();
    const Vec3 right = normalise(cross(forward, Vec3(0.0f, 0.0f, 1.0f)));
    const Vec3 up = normalise(cross(right, forward));
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;

    auto include = [&](const Vec3& p) {
        const Vec3 rel = p - BASE_FOCUS;
        halfWidth = std::max(halfWidth, std::fabs(dot(rel, right)));
        halfHeight = std::max(halfHeight, std::fabs(dot(rel, up)));
    };

    include(Vec3(-GROUND_LIMIT, -GROUND_LIMIT, 0.0f));
    include(Vec3(GROUND_LIMIT, -GROUND_LIMIT, 0.0f));
    include(Vec3(-GROUND_LIMIT, GROUND_LIMIT, 0.0f));
    include(Vec3(GROUND_LIMIT, GROUND_LIMIT, 0.0f));

    for (int ix = 0; ix < 2; ++ix) {
        for (int iy = 0; iy < 2; ++iy) {
            for (int iz = 0; iz < 2; ++iz) {
                include(Vec3(
                    ix ? CUBE_MAX.x : CUBE_MIN.x,
                    iy ? CUBE_MAX.y : CUBE_MIN.y,
                    iz ? CUBE_MAX.z : CUBE_MIN.z
                ));
            }
        }
    }

    include(SPHERE_CENTRE + Vec3(SPHERE_RADIUS, 0.0f, 0.0f));
    include(SPHERE_CENTRE + Vec3(-SPHERE_RADIUS, 0.0f, 0.0f));
    include(SPHERE_CENTRE + Vec3(0.0f, SPHERE_RADIUS, 0.0f));
    include(SPHERE_CENTRE + Vec3(0.0f, -SPHERE_RADIUS, 0.0f));
    include(SPHERE_CENTRE + Vec3(0.0f, 0.0f, SPHERE_RADIUS));
    include(SPHERE_CENTRE + Vec3(0.0f, 0.0f, -SPHERE_RADIUS));

    const float width = halfWidth * 2.04f;
    const float height = halfHeight * 2.04f;
    return std::max(height, width / aspect);
}

float viewHeightWorld() {
    const float aspect = static_cast<float>(frameWidth) / static_cast<float>(frameHeight);
    if (wholeWorldZoom()) return wholeViewHeightWorld(aspect);
    return std::max(BASE_VIEW_HEIGHT, MIN_VIEW_WIDTH / aspect) / currentZoomScale();
}

void stepZoom(int direction) {
    zoomPresetIndex = std::max(
        minimumZoomPresetIndex(),
        std::min(maximumZoomPresetIndex(), zoomPresetIndex + direction)
    );
}

bool intersectSphere(const Ray& ray, float tMin, float tMax, Hit& hit) {
    const Vec3 oc = ray.origin - SPHERE_CENTRE;
    const float a = dot(ray.direction, ray.direction);
    const float halfB = dot(oc, ray.direction);
    const float c = dot(oc, oc) - SPHERE_RADIUS * SPHERE_RADIUS;
    const float d = halfB * halfB - a * c;
    if (d < 0.0f) return false;

    const float rootPart = std::sqrt(d);
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
    const float o[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float d[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float mn[3] = {CUBE_MIN.x, CUBE_MIN.y, CUBE_MIN.z};
    const float mx[3] = {CUBE_MAX.x, CUBE_MAX.y, CUBE_MAX.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(d[axis]) < 1.0e-7f) {
            if (o[axis] < mn[axis] || o[axis] > mx[axis]) return false;
            continue;
        }
        const float inv = 1.0f / d[axis];
        float t0 = (mn[axis] - o[axis]) * inv;
        float t1 = (mx[axis] - o[axis]) * inv;
        if (t0 > t1) std::swap(t0, t1);
        nearT = std::max(nearT, t0);
        farT = std::min(farT, t1);
        if (farT < nearT) return false;
    }

    hit.found = true;
    hit.t = nearT;
    hit.point = ray.origin + ray.direction * nearT;
    hit.surface = Surface::Cube;

    float closest = std::fabs(hit.point.x - CUBE_MIN.x);
    hit.normal = Vec3(-1.0f, 0.0f, 0.0f);
    auto choose = [&](float distance, const Vec3& normal) {
        if (distance < closest) { closest = distance; hit.normal = normal; }
    };
    choose(std::fabs(hit.point.x - CUBE_MAX.x), Vec3(1.0f, 0.0f, 0.0f));
    choose(std::fabs(hit.point.y - CUBE_MIN.y), Vec3(0.0f, -1.0f, 0.0f));
    choose(std::fabs(hit.point.y - CUBE_MAX.y), Vec3(0.0f, 1.0f, 0.0f));
    choose(std::fabs(hit.point.z - CUBE_MIN.z), Vec3(0.0f, 0.0f, -1.0f));
    choose(std::fabs(hit.point.z - CUBE_MAX.z), Vec3(0.0f, 0.0f, 1.0f));
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
    if (intersectCube(ray, tMin, tMax, candidate)) { closest = candidate; tMax = candidate.t; }
    candidate = Hit();
    if (intersectSphere(ray, tMin, tMax, candidate)) { closest = candidate; tMax = candidate.t; }
    candidate = Hit();
    if (intersectGround(ray, tMin, tMax, candidate)) closest = candidate;
    return closest;
}

bool isOccluded(const Vec3& point, const Vec3& normal) {
    const Vec3 toLight = LIGHT_POSITION - point;
    const float distance = length(toLight);
    const Ray shadowRay{point + normal * EPSILON, toLight / distance};
    return traceClosest(shadowRay, EPSILON, distance - EPSILON).found;
}

Vec3 materialColour(const Hit& hit) {
    if (hit.surface == Surface::Cube) return Vec3(0.18f, 0.48f, 0.88f);
    if (hit.surface == Surface::Sphere) return Vec3(0.95f, 0.43f, 0.12f);
    const int tx = static_cast<int>(std::floor(hit.point.x + 20.0f));
    const int ty = static_cast<int>(std::floor(hit.point.y + 20.0f));
    const float base = ((tx + ty) & 1) ? 0.63f : 0.69f;
    return Vec3(base * 0.90f, base * 0.96f, base);
}

Vec3 shade(const Hit& hit) {
    const Vec3 base = materialColour(hit);
    const Vec3 toLight = LIGHT_POSITION - hit.point;
    const float distance = length(toLight);
    const Vec3 lightDirection = toLight / distance;
    const float diffuse = std::max(0.0f, dot(hit.normal, lightDirection));
    const float attenuation = 1.0f / (1.0f + 0.018f * distance * distance);
    const float visibility = isOccluded(hit.point, hit.normal) ? 0.0f : 1.0f;
    Vec3 colour = base * (0.19f + visibility * diffuse * attenuation * 1.18f);

    if (hit.surface == Surface::Ground) {
        const float ex = std::fabs(hit.point.x - std::round(hit.point.x));
        const float ey = std::fabs(hit.point.y - std::round(hit.point.y));
        if (std::min(ex, ey) < 0.022f) colour = colour * 0.78f;
    }
    return Vec3(std::min(colour.x, 1.0f), std::min(colour.y, 1.0f), std::min(colour.z, 1.0f));
}

Vec3 backgroundColour(float y) {
    const float t = std::max(0.0f, std::min(1.0f, y));
    const Vec3 top(0.075f, 0.12f, 0.18f);
    const Vec3 bottom(0.20f, 0.28f, 0.34f);
    return top * (1.0f - t) + bottom * t;
}

Vec3 traceSample(float pixelX, float pixelY) {
    const Vec3 forward = cameraForward();
    const Vec3 right = normalise(cross(forward, Vec3(0.0f, 0.0f, 1.0f)));
    const Vec3 up = normalise(cross(right, forward));
    const float aspect = static_cast<float>(frameWidth) / static_cast<float>(frameHeight);
    const float viewHeight = viewHeightWorld();
    const float viewWidth = viewHeight * aspect;
    const float screenX = (pixelX / static_cast<float>(frameWidth) - 0.5f) * viewWidth;
    const float screenY = (0.5f - pixelY / static_cast<float>(frameHeight)) * viewHeight;
    const Vec3 focus = wholeWorldZoom() ? BASE_FOCUS : BASE_FOCUS + Vec3(cameraPanX, cameraPanY, 0.0f);
    const Ray ray{focus - forward * 9.0f + right * screenX + up * screenY, forward};
    const Hit hit = traceClosest(ray, EPSILON, FAR_DISTANCE);
    return hit.found ? shade(hit) : backgroundColour(pixelY / static_cast<float>(frameHeight));
}

std::uint8_t toByte(float value) {
    value = std::pow(std::max(0.0f, std::min(1.0f, value)), 1.0f / 2.2f);
    return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
}

float distanceToSegment(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float denominator = abx * abx + aby * aby;
    const float t = denominator > 0.0f
        ? std::max(0.0f, std::min(1.0f, ((px - ax) * abx + (py - ay) * aby) / denominator))
        : 0.0f;
    const float dx = px - (ax + abx * t);
    const float dy = py - (ay + aby * t);
    return std::sqrt(dx * dx + dy * dy);
}

float curvedArrowDistance(float px, float py) {
    constexpr int segments = 28;
    const float cx = 27.5f;
    const float cy = 25.0f;
    const float rx = 19.0f;
    const float ry = 12.0f;
    const float start = PI * 0.89f;
    const float end = PI * 0.10f;
    float minimum = 1000.0f;
    float previousX = cx + rx * std::cos(start);
    float previousY = cy - ry * std::sin(start);

    for (int i = 1; i <= segments; ++i) {
        const float amount = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = start + (end - start) * amount;
        const float nextX = cx + rx * std::cos(angle);
        const float nextY = cy - ry * std::sin(angle);
        minimum = std::min(minimum, distanceToSegment(px, py, previousX, previousY, nextX, nextY));
        previousX = nextX;
        previousY = nextY;
    }

    const float tangentX = std::sin(end) * rx;
    const float tangentY = std::cos(end) * ry;
    const float tangentLength = std::sqrt(tangentX * tangentX + tangentY * tangentY);
    const float dx = tangentX / tangentLength;
    const float dy = tangentY / tangentLength;
    const float nx = -dy;
    const float ny = dx;
    const float baseX = previousX - dx * 9.0f;
    const float baseY = previousY - dy * 9.0f;
    minimum = std::min(minimum, distanceToSegment(px, py, previousX, previousY, baseX + nx * 5.5f, baseY + ny * 5.5f));
    minimum = std::min(minimum, distanceToSegment(px, py, previousX, previousY, baseX - nx * 5.5f, baseY - ny * 5.5f));
    return minimum;
}

float directionalArrowDistance(float px, float py, float dx, float dy) {
    const float c = PAN_ARROW_SIZE * 0.5f;
    const float nx = -dy;
    const float ny = dx;
    const float tailX = c - dx * 10.0f;
    const float tailY = c - dy * 10.0f;
    const float neckX = c + dx * 5.0f;
    const float neckY = c + dy * 5.0f;
    const float tipX = c + dx * 13.0f;
    const float tipY = c + dy * 13.0f;
    float minimum = distanceToSegment(px, py, tailX, tailY, neckX, neckY);
    minimum = std::min(minimum, distanceToSegment(px, py, tipX, tipY, neckX + nx * 6.0f, neckY + ny * 6.0f));
    minimum = std::min(minimum, distanceToSegment(px, py, tipX, tipY, neckX - nx * 6.0f, neckY - ny * 6.0f));
    return minimum;
}

float zoomGlyphDistance(float px, float py, bool plus) {
    const float c = ZOOM_CONTROL_SIZE * 0.5f;
    const float arm = 8.5f;
    float minimum = distanceToSegment(px, py, c - arm, c, c + arm, c);
    if (plus) minimum = std::min(minimum, distanceToSegment(px, py, c, c - arm, c, c + arm));
    return minimum;
}

dsr::OrderedImageRgbaU8 frame;
dsr::OrderedImageRgbaU8 clockwiseArrowSprite;
dsr::OrderedImageRgbaU8 counterClockwiseArrowSprite;
dsr::OrderedImageRgbaU8 panUpSprite;
dsr::OrderedImageRgbaU8 panDownSprite;
dsr::OrderedImageRgbaU8 panLeftSprite;
dsr::OrderedImageRgbaU8 panRightSprite;
dsr::OrderedImageRgbaU8 resetDiskSprite;
dsr::OrderedImageRgbaU8 zoomPlusSprite;
dsr::OrderedImageRgbaU8 zoomMinusSprite;
std::vector<std::uint8_t> rgba;

void writeSpritePixel(dsr::OrderedImageRgbaU8& sprite, int x, int y, float distance) {
    const float outer = std::max(0.0f, std::min(1.0f, 3.6f - distance));
    if (outer <= 0.0f) return;
    const float core = std::max(0.0f, std::min(1.0f, 2.25f - distance));
    const int alpha = static_cast<int>(outer * 235.0f + 0.5f);
    const int shade = static_cast<int>(72.0f + core * 178.0f + 0.5f);
    dsr::image_writePixel(sprite, x, y, dsr::ColorRgbaI32(shade, shade, shade, alpha));
}

void buildRotateArrowSprite(dsr::OrderedImageRgbaU8& sprite, bool mirror) {
    sprite = dsr::image_create_RgbaU8(ROTATE_ARROW_WIDTH, ROTATE_ARROW_HEIGHT, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));
    for (int y = 0; y < ROTATE_ARROW_HEIGHT; ++y) {
        for (int x = 0; x < ROTATE_ARROW_WIDTH; ++x) {
            const float sx = mirror ? static_cast<float>(ROTATE_ARROW_WIDTH - 1 - x) + 0.5f : static_cast<float>(x) + 0.5f;
            writeSpritePixel(sprite, x, y, curvedArrowDistance(sx, static_cast<float>(y) + 0.5f));
        }
    }
}

void buildPanArrowSprite(dsr::OrderedImageRgbaU8& sprite, float dx, float dy) {
    sprite = dsr::image_create_RgbaU8(PAN_ARROW_SIZE, PAN_ARROW_SIZE, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));
    for (int y = 0; y < PAN_ARROW_SIZE; ++y) {
        for (int x = 0; x < PAN_ARROW_SIZE; ++x) {
            writeSpritePixel(sprite, x, y, directionalArrowDistance(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, dx, dy));
        }
    }
}

void buildResetDiskSprite() {
    resetDiskSprite = dsr::image_create_RgbaU8(RESET_DISK_SIZE, RESET_DISK_SIZE, true);
    dsr::image_fill(resetDiskSprite, dsr::ColorRgbaI32(0, 0, 0, 0));
    const float c = RESET_DISK_SIZE * 0.5f;
    for (int y = 0; y < RESET_DISK_SIZE; ++y) {
        for (int x = 0; x < RESET_DISK_SIZE; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - c;
            const float dy = static_cast<float>(y) + 0.5f - c;
            const float radius = std::sqrt(dx * dx + dy * dy);
            if (radius <= 12.5f) {
                const float edge = std::max(0.0f, std::min(1.0f, 13.5f - radius));
                const int shade = radius < 9.0f ? 198 : 232;
                dsr::image_writePixel(resetDiskSprite, x, y, dsr::ColorRgbaI32(shade, shade, shade, static_cast<int>(edge * 225.0f + 0.5f)));
            }
        }
    }
}

void buildZoomGlyphSprite(dsr::OrderedImageRgbaU8& sprite, bool plus) {
    sprite = dsr::image_create_RgbaU8(ZOOM_CONTROL_SIZE, ZOOM_CONTROL_SIZE, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));
    for (int y = 0; y < ZOOM_CONTROL_SIZE; ++y) {
        for (int x = 0; x < ZOOM_CONTROL_SIZE; ++x) {
            writeSpritePixel(sprite, x, y, zoomGlyphDistance(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, plus));
        }
    }
}

void ensureFrame() {
    if (!dsr::image_exists(frame) || allocatedFrameWidth != frameWidth || allocatedFrameHeight != frameHeight) {
        frame = dsr::image_create_RgbaU8(frameWidth, frameHeight, false);
        allocatedFrameWidth = frameWidth;
        allocatedFrameHeight = frameHeight;
    }
    if (!dsr::image_exists(clockwiseArrowSprite)) {
        buildRotateArrowSprite(clockwiseArrowSprite, false);
        buildRotateArrowSprite(counterClockwiseArrowSprite, true);
    }
    if (!dsr::image_exists(panUpSprite)) {
        buildPanArrowSprite(panUpSprite, INV_SQRT_TWO, -INV_SQRT_TWO);
        buildPanArrowSprite(panDownSprite, -INV_SQRT_TWO, INV_SQRT_TWO);
        buildPanArrowSprite(panLeftSprite, -1.0f, 0.0f);
        buildPanArrowSprite(panRightSprite, 1.0f, 0.0f);
    }
    if (!dsr::image_exists(resetDiskSprite)) buildResetDiskSprite();
    if (!dsr::image_exists(zoomPlusSprite)) {
        buildZoomGlyphSprite(zoomPlusSprite, true);
        buildZoomGlyphSprite(zoomMinusSprite, false);
    }
    const std::size_t required = static_cast<std::size_t>(frameWidth) * static_cast<std::size_t>(frameHeight) * 4;
    if (rgba.size() != required) rgba.resize(required);
}

void renderScreenSpaceControls() {
    const int yawGroupWidth = ROTATE_ARROW_WIDTH + ROTATE_ROW_GAP + RESET_DISK_SIZE + ROTATE_ROW_GAP + ROTATE_ARROW_WIDTH;
    const int yawRowHeight = std::max(ROTATE_ARROW_HEIGHT, RESET_DISK_SIZE);
    const int zoomOutTop = frameHeight - CONTROL_BOTTOM - ZOOM_CONTROL_SIZE;
    const int yawRowTop = zoomOutTop - LEFT_CONTROL_GAP - yawRowHeight;
    const int zoomInTop = yawRowTop - LEFT_CONTROL_GAP - ZOOM_CONTROL_SIZE;
    const int ccwX = ROTATE_LEFT_X;
    const int resetYawX = ccwX + ROTATE_ARROW_WIDTH + ROTATE_ROW_GAP;
    const int cwX = resetYawX + RESET_DISK_SIZE + ROTATE_ROW_GAP;
    const int arrowTop = yawRowTop + (yawRowHeight - ROTATE_ARROW_HEIGHT) / 2;
    const int resetYawTop = yawRowTop + (yawRowHeight - RESET_DISK_SIZE) / 2;
    const int zoomX = ROTATE_LEFT_X + (yawGroupWidth - ZOOM_CONTROL_SIZE) / 2;

    dsr::draw_alphaFilter(frame, zoomPlusSprite, zoomX, zoomInTop);
    dsr::draw_alphaFilter(frame, counterClockwiseArrowSprite, ccwX, arrowTop);
    dsr::draw_alphaFilter(frame, resetDiskSprite, resetYawX, resetYawTop);
    dsr::draw_alphaFilter(frame, clockwiseArrowSprite, cwX, arrowTop);
    dsr::draw_alphaFilter(frame, zoomMinusSprite, zoomX, zoomOutTop);

    const int centreX = frameWidth - PAN_PAD_RIGHT - PAN_ARROW_SIZE - PAN_X_STEP;
    const int centreY = frameHeight - PAN_PAD_BOTTOM - PAN_ARROW_SIZE - PAN_Y_STEP;
    dsr::draw_alphaFilter(frame, panLeftSprite, centreX - PAN_X_STEP, centreY);
    dsr::draw_alphaFilter(frame, resetDiskSprite, centreX, centreY);
    dsr::draw_alphaFilter(frame, panRightSprite, centreX + PAN_X_STEP, centreY);
    dsr::draw_alphaFilter(frame, panUpSprite, centreX + PAN_Y_STEP, centreY - PAN_Y_STEP);
    dsr::draw_alphaFilter(frame, panDownSprite, centreX - PAN_Y_STEP, centreY + PAN_Y_STEP);
}

void renderScene() {
    ensureFrame();
    const float offsets[2] = {0.25f, 0.75f};
    for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            Vec3 colour;
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    colour = colour + traceSample(static_cast<float>(x) + offsets[sx], static_cast<float>(y) + offsets[sy]);
                }
            }
            colour = colour * 0.25f;
            dsr::image_writePixel(frame, x, y, dsr::ColorRgbaI32(toByte(colour.x), toByte(colour.y), toByte(colour.z), 255));
        }
    }

    renderScreenSpaceControls();

    for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            const dsr::ColorRgbaI32 colour = dsr::image_readPixel_border(frame, x, y);
            const std::size_t index = static_cast<std::size_t>((y * frameWidth + x) * 4);
            rgba[index] = static_cast<std::uint8_t>(colour.red);
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
        const yawDegrees = $3;
        const panX = $4;
        const panY = $5;
        const zoomPreset = $6;
        const detailed = !!$7;
        const canvas = document.getElementById('canvas');
        if (!canvas) return;
        if (canvas.width !== width) canvas.width = width;
        if (canvas.height !== height) canvas.height = height;
        const context = canvas.getContext('2d', { alpha: false });
        const imageData = context.createImageData(width, height);
        imageData.data.set(HEAPU8.subarray(pointer, pointer + width * height * 4));
        context.putImageData(imageData, 0, 0);
        document.documentElement.classList.add('wasm-ready');
        const loading = document.getElementById('loading');
        if (loading) loading.hidden = true;
        let zoomLabel = 'default';
        if (zoomPreset === 0) zoomLabel = 'whole';
        else if (zoomPreset === 1) zoomLabel = 'super far';
        else if (zoomPreset === 2) zoomLabel = 'far';
        else if (zoomPreset === 4) zoomLabel = 'close';
        else if (zoomPreset === 5) zoomLabel = 'super close';
        const status = document.getElementById('view-status');
        if (status) {
            status.textContent = 'Camera ' + yawDegrees + ' degrees around Z; pan X ' + panX.toFixed(2) + '; Y ' + panY.toFixed(2) + '; zoom ' + zoomLabel + (detailed ? ' detailed' : ' regular');
        }
    }, static_cast<int>(pointer), frameWidth, frameHeight, cameraYawStep * 45, cameraPanX, cameraPanY, zoomPresetIndex, detailedZoomMode ? 1 : 0);
}

void redraw() { renderScene(); presentScene(); }

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_render() { redraw(); }
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_resize(int width, int height) {
    frameWidth = std::max(160, std::min(1600, width));
    frameHeight = std::max(160, std::min(1600, height));
    redraw();
}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_clockwise() { cameraYawStep = (cameraYawStep + 1) & 7; redraw(); }
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_counterclockwise() { cameraYawStep = (cameraYawStep + 7) & 7; redraw(); }
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_yaw() { cameraYawStep = 0; redraw(); }
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_in() { stepZoom(1); redraw(); }
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_out() { stepZoom(-1); redraw(); }
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_detailed_mode(int enabled) {
    detailedZoomMode = enabled != 0;
    zoomPresetIndex = std::max(minimumZoomPresetIndex(), std::min(maximumZoomPresetIndex(), zoomPresetIndex));
    redraw();
}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_pan(float screenRight, float screenDown) { panCameraOnGround(screenRight, screenDown); redraw(); }
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_camera() { cameraPanX = 0.0f; cameraPanY = 0.0f; redraw(); }
