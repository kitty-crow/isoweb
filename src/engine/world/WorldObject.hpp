#pragma once

#include "engine/math/Vec3.hpp"

namespace isoweb {
namespace engine {

struct HitBox {
  Vec3 minimum;
  Vec3 maximum;

  bool intersects(const HitBox& other) const {
    return minimum.x <= other.maximum.x && maximum.x >= other.minimum.x &&
      minimum.y <= other.maximum.y && maximum.y >= other.minimum.y &&
      minimum.z <= other.maximum.z && maximum.z >= other.minimum.z;
  }

  bool contains(const Vec3& point) const {
    return point.x >= minimum.x && point.x <= maximum.x &&
      point.y >= minimum.y && point.y <= maximum.y &&
      point.z >= minimum.z && point.z <= maximum.z;
  }
};

struct WorldObject {
  bool solid = true;
  HitBox hitBox;

  bool blocks(const HitBox& other) const {
    return solid && hitBox.intersects(other);
  }
};

} // namespace engine
} // namespace isoweb
