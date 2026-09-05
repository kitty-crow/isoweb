#pragma once

#include "engine/world/IWorld.hpp"

namespace isoweb {
namespace demo {

class DemoWorld final : public engine::IWorld {
public:
  DemoWorld();

  const engine::WorldBounds& bounds() const override { return bounds_; }
  engine::Vec3 sample(const engine::Ray& ray, float backgroundY) const override;

private:
  enum class Surface {
    None,
    Ground,
    Cube,
    Sphere
  };

  struct Hit {
    bool found = false;
    float t = 1000.0f;
    engine::Vec3 point;
    engine::Vec3 normal;
    Surface surface = Surface::None;
  };

  bool intersectSphere(const engine::Ray& ray, float minimum, float maximum, Hit& hit) const;
  bool intersectCube(const engine::Ray& ray, float minimum, float maximum, Hit& hit) const;
  bool intersectGround(const engine::Ray& ray, float minimum, float maximum, Hit& hit) const;
  Hit traceClosest(const engine::Ray& ray, float minimum, float maximum) const;
  bool occluded(engine::Vec3 point, engine::Vec3 normal) const;
  engine::Vec3 material(const Hit& hit) const;
  engine::Vec3 shade(const Hit& hit) const;
  engine::Vec3 background(float y) const;

  engine::WorldBounds bounds_;
};

} // namespace demo
} // namespace isoweb
