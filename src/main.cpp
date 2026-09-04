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
constexpr int CONTROL_BOTTOM = 18;

constexpr int ZOOM_CONTROL_SIZE = 32;
constexpr int ZOOM_LEFT = 18;
constexpr int ZOOM_TOP = 18;
constexpr int ZOOM_GAP = 6;

constexpr int PAN_ARROW_SIZE = 38;
constexpr int PAN_X_STEP = 48;
constexpr int PAN_Y_STEP = 36;
constexpr int PAN_PAD_RIGHT = 18;
constexpr int PAN_PAD_BOTTOM = 16;
constexpr float PAN_BUTTON_STEP = 0.34f;

constexpr int LEVEL_RIGHT = 18;
constexpr int LEVEL_TOP = 18;
constexpr int LEVEL_GAP = 6;

constexpr float EPSILON = 0.0015f;
constexpr float FAR_DISTANCE = 1000.0f;
constexpr float PI = 3.14159265358979323846f;
constexpr float GROUND_LIMIT = 4.40f;
constexpr float PAN_LIMIT = GROUND_LIMIT - 1.15f;
constexpr float BASE_VIEW_HEIGHT = 6.15f;
constexpr float MIN_VIEW_WIDTH = 5.50f;
constexpr int DEFAULT_LEVEL_INDEX = 1;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float scale) const { return Vec3(x * scale, y * scale, z * scale); }
    Vec3 operator/(float scale) const { return Vec3(x / scale, y / scale, z / scale); }
};

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

Vec3 rotateZ(const Vec3& value, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vec3(value.x * c - value.y * s, value.x * s + value.y * c, value.z);
}

Vec3 blend(const Vec3& under, const Vec3& over, float alpha) {
    return under * (1.0f - alpha) + over * alpha;
}

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

enum class ShapeKind {
    Cube,
    Sphere,
    Cone,
    Pyramid,
    Dodecahedron,
    Icosahedron
};

struct RenderObject {
    ShapeKind kind;
    Vec3 position;
    float size;
    Vec3 colour;
};

struct LevelView {
    std::vector<RenderObject> objects;
    Vec3 lightPosition;
    Vec3 floorDark;
    Vec3 floorLight;
};

struct Hit {
    bool found = false;
    float t = FAR_DISTANCE;
    Vec3 point;
    Vec3 normal;
    Vec3 colour;
    bool ground = false;
};

const Vec3 BASE_FOCUS(0.0f, 0.15f, 0.55f);

std::vector<LevelView> makeDemoLevels() {
    std::vector<LevelView> levels;

    LevelView lower;
    lower.lightPosition = Vec3(4.20f, -3.20f, 5.60f);
    lower.floorDark = Vec3(0.34f, 0.34f, 0.36f);
    lower.floorLight = Vec3(0.40f, 0.40f, 0.42f);
    lower.objects.push_back({ShapeKind::Cone, Vec3(-1.30f, -0.80f, 0.0f), 0.86f, Vec3(0.62f, 0.25f, 0.82f)});
    lower.objects.push_back({ShapeKind::Pyramid, Vec3(1.20f, 0.85f, 0.0f), 0.92f, Vec3(0.96f, 0.78f, 0.16f)});
    levels.push_back(lower);

    LevelView middle;
    middle.lightPosition = Vec3(-3.60f, -4.20f, 6.50f);
    middle.floorDark = Vec3(0.567f, 0.605f, 0.630f);
    middle.floorLight = Vec3(0.621f, 0.662f, 0.690f);
    middle.objects.push_back({ShapeKind::Cube, Vec3(-1.05f, 0.65f, 0.80f), 0.80f, Vec3(0.18f, 0.48f, 0.88f)});
    middle.objects.push_back({ShapeKind::Sphere, Vec3(1.05f, -0.25f, 0.90f), 0.90f, Vec3(0.95f, 0.43f, 0.12f)});
    levels.push_back(middle);

    LevelView upper;
    upper.lightPosition = Vec3(3.80f, 4.40f, 7.20f);
    upper.floorDark = Vec3(0.74f, 0.74f, 0.76f);
    upper.floorLight = Vec3(0.82f, 0.82f, 0.84f);
    upper.objects.push_back({ShapeKind::Dodecahedron, Vec3(-1.35f, 0.95f, 1.00f), 0.72f, Vec3(0.18f, 0.50f, 0.94f)});
    upper.objects.push_back({ShapeKind::Icosahedron, Vec3(1.30f, -0.95f, 1.10f), 0.78f, Vec3(0.90f, 0.16f, 0.14f)});
    levels.push_back(upper);

    return levels;
}

std::vector<LevelView> levels = makeDemoLevels();
int selectedLevel = DEFAULT_LEVEL_INDEX;
int cameraYawStep = 0;
float cameraPanX = 0.0f;
float cameraPanY = 0.0f;
bool detailedZoomMode = false;
int zoomPreset = 3;

float yawRadians() {
    return -static_cast<float>(cameraYawStep) * PI * 0.25f;
}

Vec3 cameraForward() {
    return rotateZ(normalise(Vec3(1.0f, 1.0f, -1.0f)), yawRadians());
}

Vec3 cameraGroundRight() {
    return normalise(cross(cameraForward(), Vec3(0.0f, 0.0f, 1.0f)));
}

Vec3 cameraGroundDown() {
    const Vec3 forward = cameraForward();
    return normalise(Vec3(forward.x, forward.y, 0.0f));
}

bool wholeZoom() {
    return zoomPreset == 0;
}

float zoomScale() {
    switch (zoomPreset) {
        case 5: return 4.0f;
        case 4: return 2.0f;
        case 3: return 1.0f;
        case 2: return 0.5f;
        case 1: return 0.25f;
        default: return 0.25f;
    }
}

float objectBoundRadius(const RenderObject& object) {
    switch (object.kind) {
        case ShapeKind::Cone: return object.size * 1.85f;
        case ShapeKind::Pyramid: return object.size * 1.80f;
        default: return object.size * 1.55f;
    }
}

float wholeViewHeight(float aspect) {
    const Vec3 forward = cameraForward();
    const Vec3 right = normalise(cross(forward, Vec3(0.0f, 0.0f, 1.0f)));
    const Vec3 up = normalise(cross(right, forward));
    float minRight = 1.0e9f;
    float maxRight = -1.0e9f;
    float minUp = 1.0e9f;
    float maxUp = -1.0e9f;

    auto include = [&](const Vec3& point) {
        const Vec3 relative = point - BASE_FOCUS;
        const float projectedRight = dot(relative, right);
        const float projectedUp = dot(relative, up);
        minRight = std::min(minRight, projectedRight);
        maxRight = std::max(maxRight, projectedRight);
        minUp = std::min(minUp, projectedUp);
        maxUp = std::max(maxUp, projectedUp);
    };

    include(Vec3(-GROUND_LIMIT, -GROUND_LIMIT, 0.0f));
    include(Vec3(GROUND_LIMIT, -GROUND_LIMIT, 0.0f));
    include(Vec3(-GROUND_LIMIT, GROUND_LIMIT, 0.0f));
    include(Vec3(GROUND_LIMIT, GROUND_LIMIT, 0.0f));

    for (const LevelView& level : levels) {
        for (const RenderObject& object : level.objects) {
            const float radius = objectBoundRadius(object);
            for (int sx = -1; sx <= 1; sx += 2) {
                for (int sy = -1; sy <= 1; sy += 2) {
                    for (int sz = -1; sz <= 1; sz += 2) {
                        include(object.position + Vec3(radius * sx, radius * sy, radius * sz));
                    }
                }
            }
        }
    }

    const float width = (maxRight - minRight) * 1.04f;
    const float height = (maxUp - minUp) * 1.04f;
    return std::max(height, width / aspect);
}

float baseViewHeight(float aspect) {
    return std::max(BASE_VIEW_HEIGHT, MIN_VIEW_WIDTH / aspect);
}

float wholeZoomScale() {
    const float aspect = static_cast<float>(frameWidth) / static_cast<float>(frameHeight);
    return baseViewHeight(aspect) / wholeViewHeight(aspect);
}

float viewHeight() {
    const float aspect = static_cast<float>(frameWidth) / static_cast<float>(frameHeight);
    return wholeZoom() ? wholeViewHeight(aspect) : baseViewHeight(aspect) / zoomScale();
}

bool canPanAtCurrentZoom() {
    const float aspect = static_cast<float>(frameWidth) / static_cast<float>(frameHeight);
    return viewHeight() + 0.0001f < wholeViewHeight(aspect);
}

int detailedPresetAt(int position) {
    if (wholeZoomScale() > 0.25f) {
        static const int order[6] = {1, 0, 2, 3, 4, 5};
        return order[position];
    }
    return position;
}

int presetAt(int position) {
    return detailedZoomMode ? detailedPresetAt(position) : position + 2;
}

int zoomSequenceLength() {
    return detailedZoomMode ? 6 : 3;
}

int zoomSequencePosition() {
    for (int position = 0; position < zoomSequenceLength(); ++position) {
        if (presetAt(position) == zoomPreset) return position;
    }
    return detailedZoomMode ? 3 : 1;
}

bool canZoomIn() {
    return zoomSequencePosition() < zoomSequenceLength() - 1;
}

bool canZoomOut() {
    return zoomSequencePosition() > 0;
}

void stepZoom(int direction) {
    const int next = std::max(0, std::min(zoomSequenceLength() - 1, zoomSequencePosition() + direction));
    zoomPreset = presetAt(next);
}

void projectedPanResult(float screenRight, float screenDown, float& nextX, float& nextY) {
    const Vec3 delta = cameraGroundRight() * screenRight + cameraGroundDown() * screenDown;
    nextX = std::max(-PAN_LIMIT, std::min(PAN_LIMIT, cameraPanX + delta.x));
    nextY = std::max(-PAN_LIMIT, std::min(PAN_LIMIT, cameraPanY + delta.y));
}

bool canPanDelta(float screenRight, float screenDown) {
    if (!canPanAtCurrentZoom()) return false;
    float nextX = cameraPanX;
    float nextY = cameraPanY;
    projectedPanResult(screenRight, screenDown, nextX, nextY);
    return std::fabs(nextX - cameraPanX) > 0.0001f || std::fabs(nextY - cameraPanY) > 0.0001f;
}

void panCamera(float screenRight, float screenDown) {
    if (!canPanAtCurrentZoom()) return;
    projectedPanResult(screenRight, screenDown, cameraPanX, cameraPanY);
}

bool intersectSphere(const Ray& ray, const RenderObject& object, float tMin, float tMax, Hit& hit) {
    const Vec3 oc = ray.origin - object.position;
    const float a = dot(ray.direction, ray.direction);
    const float halfB = dot(oc, ray.direction);
    const float c = dot(oc, oc) - object.size * object.size;
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
    hit.normal = normalise(hit.point - object.position);
    hit.colour = object.colour;
    hit.ground = false;
    return true;
}

bool intersectCube(const Ray& ray, const RenderObject& object, float tMin, float tMax, Hit& hit) {
    const Vec3 localOrigin = ray.origin - object.position;
    const float origins[3] = {localOrigin.x, localOrigin.y, localOrigin.z};
    const float directions[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    float nearT = tMin;
    float farT = tMax;
    int nearAxis = -1;
    float nearSign = 0.0f;

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(directions[axis]) < 1.0e-7f) {
            if (origins[axis] < -object.size || origins[axis] > object.size) return false;
            continue;
        }

        const float inv = 1.0f / directions[axis];
        float t0 = (-object.size - origins[axis]) * inv;
        float t1 = (object.size - origins[axis]) * inv;
        float enteringSign = -1.0f;
        if (t0 > t1) {
            std::swap(t0, t1);
            enteringSign = 1.0f;
        }
        if (t0 > nearT) {
            nearT = t0;
            nearAxis = axis;
            nearSign = enteringSign;
        }
        farT = std::min(farT, t1);
        if (farT < nearT) return false;
    }

    if (nearAxis < 0 || nearT < tMin || nearT > tMax) return false;
    hit.found = true;
    hit.t = nearT;
    hit.point = ray.origin + ray.direction * nearT;
    hit.normal = nearAxis == 0 ? Vec3(nearSign, 0.0f, 0.0f)
               : nearAxis == 1 ? Vec3(0.0f, nearSign, 0.0f)
                               : Vec3(0.0f, 0.0f, nearSign);
    hit.colour = object.colour;
    hit.ground = false;
    return true;
}

bool intersectTriangle(
    const Ray& ray,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    float tMin,
    float tMax,
    float& t,
    Vec3& normal
) {
    const Vec3 edge1 = b - a;
    const Vec3 edge2 = c - a;
    const Vec3 p = cross(ray.direction, edge2);
    const float determinant = dot(edge1, p);
    if (std::fabs(determinant) < 1.0e-7f) return false;
    const float invDet = 1.0f / determinant;
    const Vec3 s = ray.origin - a;
    const float u = dot(s, p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3 q = cross(s, edge1);
    const float v = dot(ray.direction, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = dot(edge2, q) * invDet;
    if (t < tMin || t > tMax) return false;
    normal = normalise(cross(edge1, edge2));
    if (dot(normal, ray.direction) > 0.0f) normal = normal * -1.0f;
    return true;
}

bool intersectPyramid(const Ray& ray, const RenderObject& object, float tMin, float tMax, Hit& hit) {
    const float s = object.size;
    const float height = object.size * 1.85f;
    const Vec3 base = object.position;
    const Vec3 p0 = base + Vec3(-s, -s, 0.0f);
    const Vec3 p1 = base + Vec3(s, -s, 0.0f);
    const Vec3 p2 = base + Vec3(s, s, 0.0f);
    const Vec3 p3 = base + Vec3(-s, s, 0.0f);
    const Vec3 apex = base + Vec3(0.0f, 0.0f, height);
    const Vec3 triangles[6][3] = {
        {p0, p2, p1}, {p0, p3, p2},
        {p0, p1, apex}, {p1, p2, apex},
        {p2, p3, apex}, {p3, p0, apex}
    };

    bool found = false;
    float closest = tMax;
    Vec3 closestNormal;
    for (int index = 0; index < 6; ++index) {
        float t = 0.0f;
        Vec3 normal;
        if (intersectTriangle(ray, triangles[index][0], triangles[index][1], triangles[index][2], tMin, closest, t, normal)) {
            found = true;
            closest = t;
            closestNormal = normal;
        }
    }
    if (!found) return false;
    hit.found = true;
    hit.t = closest;
    hit.point = ray.origin + ray.direction * closest;
    hit.normal = closestNormal;
    hit.colour = object.colour;
    hit.ground = false;
    return true;
}

bool intersectCone(const Ray& ray, const RenderObject& object, float tMin, float tMax, Hit& hit) {
    const float radius = object.size;
    const float height = object.size * 1.90f;
    const float slope = radius / height;
    const float slope2 = slope * slope;
    const Vec3 origin = ray.origin - object.position;
    const Vec3 direction = ray.direction;

    const float a = direction.x * direction.x + direction.y * direction.y - slope2 * direction.z * direction.z;
    const float b = 2.0f * (origin.x * direction.x + origin.y * direction.y + slope2 * (height - origin.z) * direction.z);
    const float c = origin.x * origin.x + origin.y * origin.y - slope2 * (height - origin.z) * (height - origin.z);

    bool found = false;
    float closest = tMax;
    Vec3 closestNormal;

    const float discriminant = b * b - 4.0f * a * c;
    if (std::fabs(a) > 1.0e-7f && discriminant >= 0.0f) {
        const float root = std::sqrt(discriminant);
        const float roots[2] = {(-b - root) / (2.0f * a), (-b + root) / (2.0f * a)};
        for (float t : roots) {
            if (t < tMin || t > closest) continue;
            const Vec3 localPoint = origin + direction * t;
            if (localPoint.z < 0.0f || localPoint.z > height) continue;
            found = true;
            closest = t;
            closestNormal = normalise(Vec3(localPoint.x, localPoint.y, slope2 * (height - localPoint.z)));
        }
    }

    if (std::fabs(direction.z) > 1.0e-7f) {
        const float t = -origin.z / direction.z;
        if (t >= tMin && t <= closest) {
            const Vec3 localPoint = origin + direction * t;
            if (localPoint.x * localPoint.x + localPoint.y * localPoint.y <= radius * radius) {
                found = true;
                closest = t;
                closestNormal = Vec3(0.0f, 0.0f, -1.0f);
            }
        }
    }

    if (!found) return false;
    if (dot(closestNormal, ray.direction) > 0.0f) closestNormal = closestNormal * -1.0f;
    hit.found = true;
    hit.t = closest;
    hit.point = ray.origin + ray.direction * closest;
    hit.normal = closestNormal;
    hit.colour = object.colour;
    hit.ground = false;
    return true;
}

const std::vector<Vec3>& dodecahedronPlaneNormals() {
    static std::vector<Vec3> normals;
    if (!normals.empty()) return normals;
    const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
    for (int a : {-1, 1}) {
        for (int b : {-1, 1}) {
            normals.push_back(normalise(Vec3(0.0f, static_cast<float>(a), static_cast<float>(b) * phi)));
            normals.push_back(normalise(Vec3(static_cast<float>(a), static_cast<float>(b) * phi, 0.0f)));
            normals.push_back(normalise(Vec3(static_cast<float>(a) * phi, 0.0f, static_cast<float>(b))));
        }
    }
    return normals;
}

const std::vector<Vec3>& icosahedronPlaneNormals() {
    static std::vector<Vec3> normals;
    if (!normals.empty()) return normals;
    const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
    const float invPhi = 1.0f / phi;
    for (int x : {-1, 1}) {
        for (int y : {-1, 1}) {
            for (int z : {-1, 1}) {
                normals.push_back(normalise(Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))));
            }
        }
    }
    for (int a : {-1, 1}) {
        for (int b : {-1, 1}) {
            normals.push_back(normalise(Vec3(0.0f, static_cast<float>(a) * invPhi, static_cast<float>(b) * phi)));
            normals.push_back(normalise(Vec3(static_cast<float>(a) * invPhi, static_cast<float>(b) * phi, 0.0f)));
            normals.push_back(normalise(Vec3(static_cast<float>(a) * phi, 0.0f, static_cast<float>(b) * invPhi)));
        }
    }
    return normals;
}

bool intersectConvexPolyhedron(
    const Ray& ray,
    const RenderObject& object,
    const std::vector<Vec3>& normals,
    float tMin,
    float tMax,
    Hit& hit
) {
    const Vec3 localOrigin = ray.origin - object.position;
    float enter = tMin;
    float exit = tMax;
    Vec3 enterNormal;
    bool hasEnterNormal = false;

    for (const Vec3& normal : normals) {
        const float denominator = dot(ray.direction, normal);
        const float distance = object.size - dot(localOrigin, normal);
        if (std::fabs(denominator) < 1.0e-7f) {
            if (distance < 0.0f) return false;
            continue;
        }

        const float t = distance / denominator;
        if (denominator < 0.0f) {
            if (t > enter) {
                enter = t;
                enterNormal = normal;
                hasEnterNormal = true;
            }
        } else {
            exit = std::min(exit, t);
        }
        if (enter > exit) return false;
    }

    if (!hasEnterNormal || enter < tMin || enter > tMax) return false;
    hit.found = true;
    hit.t = enter;
    hit.point = ray.origin + ray.direction * enter;
    hit.normal = enterNormal;
    hit.colour = object.colour;
    hit.ground = false;
    return true;
}

bool intersectObject(const Ray& ray, const RenderObject& object, float tMin, float tMax, Hit& hit) {
    switch (object.kind) {
        case ShapeKind::Sphere: return intersectSphere(ray, object, tMin, tMax, hit);
        case ShapeKind::Cube: return intersectCube(ray, object, tMin, tMax, hit);
        case ShapeKind::Cone: return intersectCone(ray, object, tMin, tMax, hit);
        case ShapeKind::Pyramid: return intersectPyramid(ray, object, tMin, tMax, hit);
        case ShapeKind::Dodecahedron:
            return intersectConvexPolyhedron(ray, object, dodecahedronPlaneNormals(), tMin, tMax, hit);
        case ShapeKind::Icosahedron:
            return intersectConvexPolyhedron(ray, object, icosahedronPlaneNormals(), tMin, tMax, hit);
    }
    return false;
}

Vec3 floorColour(const LevelView& level, const Vec3& point) {
    const int tileX = static_cast<int>(std::floor(point.x + 20.0f));
    const int tileY = static_cast<int>(std::floor(point.y + 20.0f));
    Vec3 colour = ((tileX + tileY) & 1) ? level.floorDark : level.floorLight;
    const float edgeX = std::fabs(point.x - std::round(point.x));
    const float edgeY = std::fabs(point.y - std::round(point.y));
    if (std::min(edgeX, edgeY) < 0.022f) colour = colour * 0.78f;
    return colour;
}

bool intersectGround(const Ray& ray, const LevelView& level, float tMin, float tMax, Hit& hit) {
    if (std::fabs(ray.direction.z) < 1.0e-7f) return false;
    const float t = -ray.origin.z / ray.direction.z;
    if (t < tMin || t > tMax) return false;
    const Vec3 point = ray.origin + ray.direction * t;
    if (std::fabs(point.x) > GROUND_LIMIT || std::fabs(point.y) > GROUND_LIMIT) return false;
    hit.found = true;
    hit.t = t;
    hit.point = point;
    hit.normal = Vec3(0.0f, 0.0f, 1.0f);
    hit.colour = floorColour(level, point);
    hit.ground = true;
    return true;
}

Hit traceClosest(const LevelView& level, const Ray& ray, float tMin, float tMax) {
    Hit closest;
    for (const RenderObject& object : level.objects) {
        Hit candidate;
        if (intersectObject(ray, object, tMin, tMax, candidate)) {
            closest = candidate;
            tMax = candidate.t;
        }
    }
    Hit ground;
    if (intersectGround(ray, level, tMin, tMax, ground)) closest = ground;
    return closest;
}

bool isOccluded(const LevelView& level, const Vec3& point, const Vec3& normal) {
    const Vec3 toLight = level.lightPosition - point;
    const float lightDistance = length(toLight);
    const Ray shadowRay{point + normal * EPSILON, toLight / lightDistance};
    return traceClosest(level, shadowRay, EPSILON, lightDistance - EPSILON).found;
}

Vec3 shade(const LevelView& level, const Hit& hit) {
    const Vec3 toLight = level.lightPosition - hit.point;
    const float distance = length(toLight);
    const Vec3 lightDirection = toLight / distance;
    const float diffuse = std::max(0.0f, dot(hit.normal, lightDirection));
    const float attenuation = 1.0f / (1.0f + 0.018f * distance * distance);
    const float visibility = isOccluded(level, hit.point, hit.normal) ? 0.0f : 1.0f;
    const float brightness = 0.19f + visibility * diffuse * attenuation * 1.18f;
    const Vec3 colour = hit.colour * brightness;
    return Vec3(std::min(colour.x, 1.0f), std::min(colour.y, 1.0f), std::min(colour.z, 1.0f));
}

Vec3 backgroundColour(float normalisedY) {
    const float t = std::max(0.0f, std::min(1.0f, normalisedY));
    return Vec3(0.075f, 0.12f, 0.18f) * (1.0f - t) + Vec3(0.20f, 0.28f, 0.34f) * t;
}

Ray cameraRay(float pixelX, float pixelY) {
    const Vec3 forward = cameraForward();
    const Vec3 right = normalise(cross(forward, Vec3(0.0f, 0.0f, 1.0f)));
    const Vec3 up = normalise(cross(right, forward));
    const float aspect = static_cast<float>(frameWidth) / static_cast<float>(frameHeight);
    const float height = viewHeight();
    const float width = height * aspect;
    const float screenX = (pixelX / static_cast<float>(frameWidth) - 0.5f) * width;
    const float screenY = (0.5f - pixelY / static_cast<float>(frameHeight)) * height;
    const Vec3 focus = canPanAtCurrentZoom() ? BASE_FOCUS + Vec3(cameraPanX, cameraPanY, 0.0f) : BASE_FOCUS;
    return Ray{focus - forward * 9.0f + right * screenX + up * screenY, forward};
}

Vec3 traceSample(float pixelX, float pixelY) {
    const Ray ray = cameraRay(pixelX, pixelY);
    const LevelView& activeLevel = levels[static_cast<std::size_t>(selectedLevel)];
    const Hit activeHit = traceClosest(activeLevel, ray, EPSILON, FAR_DISTANCE);
    Vec3 colour = activeHit.found ? shade(activeLevel, activeHit) : backgroundColour(pixelY / static_cast<float>(frameHeight));

    // Only lower levels are ghosted. Upper levels are never rendered.
    for (int index = selectedLevel - 1, depth = 1; index >= 0; --index, ++depth) {
        const LevelView& lowerLevel = levels[static_cast<std::size_t>(index)];
        const Hit lowerHit = traceClosest(lowerLevel, ray, EPSILON, FAR_DISTANCE);
        if (!lowerHit.found) continue;
        const Vec3 ghost = shade(lowerLevel, lowerHit);
        const float alphaBase = lowerHit.ground ? 0.035f : 0.20f;
        const float alpha = alphaBase * std::pow(0.58f, static_cast<float>(depth - 1));
        colour = blend(colour, ghost, alpha);
    }

    return colour;
}

std::uint8_t toByte(float value) {
    value = std::pow(std::max(0.0f, std::min(1.0f, value)), 1.0f / 2.2f);
    return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
}

float distanceToSegment(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    const float denominator = dx * dx + dy * dy;
    const float amount = denominator > 0.0f
        ? std::max(0.0f, std::min(1.0f, ((px - ax) * dx + (py - ay) * dy) / denominator))
        : 0.0f;
    const float nearestX = ax + dx * amount;
    const float nearestY = ay + dy * amount;
    const float deltaX = px - nearestX;
    const float deltaY = py - nearestY;
    return std::sqrt(deltaX * deltaX + deltaY * deltaY);
}

float curvedArrowDistance(float px, float py) {
    constexpr int SEGMENTS = 28;
    const float cx = 27.5f;
    const float cy = 25.0f;
    const float rx = 19.0f;
    const float ry = 12.0f;
    const float start = PI * 0.89f;
    const float end = PI * 0.10f;
    float minimum = 1000.0f;
    float previousX = cx + rx * std::cos(start);
    float previousY = cy - ry * std::sin(start);

    for (int index = 1; index <= SEGMENTS; ++index) {
        const float angle = start + (end - start) * static_cast<float>(index) / static_cast<float>(SEGMENTS);
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
    return std::min(minimum, distanceToSegment(px, py, previousX, previousY, baseX - nx * 5.5f, baseY - ny * 5.5f));
}

float directionalArrowDistance(float px, float py, float dx, float dy) {
    const float centre = PAN_ARROW_SIZE * 0.5f;
    const float nx = -dy;
    const float ny = dx;
    const float tailX = centre - dx * 10.0f;
    const float tailY = centre - dy * 10.0f;
    const float neckX = centre + dx * 5.0f;
    const float neckY = centre + dy * 5.0f;
    const float tipX = centre + dx * 13.0f;
    const float tipY = centre + dy * 13.0f;
    float minimum = distanceToSegment(px, py, tailX, tailY, neckX, neckY);
    minimum = std::min(minimum, distanceToSegment(px, py, tipX, tipY, neckX + nx * 6.0f, neckY + ny * 6.0f));
    return std::min(minimum, distanceToSegment(px, py, tipX, tipY, neckX - nx * 6.0f, neckY - ny * 6.0f));
}

float zoomGlyphDistance(float px, float py, bool plus) {
    const float centre = ZOOM_CONTROL_SIZE * 0.5f;
    const float arm = 8.5f;
    float minimum = distanceToSegment(px, py, centre - arm, centre, centre + arm, centre);
    if (plus) minimum = std::min(minimum, distanceToSegment(px, py, centre, centre - arm, centre, centre + arm));
    return minimum;
}

dsr::OrderedImageRgbaU8 frame;
dsr::OrderedImageRgbaU8 clockwiseSprite;
dsr::OrderedImageRgbaU8 counterClockwiseSprite;
dsr::OrderedImageRgbaU8 upSprite;
dsr::OrderedImageRgbaU8 downSprite;
dsr::OrderedImageRgbaU8 leftSprite;
dsr::OrderedImageRgbaU8 rightSprite;
dsr::OrderedImageRgbaU8 upDisabledSprite;
dsr::OrderedImageRgbaU8 downDisabledSprite;
dsr::OrderedImageRgbaU8 leftDisabledSprite;
dsr::OrderedImageRgbaU8 rightDisabledSprite;
dsr::OrderedImageRgbaU8 resetSprite;
dsr::OrderedImageRgbaU8 resetDisabledSprite;
dsr::OrderedImageRgbaU8 plusSprite;
dsr::OrderedImageRgbaU8 minusSprite;
dsr::OrderedImageRgbaU8 plusDisabledSprite;
dsr::OrderedImageRgbaU8 minusDisabledSprite;
std::vector<std::uint8_t> rgba;

void writeSpritePixel(dsr::OrderedImageRgbaU8& sprite, int x, int y, float distance, float strength) {
    const float outer = std::max(0.0f, std::min(1.0f, 3.6f - distance));
    if (outer <= 0.0f) return;
    const float core = std::max(0.0f, std::min(1.0f, 2.25f - distance));
    const int alpha = static_cast<int>(outer * 235.0f * strength + 0.5f);
    const int shade = static_cast<int>((64.0f + core * 186.0f) * strength + 0.5f);
    dsr::image_writePixel(sprite, x, y, dsr::ColorRgbaI32(shade, shade, shade, alpha));
}

void buildRotateSprite(dsr::OrderedImageRgbaU8& sprite, bool mirror, float strength) {
    sprite = dsr::image_create_RgbaU8(ROTATE_ARROW_WIDTH, ROTATE_ARROW_HEIGHT, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));
    for (int y = 0; y < ROTATE_ARROW_HEIGHT; ++y) {
        for (int x = 0; x < ROTATE_ARROW_WIDTH; ++x) {
            const float sampleX = mirror ? ROTATE_ARROW_WIDTH - 1 - x + 0.5f : x + 0.5f;
            writeSpritePixel(sprite, x, y, curvedArrowDistance(sampleX, y + 0.5f), strength);
        }
    }
}

void buildPanSprite(dsr::OrderedImageRgbaU8& sprite, float dx, float dy, float strength) {
    sprite = dsr::image_create_RgbaU8(PAN_ARROW_SIZE, PAN_ARROW_SIZE, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));
    for (int y = 0; y < PAN_ARROW_SIZE; ++y) {
        for (int x = 0; x < PAN_ARROW_SIZE; ++x) {
            writeSpritePixel(sprite, x, y, directionalArrowDistance(x + 0.5f, y + 0.5f, dx, dy), strength);
        }
    }
}

void buildResetSprite(dsr::OrderedImageRgbaU8& sprite, float strength) {
    sprite = dsr::image_create_RgbaU8(RESET_DISK_SIZE, RESET_DISK_SIZE, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));
    const float centre = RESET_DISK_SIZE * 0.5f;
    for (int y = 0; y < RESET_DISK_SIZE; ++y) {
        for (int x = 0; x < RESET_DISK_SIZE; ++x) {
            const float dx = x + 0.5f - centre;
            const float dy = y + 0.5f - centre;
            const float radius = std::sqrt(dx * dx + dy * dy);
            if (radius > 12.5f) continue;
            const float edge = std::max(0.0f, std::min(1.0f, 13.5f - radius));
            const int shade = static_cast<int>((radius < 9.0f ? 198.0f : 232.0f) * strength + 0.5f);
            const int alpha = static_cast<int>(edge * 225.0f * strength + 0.5f);
            dsr::image_writePixel(sprite, x, y, dsr::ColorRgbaI32(shade, shade, shade, alpha));
        }
    }
}

void buildZoomSprite(dsr::OrderedImageRgbaU8& sprite, bool plus, float strength) {
    sprite = dsr::image_create_RgbaU8(ZOOM_CONTROL_SIZE, ZOOM_CONTROL_SIZE, true);
    dsr::image_fill(sprite, dsr::ColorRgbaI32(0, 0, 0, 0));
    for (int y = 0; y < ZOOM_CONTROL_SIZE; ++y) {
        for (int x = 0; x < ZOOM_CONTROL_SIZE; ++x) {
            writeSpritePixel(sprite, x, y, zoomGlyphDistance(x + 0.5f, y + 0.5f, plus), strength);
        }
    }
}

void ensureFrame() {
    if (!dsr::image_exists(frame) || allocatedFrameWidth != frameWidth || allocatedFrameHeight != frameHeight) {
        frame = dsr::image_create_RgbaU8(frameWidth, frameHeight, false);
        allocatedFrameWidth = frameWidth;
        allocatedFrameHeight = frameHeight;
    }
    if (!dsr::image_exists(clockwiseSprite)) {
        buildRotateSprite(clockwiseSprite, false, 1.0f);
        buildRotateSprite(counterClockwiseSprite, true, 1.0f);
    }
    if (!dsr::image_exists(upSprite)) {
        buildPanSprite(upSprite, 0.0f, -1.0f, 1.0f);
        buildPanSprite(downSprite, 0.0f, 1.0f, 1.0f);
        buildPanSprite(leftSprite, -1.0f, 0.0f, 1.0f);
        buildPanSprite(rightSprite, 1.0f, 0.0f, 1.0f);
        buildPanSprite(upDisabledSprite, 0.0f, -1.0f, 0.28f);
        buildPanSprite(downDisabledSprite, 0.0f, 1.0f, 0.28f);
        buildPanSprite(leftDisabledSprite, -1.0f, 0.0f, 0.28f);
        buildPanSprite(rightDisabledSprite, 1.0f, 0.0f, 0.28f);
    }
    if (!dsr::image_exists(resetSprite)) {
        buildResetSprite(resetSprite, 1.0f);
        buildResetSprite(resetDisabledSprite, 0.28f);
    }
    if (!dsr::image_exists(plusSprite)) {
        buildZoomSprite(plusSprite, true, 1.0f);
        buildZoomSprite(minusSprite, false, 1.0f);
        buildZoomSprite(plusDisabledSprite, true, 0.28f);
        buildZoomSprite(minusDisabledSprite, false, 0.28f);
    }
    const std::size_t required = static_cast<std::size_t>(frameWidth) * static_cast<std::size_t>(frameHeight) * 4;
    if (rgba.size() != required) rgba.resize(required);
}

void renderControls() {
    // Zoom, top-left.
    const int zoomX = ZOOM_LEFT + (RESET_DISK_SIZE - ZOOM_CONTROL_SIZE) / 2;
    const int zoomResetX = ZOOM_LEFT;
    const int zoomInTop = ZOOM_TOP;
    const int zoomResetTop = zoomInTop + ZOOM_CONTROL_SIZE + ZOOM_GAP;
    const int zoomOutTop = zoomResetTop + RESET_DISK_SIZE + ZOOM_GAP;
    dsr::draw_alphaFilter(frame, canZoomIn() ? plusSprite : plusDisabledSprite, zoomX, zoomInTop);
    dsr::draw_alphaFilter(frame, zoomPreset == 3 ? resetDisabledSprite : resetSprite, zoomResetX, zoomResetTop);
    dsr::draw_alphaFilter(frame, canZoomOut() ? minusSprite : minusDisabledSprite, zoomX, zoomOutTop);

    // Z-level selector, top-right. The level count is dynamic.
    const int levelX = frameWidth - LEVEL_RIGHT - PAN_ARROW_SIZE;
    const int levelResetX = levelX + (PAN_ARROW_SIZE - RESET_DISK_SIZE) / 2;
    const int levelUpTop = LEVEL_TOP;
    const int levelResetTop = levelUpTop + PAN_ARROW_SIZE + LEVEL_GAP;
    const int levelDownTop = levelResetTop + RESET_DISK_SIZE + LEVEL_GAP;
    const bool canLevelUp = selectedLevel + 1 < static_cast<int>(levels.size());
    const bool canLevelDown = selectedLevel > 0;
    const bool canResetLevel = selectedLevel != DEFAULT_LEVEL_INDEX && DEFAULT_LEVEL_INDEX < static_cast<int>(levels.size());
    dsr::draw_alphaFilter(frame, canLevelUp ? upSprite : upDisabledSprite, levelX, levelUpTop);
    dsr::draw_alphaFilter(frame, canResetLevel ? resetSprite : resetDisabledSprite, levelResetX, levelResetTop);
    dsr::draw_alphaFilter(frame, canLevelDown ? downSprite : downDisabledSprite, levelX, levelDownTop);

    // Yaw, bottom-left.
    const int yawTop = frameHeight - CONTROL_BOTTOM - ROTATE_ARROW_HEIGHT;
    const int counterClockwiseX = ROTATE_LEFT_X;
    const int resetYawX = counterClockwiseX + ROTATE_ARROW_WIDTH + ROTATE_ROW_GAP;
    const int clockwiseX = resetYawX + RESET_DISK_SIZE + ROTATE_ROW_GAP;
    const int resetYawTop = yawTop + (ROTATE_ARROW_HEIGHT - RESET_DISK_SIZE) / 2;
    dsr::draw_alphaFilter(frame, counterClockwiseSprite, counterClockwiseX, yawTop);
    dsr::draw_alphaFilter(frame, cameraYawStep == 0 ? resetDisabledSprite : resetSprite, resetYawX, resetYawTop);
    dsr::draw_alphaFilter(frame, clockwiseSprite, clockwiseX, yawTop);

    // X/Y panning, bottom-right. Every arrow is independently state-aware.
    const int centreX = frameWidth - PAN_PAD_RIGHT - PAN_ARROW_SIZE - PAN_X_STEP;
    const int centreY = frameHeight - PAN_PAD_BOTTOM - PAN_ARROW_SIZE - PAN_Y_STEP;
    const bool canLeft = canPanDelta(-PAN_BUTTON_STEP, 0.0f);
    const bool canRight = canPanDelta(PAN_BUTTON_STEP, 0.0f);
    const bool canUp = canPanDelta(0.0f, PAN_BUTTON_STEP);
    const bool canDown = canPanDelta(0.0f, -PAN_BUTTON_STEP);
    const bool canResetPan = canPanAtCurrentZoom() && (std::fabs(cameraPanX) > 0.0001f || std::fabs(cameraPanY) > 0.0001f);
    dsr::draw_alphaFilter(frame, canLeft ? leftSprite : leftDisabledSprite, centreX - PAN_X_STEP, centreY);
    dsr::draw_alphaFilter(frame, canResetPan ? resetSprite : resetDisabledSprite, centreX, centreY);
    dsr::draw_alphaFilter(frame, canRight ? rightSprite : rightDisabledSprite, centreX + PAN_X_STEP, centreY);
    dsr::draw_alphaFilter(frame, canUp ? upSprite : upDisabledSprite, centreX, centreY - PAN_Y_STEP);
    dsr::draw_alphaFilter(frame, canDown ? downSprite : downDisabledSprite, centreX, centreY + PAN_Y_STEP);
}

void renderScene() {
    ensureFrame();
    const float offsets[2] = {0.25f, 0.75f};
    for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            Vec3 colour;
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    colour = colour + traceSample(x + offsets[sx], y + offsets[sy]);
                }
            }
            colour = colour * 0.25f;
            dsr::image_writePixel(frame, x, y, dsr::ColorRgbaI32(toByte(colour.x), toByte(colour.y), toByte(colour.z), 255));
        }
    }

    renderControls();

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
    const bool panLeftEnabled = canPanDelta(-PAN_BUTTON_STEP, 0.0f);
    const bool panRightEnabled = canPanDelta(PAN_BUTTON_STEP, 0.0f);
    const bool panUpEnabled = canPanDelta(0.0f, PAN_BUTTON_STEP);
    const bool panDownEnabled = canPanDelta(0.0f, -PAN_BUTTON_STEP);
    const bool panResetEnabled = canPanAtCurrentZoom() && (std::fabs(cameraPanX) > 0.0001f || std::fabs(cameraPanY) > 0.0001f);
    const bool zoomResetEnabled = zoomPreset != 3;
    const bool yawResetEnabled = cameraYawStep != 0;
    const bool levelUpEnabled = selectedLevel + 1 < static_cast<int>(levels.size());
    const bool levelDownEnabled = selectedLevel > 0;
    const bool levelResetEnabled = selectedLevel != DEFAULT_LEVEL_INDEX && DEFAULT_LEVEL_INDEX < static_cast<int>(levels.size());
    const float currentViewHeight = viewHeight();
    const float fitScale = wholeZoomScale();

    EM_ASM({
        const pointer = $0;
        const width = $1;
        const height = $2;
        const yaw = $3;
        const panX = $4;
        const panY = $5;
        const zoomPreset = $6;
        const detailed = !!$7;
        const canZoomIn = !!$8;
        const canZoomOut = !!$9;
        const zoomResetEnabled = !!$10;
        const yawResetEnabled = !!$11;
        const panLeftEnabled = !!$12;
        const panRightEnabled = !!$13;
        const panUpEnabled = !!$14;
        const panDownEnabled = !!$15;
        const panResetEnabled = !!$16;
        const selectedLevel = $17;
        const levelCount = $18;
        const levelUpEnabled = !!$19;
        const levelDownEnabled = !!$20;
        const levelResetEnabled = !!$21;
        const viewHeight = $22;
        const wholeScale = $23;

        const canvas = document.getElementById('canvas');
        if (!canvas) return;
        if (canvas.width !== width) canvas.width = width;
        if (canvas.height !== height) canvas.height = height;
        const context = canvas.getContext('2d', { alpha: false });
        const imageData = context.createImageData(width, height);
        imageData.data.set(HEAPU8.subarray(pointer, pointer + width * height * 4));
        context.putImageData(imageData, 0, 0);

        window.isowebViewHeightWorld = viewHeight;
        window.isowebWholeZoomScale = wholeScale;
        window.isowebSelectedLevel = selectedLevel;
        window.isowebLevelCount = levelCount;

        function setControlEnabled(id, enabled) {
            const control = document.getElementById(id);
            if (control) control.disabled = !enabled;
        }
        setControlEnabled('zoom-in', canZoomIn);
        setControlEnabled('zoom-out', canZoomOut);
        setControlEnabled('reset-zoom', zoomResetEnabled);
        setControlEnabled('reset-yaw', yawResetEnabled);
        setControlEnabled('pan-left', panLeftEnabled);
        setControlEnabled('pan-right', panRightEnabled);
        setControlEnabled('pan-up', panUpEnabled);
        setControlEnabled('pan-down', panDownEnabled);
        setControlEnabled('reset-camera', panResetEnabled);
        setControlEnabled('level-up', levelUpEnabled);
        setControlEnabled('level-down', levelDownEnabled);
        setControlEnabled('reset-level', levelResetEnabled);

        document.documentElement.classList.add('wasm-ready');
        const loading = document.getElementById('loading');
        if (loading) loading.hidden = true;

        let zoomLabel = '1x';
        if (zoomPreset === 0) zoomLabel = 'whole';
        else if (zoomPreset === 1) zoomLabel = '0.25x';
        else if (zoomPreset === 2) zoomLabel = '0.5x';
        else if (zoomPreset === 4) zoomLabel = '2x';
        else if (zoomPreset === 5) zoomLabel = '4x';
        const status = document.getElementById('view-status');
        if (status) {
            status.textContent =
                'Yaw ' + yaw + ' degrees; pan X ' + panY.toFixed(2) + '; pan Y ' + panX.toFixed(2) +
                '; zoom ' + zoomLabel + (detailed ? ' detailed' : ' regular') +
                '; Z level ' + (selectedLevel + 1) + ' of ' + levelCount;
        }
    }, static_cast<int>(pointer), frameWidth, frameHeight, cameraYawStep * 45,
       cameraPanX, cameraPanY, zoomPreset, detailedZoomMode ? 1 : 0,
       canZoomIn() ? 1 : 0, canZoomOut() ? 1 : 0, zoomResetEnabled ? 1 : 0,
       yawResetEnabled ? 1 : 0, panLeftEnabled ? 1 : 0, panRightEnabled ? 1 : 0,
       panUpEnabled ? 1 : 0, panDownEnabled ? 1 : 0, panResetEnabled ? 1 : 0,
       selectedLevel, static_cast<int>(levels.size()), levelUpEnabled ? 1 : 0,
       levelDownEnabled ? 1 : 0, levelResetEnabled ? 1 : 0, currentViewHeight, fitScale);
}

void redraw() {
    renderScene();
    presentScene();
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_render() { redraw(); }

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_resize(int width, int height) {
    frameWidth = std::max(160, std::min(1600, width));
    frameHeight = std::max(160, std::min(1600, height));
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_clockwise() {
    cameraYawStep = (cameraYawStep + 1) & 7;
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_counterclockwise() {
    cameraYawStep = (cameraYawStep + 7) & 7;
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_yaw() {
    cameraYawStep = 0;
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_in() {
    if (canZoomIn()) stepZoom(1);
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_out() {
    if (canZoomOut()) stepZoom(-1);
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_zoom() {
    zoomPreset = 3;
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_detailed_mode(int enabled) {
    detailedZoomMode = enabled != 0;
    if (!detailedZoomMode) {
        if (zoomPreset < 2) zoomPreset = 2;
        if (zoomPreset > 4) zoomPreset = 4;
    }
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_pan(float screenRight, float screenDown) {
    panCamera(screenRight, screenDown);
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_camera() {
    if (canPanAtCurrentZoom()) {
        cameraPanX = 0.0f;
        cameraPanY = 0.0f;
    }
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_level_up() {
    if (selectedLevel + 1 < static_cast<int>(levels.size())) ++selectedLevel;
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_level_down() {
    if (selectedLevel > 0) --selectedLevel;
    redraw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_level() {
    if (DEFAULT_LEVEL_INDEX < static_cast<int>(levels.size())) selectedLevel = DEFAULT_LEVEL_INDEX;
    redraw();
}
