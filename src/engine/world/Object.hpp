#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "engine/math/Vec3.hpp"

namespace isoweb {
namespace engine {

struct EntityLocation {
  std::string worldId;
  std::string timelineId;
  std::string levelId;
  Vec3 position;

  bool sharesSpaceWith(const EntityLocation& other) const {
    const auto compatible = [](const std::string& left, const std::string& right) {
      return left.empty() || right.empty() || left == right;
    };

    return compatible(worldId, other.worldId) &&
      compatible(timelineId, other.timelineId) &&
      compatible(levelId, other.levelId);
  }
};

struct HitBox {
  Vec3 minimum;
  Vec3 maximum;

  Vec3 centre() const {
    return (minimum + maximum) * 0.5f;
  }

  Vec3 halfExtent() const {
    return (maximum - minimum) * 0.5f;
  }

  Vec3 size() const {
    return maximum - minimum;
  }

  bool intersects(const HitBox& other) const {
    return minimum.x < other.maximum.x && maximum.x > other.minimum.x &&
      minimum.y < other.maximum.y && maximum.y > other.minimum.y &&
      minimum.z < other.maximum.z && maximum.z > other.minimum.z;
  }

  bool contains(const Vec3& point) const {
    return point.x >= minimum.x && point.x <= maximum.x &&
      point.y >= minimum.y && point.y <= maximum.y &&
      point.z >= minimum.z && point.z <= maximum.z;
  }
};

class Object {
public:
  virtual ~Object() = default;

  std::string id;
  EntityLocation location;
  Vec3 forward = {0.0f, 1.0f, 0.0f};
  HitBox hitBox;

  bool solid = true;
  std::vector<std::string> collisionTags;
  std::vector<std::string> mustCollideWith;

  bool hasCollisionTag(const std::string& tag) const {
    return std::find(collisionTags.begin(), collisionTags.end(), tag) != collisionTags.end();
  }

  bool matchesCollisionSelector(const std::string& selector) const {
    return (!id.empty() && selector == id) || hasCollisionTag(selector);
  }

  bool demandsCollisionWith(const Object& other) const {
    for (const std::string& selector : mustCollideWith) {
      if (other.matchesCollisionSelector(selector)) return true;
    }
    return false;
  }

  bool collisionEnabledWith(const Object& other) const {
    return (solid && other.solid) ||
      demandsCollisionWith(other) ||
      other.demandsCollisionWith(*this);
  }

  bool overlaps(const Object& other) const {
    if (!location.sharesSpaceWith(other.location)) return false;

    const OrientedBox a = orientedBox();
    const OrientedBox b = other.orientedBox();

    if (std::fabs(a.centre.z - b.centre.z) >= a.halfZ + b.halfZ) return false;

    const Vec3 axes[4] = {a.right, a.forward, b.right, b.forward};
    const Vec3 delta = b.centre - a.centre;

    for (const Vec3& axis : axes) {
      const float centreDistance = std::fabs(dot2(delta, axis));
      const float radiusA = projectionRadius(a, axis);
      const float radiusB = projectionRadius(b, axis);
      if (centreDistance >= radiusA + radiusB) return false;
    }

    return true;
  }

  bool blocks(const Object& other) const {
    return collisionEnabledWith(other) && overlaps(other);
  }

  // Compatibility helper for the existing demo collision query. New stateful
  // entities should use Object-vs-Object collision so location, orientation,
  // solidity, and must-collide overrides all participate.
  bool blocks(const HitBox& other) const {
    return solid && hitBox.intersects(other);
  }

private:
  struct OrientedBox {
    Vec3 centre;
    Vec3 right;
    Vec3 forward;
    float halfX;
    float halfY;
    float halfZ;
  };

  static float dot2(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y;
  }

  static Vec3 horizontalForward(const Vec3& value) {
    const float magnitude = std::sqrt(value.x * value.x + value.y * value.y);
    return magnitude > 1e-7f
      ? Vec3(value.x / magnitude, value.y / magnitude, 0.0f)
      : Vec3(0.0f, 1.0f, 0.0f);
  }

  static float projectionRadius(const OrientedBox& box, const Vec3& axis) {
    return box.halfX * std::fabs(dot2(box.right, axis)) +
      box.halfY * std::fabs(dot2(box.forward, axis));
  }

  OrientedBox orientedBox() const {
    const Vec3 facing = horizontalForward(forward);
    const Vec3 right(facing.y, -facing.x, 0.0f);
    const Vec3 localCentre = hitBox.centre();
    const Vec3 half = hitBox.halfExtent();

    OrientedBox box;
    box.right = right;
    box.forward = facing;
    box.halfX = std::fabs(half.x);
    box.halfY = std::fabs(half.y);
    box.halfZ = std::fabs(half.z);
    box.centre = location.position +
      right * localCentre.x +
      facing * localCentre.y +
      Vec3(0.0f, 0.0f, localCentre.z);
    return box;
  }
};

} // namespace engine
} // namespace isoweb
