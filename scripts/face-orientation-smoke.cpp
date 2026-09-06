#include <cmath>
#include <cstdlib>
#include <iostream>

#include "engine/world/Object.hpp"

using isoweb::engine::Object;
using isoweb::engine::ObjectFace;
using isoweb::engine::ObjectRayHit;
using isoweb::engine::Ray;
using isoweb::engine::Vec3;

namespace {

bool close(float a, float b) {
  return std::fabs(a - b) < 1e-5f;
}

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "[face-orientation] " << message << '\n';
    std::exit(1);
  }
}

ObjectRayHit cast(const Object& object, const Ray& ray, ObjectFace expected) {
  ObjectRayHit hit;
  require(object.intersectRay(ray, 0.001f, 100.0f, hit), "ray missed debug Character box");
  require(hit.face == expected, "ray resolved the wrong Character face");
  return hit;
}

} // namespace

int main() {
  Object object;
  object.location.position = {0.0f, 0.0f, 0.0f};
  object.forward = {0.0f, 1.0f, 0.0f};
  object.hitBox.minimum = {-1.0f, -2.0f, 0.0f};
  object.hitBox.maximum = {1.0f, 2.0f, 3.0f};

  // Back is the known-good reference: +X must remain +X when read from behind.
  const ObjectRayHit back = cast(object, {{0.4f, -5.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, ObjectFace::Back);
  require(close(back.geometricLocalPoint.x, 0.4f), "back geometric X changed");
  require(close(back.localPoint.x, 0.4f), "back skin was mirrored");

  // From the front, an outward-facing label sees Character-local X reversed.
  const ObjectRayHit front = cast(object, {{0.4f, 5.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}, ObjectFace::Front);
  require(close(front.geometricLocalPoint.x, 0.4f), "front geometric X changed");
  require(close(front.localPoint.x, -0.4f), "front skin is not outward-facing");

  // Right-side screen-right follows +Y; left-side screen-right follows -Y.
  const ObjectRayHit right = cast(object, {{5.0f, 0.7f, 1.0f}, {-1.0f, 0.0f, 0.0f}}, ObjectFace::Right);
  require(close(right.geometricLocalPoint.y, 0.7f), "right geometric Y changed");
  require(close(right.localPoint.y, 0.7f), "right skin was mirrored");

  const ObjectRayHit left = cast(object, {{-5.0f, 0.7f, 1.0f}, {1.0f, 0.0f, 0.0f}}, ObjectFace::Left);
  require(close(left.geometricLocalPoint.y, 0.7f), "left geometric Y changed");
  require(close(left.localPoint.y, -0.7f), "left skin is not outward-facing");

  // Top retains the authored XY frame. Bottom reverses X to keep the skin
  // outward-facing rather than displaying the underside as a mirror image.
  const ObjectRayHit top = cast(object, {{0.4f, 0.7f, 6.0f}, {0.0f, 0.0f, -1.0f}}, ObjectFace::Top);
  require(close(top.geometricLocalPoint.x, 0.4f), "top geometric X changed");
  require(close(top.localPoint.x, 0.4f), "top skin was mirrored");

  const ObjectRayHit bottom = cast(object, {{0.4f, 0.7f, -3.0f}, {0.0f, 0.0f, 1.0f}}, ObjectFace::Bottom);
  require(close(bottom.geometricLocalPoint.x, 0.4f), "bottom geometric X changed");
  require(close(bottom.localPoint.x, -0.4f), "bottom skin is not outward-facing");

  std::cout << "Character face orientation smoke passed: F/R/L/B/T/Bt use outward-readable UV handedness.\n";
  return 0;
}
