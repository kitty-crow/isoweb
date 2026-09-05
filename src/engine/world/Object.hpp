#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "engine/math/Vec3.hpp"
#include "engine/render/Ray.hpp"

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

enum class ObjectFace {
  Front,
  Back,
  Left,
  Right,
  Top,
  Bottom
};

struct ObjectRayHit {
  bool found = false;
  float t = 0.0f;
  Vec3 point;
  Vec3 localPoint;
  Vec3 normal;
  ObjectFace face = ObjectFace::Front;
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

  // Intersects a world-space ray with this object's oriented local hitbox.
  // This is useful for picking and for the no-artwork character fallback.
  bool rayIntersection(
    const Ray& ray,
    float minimum,
    float maximum,
    ObjectRayHit& hit
  ) const {
    const Vec3 facing = horizontalForward(forward);
    const Vec3 right(facing.y, -facing.x, 0.0f);
    const Vec3 relativeOrigin = ray.origin - location.position;
    const Vec3 localOrigin(
      dot2(relativeOrigin, right),
      dot2(relativeOrigin, facing),
      relativeOrigin.z
    );
    const Vec3 localDirection(
      dot2(ray.direction, right),
      dot2(ray.direction, facing),
      ray.direction.z
    );

    const float origin[3] = {localOrigin.x, localOrigin.y, localOrigin.z};
    const float direction[3] = {localDirection.x, localDirection.y, localDirection.z};
    const float boundsMinimum[3] = {hitBox.minimum.x, hitBox.minimum.y, hitBox.minimum.z};
    const float boundsMaximum[3] = {hitBox.maximum.x, hitBox.maximum.y, hitBox.maximum.z};

    float nearT = minimum;
    float farT = maximum;
    int nearAxis = -1;
    float nearSign = 0.0f;
    int farAxis = -1;
    float farSign = 0.0f;

    for (int axis = 0; axis < 3; ++axis) {
      if (std::fabs(direction[axis]) < 1e-7f) {
        if (origin[axis] < boundsMinimum[axis] || origin[axis] > boundsMaximum[axis]) {
          return false;
        }
        continue;
      }

      const float inverse = 1.0f / direction[axis];
      float t0 = (boundsMinimum[axis] - origin[axis]) * inverse;
      float t1 = (boundsMaximum[axis] - origin[axis]) * inverse;
      float sign0 = -1.0f;
      float sign1 = 1.0f;

      if (t0 > t1) {
        std::swap(t0, t1);
        std::swap(sign0, sign1);
      }

      if (t0 > nearT) {
        nearT = t0;
        nearAxis = axis;
        nearSign = sign0;
      }
      if (t1 < farT) {
        farT = t1;
        farAxis = axis;
        farSign = sign1;
      }
      if (farT < nearT) return false;
    }

    float t = nearT;
    int axis = nearAxis;
    float sign = nearSign;
    if (axis < 0) {
      t = farT;
      axis = farAxis;
      sign = farSign;
    }
    if (axis < 0 || t < minimum || t > maximum) return false;

    const Vec3 localPoint = localOrigin + localDirection * t;
    Vec3 localNormal;
    if (axis == 0) localNormal = {sign, 0.0f, 0.0f};
    else if (axis == 1) localNormal = {0.0f, sign, 0.0f};
    else localNormal = {0.0f, 0.0f, sign};

    hit.found = true;
    hit.t = t;
    hit.point = ray.origin + ray.direction * t;
    hit.localPoint = localPoint;
    hit.normal = right * localNormal.x +
      facing * localNormal.y +
      Vec3(0.0f, 0.0f, localNormal.z);

    if (axis == 0) {
      hit.face = sign > 0.0f ? ObjectFace::Right : ObjectFace::Left;
    } else if (axis == 1) {
      hit.face = sign > 0.0f ? ObjectFace::Front : ObjectFace::Back;
    } else {
      hit.face = sign > 0.0f ? ObjectFace::Top : ObjectFace::Bottom;
    }
    return true;
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
