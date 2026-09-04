#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <emscripten/emscripten.h>
#include "DFPSR/api/imageAPI.h"

namespace {

constexpr int FRAME_WIDTH = 512;
constexpr int FRAME_HEIGHT = 288;
constexpr float EPSILON = 0.0015f;
constexpr float FAR_DISTANCE = 1000.0f;

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

const Vec3 CUBE_MIN(-1.85f, 0.0f, -0.15f);
const Vec3 CUBE_MAX(-0.25f, 1.55f, 1.45f);
const Vec3 SPHERE_CENTRE(1.05f, 0.90f, -0.25f);
constexpr float SPHERE_RADIUS = 0.90f;
constexpr float GROUND_LIMIT = 4.40f;
const Vec3 LIGHT_POSITION(-3.60f, 6.50f, -4.20f);

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
    if (std::fabs(ray.direction.y) < 1.0e-7f) return false;
    const float t = -ray.origin.y / ray.direction.y;
    if (t < tMin || t > tMax) return false;

    const Vec3 point = ray.origin + ray.direction * t;
    if (std::fabs(point.x) > GROUND_LIMIT || std::fabs(point.z) > GROUND_LIMIT) return false;

    hit.found = true;
    hit.t = t;
    hit.point = point;
    hit.normal = Vec3(0.0f, 1.0f, 0.0f);
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
    const int tileZ = static_cast<int>(std::floor(hit.point.z + 20.0f));
    const bool alternate = ((tileX + tileZ) & 1) != 0;
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
        const float edgeZ = std::fabs(hit.point.z - std::round(hit.point.z));
        if (std::min(edgeX, edgeZ) < 0.022f) colour = colour * 0.78f;
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
    const Vec3 forward = normalise(Vec3(1.0f, -1.0f, 1.0f));
    const Vec3 right = normalise(cross(forward, Vec3(0.0f, 1.0f, 0.0f)));
    const Vec3 up = normalise(cross(right, forward));

    const float aspect = static_cast<float>(FRAME_WIDTH) / static_cast<float>(FRAME_HEIGHT);
    const float viewHeight = 6.15f;
    const float viewWidth = viewHeight * aspect;
    const float screenX = ((pixelX / static_cast<float>(FRAME_WIDTH)) - 0.5f) * viewWidth;
    const float screenY = (0.5f - (pixelY / static_cast<float>(FRAME_HEIGHT))) * viewHeight;

    const Vec3 focus(0.0f, 0.55f, 0.15f);
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

dsr::OrderedImageRgbaU8 frame;
std::vector<std::uint8_t> rgba;

void ensureFrame() {
    if (!dsr::image_exists(frame)) {
        frame = dsr::image_create_RgbaU8(FRAME_WIDTH, FRAME_HEIGHT, false);
    }
    if (rgba.size() != static_cast<std::size_t>(FRAME_WIDTH * FRAME_HEIGHT * 4)) {
        rgba.resize(static_cast<std::size_t>(FRAME_WIDTH * FRAME_HEIGHT * 4));
    }
}

void renderScene() {
    ensureFrame();

    const float offsets[2] = {0.25f, 0.75f};
    for (int y = 0; y < FRAME_HEIGHT; ++y) {
        for (int x = 0; x < FRAME_WIDTH; ++x) {
            Vec3 colour;
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    colour = colour + traceSample(static_cast<float>(x) + offsets[sx], static_cast<float>(y) + offsets[sy]);
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
    }, pointer, FRAME_WIDTH, FRAME_HEIGHT);
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_render() {
    renderScene();
    presentScene();
}
