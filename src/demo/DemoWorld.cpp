#include "demo/DemoWorld.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace isoweb {
namespace demo {
namespace {

using engine::Ray;
using engine::Vec3;
using engine::WorldBounds;

constexpr float EPSILON = 0.0015f;
constexpr float FAR_DISTANCE = 1000.0f;
constexpr float GROUND_LIMIT = 4.40f;
const Vec3 BASE_FOCUS(0.0f, 0.15f, 0.55f);

constexpr int STAIR_STEP_COUNT = 7;
constexpr float STAIR_RISE = 1.40f;
constexpr float STAIR_WIDTH = 0.78f;
constexpr float STAIR_LOW_Y = -3.30f;
constexpr float STAIR_HIGH_Y = -1.20f;
constexpr float LOWER_MIDDLE_STAIR_X = 2.15f;
constexpr float MIDDLE_UPPER_STAIR_X = 3.20f;
constexpr float STAIR_HOLE_MARGIN = 0.06f;

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
  float height;
  Vec3 colour;
};

struct FloorHole {
  float minimumX;
  float maximumX;
  float minimumY;
  float maximumY;
};

struct Staircase {
  float centreX;
  float startY;
  float endY;
  float startZ;
  float endZ;
  float width;
};

struct LevelDefinition {
  std::vector<RenderObject> objects;
  std::vector<FloorHole> floorHoles;
  std::vector<Staircase> staircases;
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
};

float triangleIntersection(
  const Ray& ray,
  const Vec3& a,
  const Vec3& b,
  const Vec3& c,
  float minimum,
  float maximum,
  Vec3& normal
) {
  const Vec3 edge1 = b - a;
  const Vec3 edge2 = c - a;
  const Vec3 p = engine::cross(ray.direction, edge2);
  const float determinant = engine::dot(edge1, p);
  if (std::fabs(determinant) < 1e-7f) return FAR_DISTANCE;

  const float inverse = 1.0f / determinant;
  const Vec3 s = ray.origin - a;
  const float u = engine::dot(s, p) * inverse;
  if (u < 0.0f || u > 1.0f) return FAR_DISTANCE;

  const Vec3 q = engine::cross(s, edge1);
  const float v = engine::dot(ray.direction, q) * inverse;
  if (v < 0.0f || u + v > 1.0f) return FAR_DISTANCE;

  const float t = engine::dot(edge2, q) * inverse;
  if (t < minimum || t > maximum) return FAR_DISTANCE;

  normal = engine::normalise(engine::cross(edge1, edge2));
  if (engine::dot(normal, ray.direction) > 0.0f) normal = normal * -1.0f;
  return t;
}

const std::vector<Vec3>& dodecahedronNormals() {
  static std::vector<Vec3> normals;
  if (!normals.empty()) return normals;

  const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
  for (int a : {-1, 1}) {
    for (int b : {-1, 1}) {
      normals.push_back(engine::normalise({0.0f, static_cast<float>(a), static_cast<float>(b) * phi}));
      normals.push_back(engine::normalise({static_cast<float>(a), static_cast<float>(b) * phi, 0.0f}));
      normals.push_back(engine::normalise({static_cast<float>(a) * phi, 0.0f, static_cast<float>(b)}));
    }
  }
  return normals;
}

const std::vector<Vec3>& icosahedronNormals() {
  static std::vector<Vec3> normals;
  if (!normals.empty()) return normals;

  const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
  const float inversePhi = 1.0f / phi;

  for (int x : {-1, 1}) {
    for (int y : {-1, 1}) {
      for (int z : {-1, 1}) {
        normals.push_back(engine::normalise({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}));
      }
    }
  }

  for (int a : {-1, 1}) {
    for (int b : {-1, 1}) {
      normals.push_back(engine::normalise({0.0f, static_cast<float>(a) * inversePhi, static_cast<float>(b) * phi}));
      normals.push_back(engine::normalise({static_cast<float>(a) * inversePhi, static_cast<float>(b) * phi, 0.0f}));
      normals.push_back(engine::normalise({static_cast<float>(a) * phi, 0.0f, static_cast<float>(b) * inversePhi}));
    }
  }
  return normals;
}

FloorHole stairHole(float centreX) {
  return {
    centreX - STAIR_WIDTH * 0.5f - STAIR_HOLE_MARGIN,
    centreX + STAIR_WIDTH * 0.5f + STAIR_HOLE_MARGIN,
    STAIR_LOW_Y - STAIR_HOLE_MARGIN,
    STAIR_HIGH_Y + STAIR_HOLE_MARGIN
  };
}

class DemoLevel final : public engine::IWorldLevel {
public:
  explicit DemoLevel(LevelDefinition definition)
      : definition_(std::move(definition)) {
    buildBounds();
  }

  const WorldBounds& bounds() const override {
    return bounds_;
  }

  Vec3 sample(const Ray& ray, float backgroundY) const override {
    const Hit hit = traceClosest(ray, EPSILON, FAR_DISTANCE);
    return hit.found ? shade(hit) : background(backgroundY);
  }

private:
  void buildBounds() {
    bounds_.focus = BASE_FOCUS;
    bounds_.points.clear();
    bounds_.points.reserve(
      4 + definition_.objects.size() * 8 + definition_.staircases.size() * 8
    );

    bounds_.points.push_back({-GROUND_LIMIT, -GROUND_LIMIT, 0.0f});
    bounds_.points.push_back({GROUND_LIMIT, -GROUND_LIMIT, 0.0f});
    bounds_.points.push_back({-GROUND_LIMIT, GROUND_LIMIT, 0.0f});
    bounds_.points.push_back({GROUND_LIMIT, GROUND_LIMIT, 0.0f});

    for (const RenderObject& object : definition_.objects) {
      const Vec3 extent = objectExtent(object);
      for (int x : {-1, 1}) {
        for (int y : {-1, 1}) {
          for (int z : {-1, 1}) {
            bounds_.points.push_back(
              object.position + Vec3(extent.x * x, extent.y * y, extent.z * z)
            );
          }
        }
      }
    }

    for (const Staircase& staircase : definition_.staircases) {
      const float minimumY = std::min(staircase.startY, staircase.endY);
      const float maximumY = std::max(staircase.startY, staircase.endY);
      const float minimumZ = std::min(staircase.startZ, staircase.endZ);
      const float maximumZ = std::max(staircase.startZ, staircase.endZ);
      for (float x : {
        staircase.centreX - staircase.width * 0.5f,
        staircase.centreX + staircase.width * 0.5f
      }) {
        for (float y : {minimumY, maximumY}) {
          for (float z : {minimumZ, maximumZ}) {
            bounds_.points.push_back({x, y, z});
          }
        }
      }
    }
  }

  Vec3 objectExtent(const RenderObject& object) const {
    if (object.kind == ShapeKind::Sphere) return {object.size, object.size, object.size};
    if (object.kind == ShapeKind::Dodecahedron || object.kind == ShapeKind::Icosahedron) {
      const float radius = object.size * 1.55f;
      return {radius, radius, radius};
    }
    return {object.size, object.size, object.height * 0.5f};
  }

  bool intersectSphere(const Ray& ray, const RenderObject& object, float minimum, float maximum, Hit& hit) const {
    const Vec3 offset = ray.origin - object.position;
    const float a = engine::dot(ray.direction, ray.direction);
    const float halfB = engine::dot(offset, ray.direction);
    const float c = engine::dot(offset, offset) - object.size * object.size;
    const float discriminant = halfB * halfB - a * c;
    if (discriminant < 0.0f) return false;

    const float root = std::sqrt(discriminant);
    float t = (-halfB - root) / a;
    if (t < minimum || t > maximum) {
      t = (-halfB + root) / a;
      if (t < minimum || t > maximum) return false;
    }

    hit.found = true;
    hit.t = t;
    hit.point = ray.origin + ray.direction * t;
    hit.normal = engine::normalise(hit.point - object.position);
    hit.colour = object.colour;
    return true;
  }

  bool intersectCube(const Ray& ray, const RenderObject& object, float minimum, float maximum, Hit& hit) const {
    const Vec3 localOrigin = ray.origin - object.position;
    const float half[3] = {object.size, object.size, object.height * 0.5f};
    const float origin[3] = {localOrigin.x, localOrigin.y, localOrigin.z};
    const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    float nearT = minimum;
    float farT = maximum;
    int normalAxis = -1;
    float normalSign = 0.0f;

    for (int axis = 0; axis < 3; ++axis) {
      if (std::fabs(direction[axis]) < 1e-7f) {
        if (origin[axis] < -half[axis] || origin[axis] > half[axis]) return false;
        continue;
      }

      const float inverse = 1.0f / direction[axis];
      float t0 = (-half[axis] - origin[axis]) * inverse;
      float t1 = (half[axis] - origin[axis]) * inverse;
      float sign = -1.0f;
      if (t0 > t1) {
        std::swap(t0, t1);
        sign = 1.0f;
      }
      if (t0 > nearT) {
        nearT = t0;
        normalAxis = axis;
        normalSign = sign;
      }
      farT = std::min(farT, t1);
      if (farT < nearT) return false;
    }

    if (normalAxis < 0) return false;
    hit.found = true;
    hit.t = nearT;
    hit.point = ray.origin + ray.direction * nearT;
    hit.normal = normalAxis == 0
      ? Vec3(normalSign, 0.0f, 0.0f)
      : normalAxis == 1
        ? Vec3(0.0f, normalSign, 0.0f)
        : Vec3(0.0f, 0.0f, normalSign);
    hit.colour = object.colour;
    return true;
  }

  bool intersectCone(const Ray& ray, const RenderObject& object, float minimum, float maximum, Hit& hit) const {
    const float radius = object.size;
    const float height = object.height;
    const float halfHeight = height * 0.5f;
    const float slope = radius / height;
    const float slopeSquared = slope * slope;
    const Vec3 origin = ray.origin - object.position;
    const Vec3 direction = ray.direction;
    const float apex = halfHeight;

    const float a = direction.x * direction.x + direction.y * direction.y - slopeSquared * direction.z * direction.z;
    const float b = 2.0f * (
      origin.x * direction.x + origin.y * direction.y +
      slopeSquared * (apex - origin.z) * direction.z
    );
    const float c = origin.x * origin.x + origin.y * origin.y -
      slopeSquared * (apex - origin.z) * (apex - origin.z);

    bool found = false;
    float closest = maximum;
    Vec3 normal;
    const float discriminant = b * b - 4.0f * a * c;

    if (std::fabs(a) > 1e-7f && discriminant >= 0.0f) {
      const float root = std::sqrt(discriminant);
      const float roots[2] = {(-b - root) / (2.0f * a), (-b + root) / (2.0f * a)};
      for (float t : roots) {
        if (t < minimum || t > closest) continue;
        const Vec3 point = origin + direction * t;
        if (point.z < -halfHeight || point.z > halfHeight) continue;
        found = true;
        closest = t;
        normal = engine::normalise({point.x, point.y, slopeSquared * (apex - point.z)});
      }
    }

    if (std::fabs(direction.z) > 1e-7f) {
      const float t = (-halfHeight - origin.z) / direction.z;
      if (t >= minimum && t <= closest) {
        const Vec3 point = origin + direction * t;
        if (point.x * point.x + point.y * point.y <= radius * radius) {
          found = true;
          closest = t;
          normal = {0.0f, 0.0f, -1.0f};
        }
      }
    }

    if (!found) return false;
    if (engine::dot(normal, ray.direction) > 0.0f) normal = normal * -1.0f;
    hit.found = true;
    hit.t = closest;
    hit.point = ray.origin + ray.direction * closest;
    hit.normal = normal;
    hit.colour = object.colour;
    return true;
  }

  bool intersectPyramid(const Ray& ray, const RenderObject& object, float minimum, float maximum, Hit& hit) const {
    const float size = object.size;
    const float baseZ = object.position.z - object.height * 0.5f;
    const float apexZ = object.position.z + object.height * 0.5f;
    const Vec3 p0(object.position.x - size, object.position.y - size, baseZ);
    const Vec3 p1(object.position.x + size, object.position.y - size, baseZ);
    const Vec3 p2(object.position.x + size, object.position.y + size, baseZ);
    const Vec3 p3(object.position.x - size, object.position.y + size, baseZ);
    const Vec3 apex(object.position.x, object.position.y, apexZ);
    const Vec3 triangles[6][3] = {
      {p0, p2, p1}, {p0, p3, p2},
      {p0, p1, apex}, {p1, p2, apex},
      {p2, p3, apex}, {p3, p0, apex}
    };

    bool found = false;
    float closest = maximum;
    Vec3 closestNormal;
    for (int index = 0; index < 6; ++index) {
      Vec3 normal;
      const float t = triangleIntersection(
        ray,
        triangles[index][0],
        triangles[index][1],
        triangles[index][2],
        minimum,
        closest,
        normal
      );
      if (t < closest) {
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
    return true;
  }

  bool intersectPolyhedron(
    const Ray& ray,
    const RenderObject& object,
    const std::vector<Vec3>& normals,
    float minimum,
    float maximum,
    Hit& hit
  ) const {
    const Vec3 localOrigin = ray.origin - object.position;
    float enter = minimum;
    float exit = maximum;
    Vec3 enterNormal;
    bool hasEnterNormal = false;

    for (const Vec3& normal : normals) {
      const float denominator = engine::dot(ray.direction, normal);
      const float distance = object.size - engine::dot(localOrigin, normal);
      if (std::fabs(denominator) < 1e-7f) {
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

    if (!hasEnterNormal || enter < minimum || enter > maximum) return false;
    hit.found = true;
    hit.t = enter;
    hit.point = ray.origin + ray.direction * enter;
    hit.normal = enterNormal;
    hit.colour = object.colour;
    return true;
  }

  bool intersectObject(const Ray& ray, const RenderObject& object, float minimum, float maximum, Hit& hit) const {
    switch (object.kind) {
      case ShapeKind::Cube:
        return intersectCube(ray, object, minimum, maximum, hit);
      case ShapeKind::Sphere:
        return intersectSphere(ray, object, minimum, maximum, hit);
      case ShapeKind::Cone:
        return intersectCone(ray, object, minimum, maximum, hit);
      case ShapeKind::Pyramid:
        return intersectPyramid(ray, object, minimum, maximum, hit);
      case ShapeKind::Dodecahedron:
        return intersectPolyhedron(ray, object, dodecahedronNormals(), minimum, maximum, hit);
      case ShapeKind::Icosahedron:
        return intersectPolyhedron(ray, object, icosahedronNormals(), minimum, maximum, hit);
    }
    return false;
  }

  Vec3 floorColour(const Vec3& point) const {
    const int x = static_cast<int>(std::floor(point.x + 20.0f));
    const int y = static_cast<int>(std::floor(point.y + 20.0f));
    Vec3 colour = ((x + y) & 1) ? definition_.floorDark : definition_.floorLight;
    const float edgeX = std::fabs(point.x - std::round(point.x));
    const float edgeY = std::fabs(point.y - std::round(point.y));
    if (std::min(edgeX, edgeY) < 0.022f) colour = colour * 0.78f;
    return colour;
  }

  bool insideFloorHole(const Vec3& point) const {
    for (const FloorHole& hole : definition_.floorHoles) {
      if (
        point.x >= hole.minimumX && point.x <= hole.maximumX &&
        point.y >= hole.minimumY && point.y <= hole.maximumY
      ) {
        return true;
      }
    }
    return false;
  }

  bool intersectAxisAlignedBox(
    const Ray& ray,
    const Vec3& centre,
    const Vec3& halfExtent,
    float minimum,
    float maximum,
    Hit& hit
  ) const {
    const Vec3 localOrigin = ray.origin - centre;
    const float origin[3] = {localOrigin.x, localOrigin.y, localOrigin.z};
    const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float half[3] = {halfExtent.x, halfExtent.y, halfExtent.z};
    float nearT = minimum;
    float farT = maximum;
    int normalAxis = -1;
    float normalSign = 0.0f;

    for (int axis = 0; axis < 3; ++axis) {
      if (std::fabs(direction[axis]) < 1e-7f) {
        if (origin[axis] < -half[axis] || origin[axis] > half[axis]) return false;
        continue;
      }

      const float inverse = 1.0f / direction[axis];
      float t0 = (-half[axis] - origin[axis]) * inverse;
      float t1 = (half[axis] - origin[axis]) * inverse;
      float sign = -1.0f;
      if (t0 > t1) {
        std::swap(t0, t1);
        sign = 1.0f;
      }
      if (t0 > nearT) {
        nearT = t0;
        normalAxis = axis;
        normalSign = sign;
      }
      farT = std::min(farT, t1);
      if (farT < nearT) return false;
    }

    if (normalAxis < 0) return false;
    hit.found = true;
    hit.t = nearT;
    hit.point = ray.origin + ray.direction * nearT;
    hit.normal = normalAxis == 0
      ? Vec3(normalSign, 0.0f, 0.0f)
      : normalAxis == 1
        ? Vec3(0.0f, normalSign, 0.0f)
        : Vec3(0.0f, 0.0f, normalSign);
    return true;
  }

  bool intersectStaircase(
    const Ray& ray,
    const Staircase& staircase,
    float minimum,
    float maximum,
    Hit& hit
  ) const {
    bool found = false;
    float closest = maximum;
    const float lowerZ = std::min(staircase.startZ, staircase.endZ);
    const bool ascending = staircase.endZ > staircase.startZ;

    for (int index = 0; index < STAIR_STEP_COUNT; ++index) {
      const float y0 = staircase.startY +
        (staircase.endY - staircase.startY) * static_cast<float>(index) / STAIR_STEP_COUNT;
      const float y1 = staircase.startY +
        (staircase.endY - staircase.startY) * static_cast<float>(index + 1) / STAIR_STEP_COUNT;
      const float fraction = ascending
        ? static_cast<float>(index + 1) / STAIR_STEP_COUNT
        : static_cast<float>(index) / STAIR_STEP_COUNT;
      const float topZ = staircase.startZ +
        (staircase.endZ - staircase.startZ) * fraction;
      const float boxMinimumZ = lowerZ;
      const float boxMaximumZ = std::max(topZ, lowerZ + 0.025f);
      const Vec3 centre(
        staircase.centreX,
        (y0 + y1) * 0.5f,
        (boxMinimumZ + boxMaximumZ) * 0.5f
      );
      const Vec3 halfExtent(
        staircase.width * 0.5f,
        std::fabs(y1 - y0) * 0.5f,
        (boxMaximumZ - boxMinimumZ) * 0.5f
      );

      Hit candidate;
      if (intersectAxisAlignedBox(ray, centre, halfExtent, minimum, closest, candidate)) {
        found = true;
        closest = candidate.t;
        hit = candidate;
        hit.colour = floorColour(hit.point);
      }
    }
    return found;
  }

  bool intersectGround(const Ray& ray, float minimum, float maximum, Hit& hit) const {
    if (std::fabs(ray.direction.z) < 1e-7f) return false;
    const float t = -ray.origin.z / ray.direction.z;
    if (t < minimum || t > maximum) return false;

    const Vec3 point = ray.origin + ray.direction * t;
    if (std::fabs(point.x) > GROUND_LIMIT || std::fabs(point.y) > GROUND_LIMIT) return false;
    if (insideFloorHole(point)) return false;

    hit.found = true;
    hit.t = t;
    hit.point = point;
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.colour = floorColour(point);
    return true;
  }

  Hit traceClosest(const Ray& ray, float minimum, float maximum) const {
    Hit result;
    for (const RenderObject& object : definition_.objects) {
      Hit hit;
      if (intersectObject(ray, object, minimum, maximum, hit)) {
        result = hit;
        maximum = hit.t;
      }
    }

    for (const Staircase& staircase : definition_.staircases) {
      Hit hit;
      if (intersectStaircase(ray, staircase, minimum, maximum, hit)) {
        result = hit;
        maximum = hit.t;
      }
    }

    Hit ground;
    if (intersectGround(ray, minimum, maximum, ground)) result = ground;
    return result;
  }

  bool occluded(Vec3 point, Vec3 normal) const {
    const Vec3 toLight = definition_.lightPosition - point;
    const float distance = engine::length(toLight);
    return traceClosest(
      {point + normal * EPSILON, toLight / distance},
      EPSILON,
      distance - EPSILON
    ).found;
  }

  Vec3 shade(const Hit& hit) const {
    const Vec3 toLight = definition_.lightPosition - hit.point;
    const float distance = engine::length(toLight);
    const float diffuse = std::max(0.0f, engine::dot(hit.normal, toLight / distance));
    const float attenuation = 1.0f / (1.0f + 0.018f * distance * distance);
    const float visibility = occluded(hit.point, hit.normal) ? 0.0f : 1.0f;
    const Vec3 colour = hit.colour * (0.19f + visibility * diffuse * attenuation * 1.18f);
    return {
      std::min(colour.x, 1.0f),
      std::min(colour.y, 1.0f),
      std::min(colour.z, 1.0f)
    };
  }

  Vec3 background(float y) const {
    const float t = std::max(0.0f, std::min(1.0f, y));
    return Vec3(0.075f, 0.12f, 0.18f) * (1.0f - t) + Vec3(0.20f, 0.28f, 0.34f) * t;
  }

  LevelDefinition definition_;
  WorldBounds bounds_;
};

LevelDefinition lowerLevel() {
  LevelDefinition level;
  level.lightPosition = {4.20f, -3.20f, 5.60f};
  level.floorDark = {0.34f, 0.34f, 0.36f};
  level.floorLight = {0.40f, 0.40f, 0.42f};
  level.objects.push_back({ShapeKind::Cone, {-1.30f, -0.80f, 0.82f}, 0.86f, 1.64f, {0.62f, 0.25f, 0.82f}});
  level.objects.push_back({ShapeKind::Pyramid, {1.20f, 0.85f, 0.83f}, 0.92f, 1.66f, {0.96f, 0.78f, 0.16f}});
  level.staircases.push_back({
    LOWER_MIDDLE_STAIR_X,
    STAIR_LOW_Y,
    STAIR_HIGH_Y,
    0.0f,
    STAIR_RISE,
    STAIR_WIDTH
  });
  return level;
}

LevelDefinition middleLevel() {
  LevelDefinition level;
  level.lightPosition = {-3.60f, -4.20f, 6.50f};
  level.floorDark = {0.567f, 0.6048f, 0.630f};
  level.floorLight = {0.621f, 0.6624f, 0.690f};
  level.objects.push_back({ShapeKind::Cube, {-1.05f, 0.65f, 0.775f}, 0.80f, 1.55f, {0.18f, 0.48f, 0.88f}});
  level.objects.push_back({ShapeKind::Sphere, {1.05f, -0.25f, 0.90f}, 0.90f, 1.80f, {0.95f, 0.43f, 0.12f}});

  level.floorHoles.push_back(stairHole(LOWER_MIDDLE_STAIR_X));
  level.staircases.push_back({
    LOWER_MIDDLE_STAIR_X,
    STAIR_HIGH_Y,
    STAIR_LOW_Y,
    0.0f,
    -STAIR_RISE,
    STAIR_WIDTH
  });
  level.staircases.push_back({
    MIDDLE_UPPER_STAIR_X,
    STAIR_LOW_Y,
    STAIR_HIGH_Y,
    0.0f,
    STAIR_RISE,
    STAIR_WIDTH
  });
  return level;
}

LevelDefinition upperLevel() {
  LevelDefinition level;
  level.lightPosition = {3.80f, 4.40f, 7.20f};
  level.floorDark = {0.74f, 0.74f, 0.76f};
  level.floorLight = {0.82f, 0.82f, 0.84f};
  level.objects.push_back({ShapeKind::Dodecahedron, {-1.35f, 0.95f, 1.00f}, 0.72f, 0.0f, {0.18f, 0.50f, 0.94f}});
  level.objects.push_back({ShapeKind::Icosahedron, {1.30f, -0.95f, 1.10f}, 0.78f, 0.0f, {0.90f, 0.16f, 0.14f}});

  level.floorHoles.push_back(stairHole(MIDDLE_UPPER_STAIR_X));
  level.staircases.push_back({
    MIDDLE_UPPER_STAIR_X,
    STAIR_HIGH_Y,
    STAIR_LOW_Y,
    0.0f,
    -STAIR_RISE,
    STAIR_WIDTH
  });
  return level;
}

} // namespace

std::vector<std::unique_ptr<engine::IWorldLevel>> DemoWorld::makeLevels() {
  std::vector<std::unique_ptr<engine::IWorldLevel>> levels;
  levels.reserve(3);
  levels.push_back(std::make_unique<DemoLevel>(lowerLevel()));
  levels.push_back(std::make_unique<DemoLevel>(middleLevel()));
  levels.push_back(std::make_unique<DemoLevel>(upperLevel()));
  return levels;
}

DemoWorld::DemoWorld()
    : engine::World(makeLevels(), 1) {}

} // namespace demo
} // namespace isoweb
