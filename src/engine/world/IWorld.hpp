#pragma once

#include <cstddef>
#include <vector>

#include "engine/math/Vec3.hpp"
#include "engine/render/Ray.hpp"
#include "engine/world/WorldObject.hpp"

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
  virtual const std::vector<Object>& objects() const = 0;
  virtual bool intersectsSolid(const HitBox& hitBox) const = 0;
  virtual bool collidesWith(const Object& candidate) const = 0;

  virtual std::size_t levelCount() const = 0;
  virtual std::size_t activeLevelIndex() const = 0;
  virtual std::size_t defaultLevelIndex() const = 0;
};

} // namespace engine
} // namespace isoweb
