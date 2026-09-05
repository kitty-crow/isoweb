#include "demo/DemoWorld.hpp"

#include <algorithm>
#include <cmath>

namespace isoweb {
namespace demo {
namespace {

using engine::Vec3;

constexpr float EPSILON = 0.0015f;
constexpr float FAR_DISTANCE = 1000.0f;
constexpr float GROUND_LIMIT = 4.40f;
const Vec3 CUBE_MIN(-1.85f, -0.15f, 0.0f);
const Vec3 CUBE_MAX(-0.25f, 1.45f, 1.55f);
const Vec3 SPHERE_CENTRE(1.05f, -0.25f, 0.90f);
const Vec3 LIGHT_POSITION(-3.60f, -4.20f, 6.50f);
const Vec3 BASE_FOCUS(0.0f, 0.15f, 0.55f);
constexpr float SPHERE_RADIUS = 0.90f;

} // namespace

DemoWorld::DemoWorld() {
  bounds_.focus = BASE_FOCUS;
  bounds_.points.reserve(18);

  bounds_.points.push_back({-GROUND_LIMIT, -GROUND_LIMIT, 0.0f});
  bounds_.points.push_back({GROUND_LIMIT, -GROUND_LIMIT, 0.0f});
  bounds_.points.push_back({-GROUND_LIMIT, GROUND_LIMIT, 0.0f});
  bounds_.points.push_back({GROUND_LIMIT, GROUND_LIMIT, 0.0f});

  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      for (int z = 0; z < 2; ++z) {
        bounds_.points.push_back({
          x ? CUBE_MAX.x : CUBE_MIN.x,
          y ? CUBE_MAX.y : CUBE_MIN.y,
          z ? CUBE_MAX.z : CUBE_MIN.z
        });
      }
    }
  }

  bounds_.points.push_back(SPHERE_CENTRE + Vec3(SPHERE_RADIUS, 0.0f, 0.0f));
  bounds_.points.push_back(SPHERE_CENTRE + Vec3(-SPHERE_RADIUS, 0.0f, 0.0f));
  bounds_.points.push_back(SPHERE_CENTRE + Vec3(0.0f, SPHERE_RADIUS, 0.0f));
  bounds_.points.push_back(SPHERE_CENTRE + Vec3(0.0f, -SPHERE_RADIUS, 0.0f));
  bounds_.points.push_back(SPHERE_CENTRE + Vec3(0.0f, 0.0f, SPHERE_RADIUS));
  bounds_.points.push_back(SPHERE_CENTRE + Vec3(0.0f, 0.0f, -SPHERE_RADIUS));
}

bool DemoWorld::intersectSphere(const engine::Ray& ray, float minimum, float maximum, Hit& hit) const {
  const Vec3 offset = ray.origin - SPHERE_CENTRE;
  const float a = engine::dot(ray.direction, ray.direction);
  const float halfB = engine::dot(offset, ray.direction);
  const float c = engine::dot(offset, offset) - SPHERE_RADIUS * SPHERE_RADIUS;
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
  hit.normal = engine::normalise(hit.point - SPHERE_CENTRE);
  hit.surface = Surface::Sphere;
  return true;
}

bool DemoWorld::intersectCube(const engine::Ray& ray, float minimum, float maximum, Hit& hit) const {
  float nearT = minimum;
  float farT = maximum;
  const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
  const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
  const float low[3] = {CUBE_MIN.x, CUBE_MIN.y, CUBE_MIN.z};
  const float high[3] = {CUBE_MAX.x, CUBE_MAX.y, CUBE_MAX.z};

  for (int axis = 0; axis < 3; ++axis) {
    if (std::fabs(direction[axis]) < 1e-7f) {
      if (origin[axis] < low[axis] || origin[axis] > high[axis]) return false;
      continue;
    }

    const float inverse = 1.0f / direction[axis];
    float t0 = (low[axis] - origin[axis]) * inverse;
    float t1 = (high[axis] - origin[axis]) * inverse;
    if (t0 > t1) std::swap(t0, t1);
    nearT = std::max(nearT, t0);
    farT = std::min(farT, t1);
    if (farT < nearT) return false;
  }

  hit.found = true;
  hit.t = nearT;
  hit.point = ray.origin + ray.direction * nearT;
  hit.surface = Surface::Cube;

  float best = std::fabs(hit.point.x - CUBE_MIN.x);
  hit.normal = {-1.0f, 0.0f, 0.0f};
  const auto choose = [&](float distance, Vec3 normal) {
    if (distance < best) {
      best = distance;
      hit.normal = normal;
    }
  };

  choose(std::fabs(hit.point.x - CUBE_MAX.x), {1.0f, 0.0f, 0.0f});
  choose(std::fabs(hit.point.y - CUBE_MIN.y), {0.0f, -1.0f, 0.0f});
  choose(std::fabs(hit.point.y - CUBE_MAX.y), {0.0f, 1.0f, 0.0f});
  choose(std::fabs(hit.point.z - CUBE_MIN.z), {0.0f, 0.0f, -1.0f});
  choose(std::fabs(hit.point.z - CUBE_MAX.z), {0.0f, 0.0f, 1.0f});
  return true;
}

bool DemoWorld::intersectGround(const engine::Ray& ray, float minimum, float maximum, Hit& hit) const {
  if (std::fabs(ray.direction.z) < 1e-7f) return false;

  const float t = -ray.origin.z / ray.direction.z;
  if (t < minimum || t > maximum) return false;

  const Vec3 point = ray.origin + ray.direction * t;
  if (std::fabs(point.x) > GROUND_LIMIT || std::fabs(point.y) > GROUND_LIMIT) return false;

  hit.found = true;
  hit.t = t;
  hit.point = point;
  hit.normal = {0.0f, 0.0f, 1.0f};
  hit.surface = Surface::Ground;
  return true;
}

DemoWorld::Hit DemoWorld::traceClosest(const engine::Ray& ray, float minimum, float maximum) const {
  Hit output;
  Hit hit;

  if (intersectCube(ray, minimum, maximum, hit)) {
    output = hit;
    maximum = hit.t;
  }

  hit = Hit();
  if (intersectSphere(ray, minimum, maximum, hit)) {
    output = hit;
    maximum = hit.t;
  }

  hit = Hit();
  if (intersectGround(ray, minimum, maximum, hit)) output = hit;
  return output;
}

bool DemoWorld::occluded(Vec3 point, Vec3 normal) const {
  const Vec3 toLight = LIGHT_POSITION - point;
  const float distance = engine::length(toLight);
  return traceClosest(
    {point + normal * EPSILON, toLight / distance},
    EPSILON,
    distance - EPSILON
  ).found;
}

Vec3 DemoWorld::material(const Hit& hit) const {
  if (hit.surface == Surface::Cube) return {0.18f, 0.48f, 0.88f};
  if (hit.surface == Surface::Sphere) return {0.95f, 0.43f, 0.12f};

  const int x = static_cast<int>(std::floor(hit.point.x + 20.0f));
  const int y = static_cast<int>(std::floor(hit.point.y + 20.0f));
  const float brightness = ((x + y) & 1) ? 0.63f : 0.69f;
  return {brightness * 0.90f, brightness * 0.96f, brightness};
}

Vec3 DemoWorld::shade(const Hit& hit) const {
  const Vec3 base = material(hit);
  const Vec3 toLight = LIGHT_POSITION - hit.point;
  const float distance = engine::length(toLight);
  const float diffuse = std::max(0.0f, engine::dot(hit.normal, toLight / distance));
  const float attenuation = 1.0f / (1.0f + 0.018f * distance * distance);
  const float visibility = occluded(hit.point, hit.normal) ? 0.0f : 1.0f;
  Vec3 colour = base * (0.19f + visibility * diffuse * attenuation * 1.18f);

  if (hit.surface == Surface::Ground) {
    const float edgeX = std::fabs(hit.point.x - std::round(hit.point.x));
    const float edgeY = std::fabs(hit.point.y - std::round(hit.point.y));
    if (std::min(edgeX, edgeY) < 0.022f) colour = colour * 0.78f;
  }

  return {
    std::min(colour.x, 1.0f),
    std::min(colour.y, 1.0f),
    std::min(colour.z, 1.0f)
  };
}

Vec3 DemoWorld::background(float y) const {
  const float t = std::max(0.0f, std::min(1.0f, y));
  return Vec3(0.075f, 0.12f, 0.18f) * (1.0f - t) + Vec3(0.20f, 0.28f, 0.34f) * t;
}

Vec3 DemoWorld::sample(const engine::Ray& ray, float backgroundY) const {
  const Hit hit = traceClosest(ray, EPSILON, FAR_DISTANCE);
  return hit.found ? shade(hit) : background(backgroundY);
}

} // namespace demo
} // namespace isoweb
