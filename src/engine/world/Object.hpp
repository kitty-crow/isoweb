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
  // Non-empty while the entity physically occupies a connector between
  // levels/worlds. levelId remains the coordinate frame currently used for
  // simulation; liminalObjectId is the authoritative spatial membership.
  std::string liminalObjectId;

  bool sharesSpaceWith(const EntityLocation& other) const {
    const auto compatible = [](const std::string& left, const std::string& right) {
      return left.empty() || right.empty() || left == right;
    };

    if (!liminalObjectId.empty() || !other.liminalObjectId.empty()) {
      return !liminalObjectId.empty() &&
        liminalObjectId == other.liminalObjectId &&
        compatible(timelineId, other.timelineId);
    }

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
  Left,
  Right,
  Back,
  Front,
  Bottom,
  Top
};

struct ObjectRayHit {
  float distance = 0.0f;
  Vec3 worldPoint;
  Vec3 localPoint;
  Vec3 worldNormal;
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

  bool blocks(const HitBox& other) const {
    return solid && hitBox.intersects(other);
  }

  void horizontalBasis(Vec3& facing, Vec3& right) const {
    // Forward is public because entities are intentionally lightweight, so
    // cache against the source components instead of relying on an explicit
    // invalidation call. Movement can change facing at any time; the next
    // access refreshes once and subsequent ray/collision operations are free
    // of normalisation until x/y changes again.
    if (!basisCacheValid_ || cachedForwardX_ != forward.x || cachedForwardY_ != forward.y) {
      cachedFacing_ = normalisedHorizontal(forward);
      cachedRight_ = {cachedFacing_.y, -cachedFacing_.x, 0.0f};
      cachedForwardX_ = forward.x;
      cachedForwardY_ = forward.y;
      basisCacheValid_ = true;
    }
    facing = cachedFacing_;
    right = cachedRight_;
  }

  Vec3 horizontalForward() const {
    Vec3 facing;
    Vec3 right;
    horizontalBasis(facing, right);
    return facing;
  }

  Vec3 horizontalRight() const {
    Vec3 facing;
    Vec3 right;
    horizontalBasis(facing, right);
    return right;
  }

  Vec3 localToWorld(const Vec3& local) const {
    Vec3 facing;
    Vec3 right;
    horizontalBasis(facing, right);
    return location.position + right * local.x + facing * local.y + Vec3(0.0f, 0.0f, local.z);
  }

  Vec3 worldToLocal(const Vec3& world) const {
    Vec3 facing;
    Vec3 right;
    horizontalBasis(facing, right);
    const Vec3 delta = world - location.position;
    return {dot2(delta, right), dot2(delta, facing), delta.z};
  }

  bool intersectRay(const Ray& ray, float minimum, float maximum, ObjectRayHit& hit) const {
    Vec3 facing;
    Vec3 right;
    horizontalBasis(facing, right);
    const Vec3 relativeOrigin = ray.origin - location.position;
    const Vec3 localOrigin(dot2(relativeOrigin, right), dot2(relativeOrigin, facing), relativeOrigin.z);
    const Vec3 localDirection(dot2(ray.direction, right), dot2(ray.direction, facing), ray.direction.z);

    float nearT = minimum;
    float farT = maximum;
    int nearAxis = -1;
    float nearSign = 0.0f;
    const float origins[3] = {localOrigin.x, localOrigin.y, localOrigin.z};
    const float directions[3] = {localDirection.x, localDirection.y, localDirection.z};
    const float mins[3] = {hitBox.minimum.x, hitBox.minimum.y, hitBox.minimum.z};
    const float maxs[3] = {hitBox.maximum.x, hitBox.maximum.y, hitBox.maximum.z};

    for (int axis = 0; axis < 3; ++axis) {
      if (std::fabs(directions[axis]) < 1e-7f) {
        if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return false;
        continue;
      }
      const float inverse = 1.0f / directions[axis];
      float t0 = (mins[axis] - origins[axis]) * inverse;
      float t1 = (maxs[axis] - origins[axis]) * inverse;
      float sign = -1.0f;
      if (t0 > t1) {
        std::swap(t0, t1);
        sign = 1.0f;
      }
      if (t0 > nearT) {
        nearT = t0;
        nearAxis = axis;
        nearSign = sign;
      }
      farT = std::min(farT, t1);
      if (farT < nearT) return false;
    }

    if (nearAxis < 0 || nearT < minimum || nearT > maximum) return false;

    hit.distance = nearT;
    hit.worldPoint = ray.origin + ray.direction * nearT;
    hit.localPoint = localOrigin + localDirection * nearT;
    if (nearAxis == 0) {
      hit.worldNormal = right * nearSign;
      hit.face = nearSign < 0.0f ? ObjectFace::Left : ObjectFace::Right;
    } else if (nearAxis == 1) {
      hit.worldNormal = facing * nearSign;
      hit.face = nearSign < 0.0f ? ObjectFace::Back : ObjectFace::Front;
    } else {
      hit.worldNormal = {0.0f, 0.0f, nearSign};
      hit.face = nearSign < 0.0f ? ObjectFace::Bottom : ObjectFace::Top;
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

  static Vec3 normalisedHorizontal(const Vec3& value) {
    const float magnitudeSquared = value.x * value.x + value.y * value.y;
    if (magnitudeSquared <= 1e-14f) return {0.0f, 1.0f, 0.0f};
    const float inverseMagnitude = 1.0f / std::sqrt(magnitudeSquared);
    return {value.x * inverseMagnitude, value.y * inverseMagnitude, 0.0f};
  }

  static float projectionRadius(const OrientedBox& box, const Vec3& axis) {
    return box.halfX * std::fabs(dot2(box.right, axis)) +
      box.halfY * std::fabs(dot2(box.forward, axis));
  }

  OrientedBox orientedBox() const {
    Vec3 facing;
    Vec3 right;
    horizontalBasis(facing, right);
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

  mutable bool basisCacheValid_ = false;
  mutable float cachedForwardX_ = 0.0f;
  mutable float cachedForwardY_ = 0.0f;
  mutable Vec3 cachedFacing_ = {0.0f, 1.0f, 0.0f};
  mutable Vec3 cachedRight_ = {1.0f, 0.0f, 0.0f};
};

} // namespace engine
} // namespace isoweb
