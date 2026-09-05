#pragma once

#include <vector>

#include "engine/math/Vec3.hpp"
#include "engine/render/Ray.hpp"

namespace isoweb {
namespace engine {

struct WorldBounds {
  Vec3 focus;
  std::vector<Vec3> points;
};

class IWorld {
public:
  virtual ~IWorld() = default;

  virtual const WorldBounds& bounds() const = 0;
  virtual Vec3 sample(const Ray& ray, float backgroundY) const = 0;
};

} // namespace engine
} // namespace isoweb
