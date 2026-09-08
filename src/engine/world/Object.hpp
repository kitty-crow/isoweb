#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
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

    // Two entities simultaneously inside liminal space only share space when
    // they occupy the same physical connector. If only one entity is liminal,
    // preserve collisions with ordinary occupants of its current endpoint
    // level rather than making the connector an isolation bubble.
    if (!liminalObjectId.empty() && !other.liminalObjectId.empty()) {
      return liminalObjectId == other.liminalObjectId &&
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

enum class SurfaceTextureMode {
  Stretch,
  TileLocal,
  TileWorld
};

struct SurfaceUV {
  float u = 0.0f;
  float v = 0.0f;
};

struct ObjectRayHit {
  float distance = 0.0f;
  Vec3 worldPoint;
  // Geometrically exact object-local point.
  Vec3 geometricLocalPoint;
  // Object-local surface coordinates oriented so artwork/text reads correctly
  // when viewed from outside that face. This is what debug face skins sample.
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

  // Runtime posture/deformation may use a different collision shape while the
  // authored hitBox remains the stable standing/base geometry.
  virtual HitBox collisionHitBox() const { return hitBox; }

  // Surface textures are metadata today and are deliberately independent of
  // hit-box dimensions. Tile modes therefore keep a fixed world-space texel
  // density when an obstacle changes length instead of stretching one image
  // across the resized face.
  SurfaceTextureMode surfaceTextureMode = SurfaceTextureMode::Stretch;
  float textureWorldUnitsPerTile = 1.0f;

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
      rayDirectionCacheValid_ = false;
      rayProjectionCacheValid_ = false;
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

  SurfaceUV surfaceTextureUV(const ObjectRayHit& hit) const {
    auto normalisedAxis = [](float value, float minimum, float maximum) {
      const float range = maximum - minimum;
      return range > 1e-7f ? (value - minimum) / range : 0.5f;
    };

    float u = 0.5f;
    float v = 0.5f;
    if (surfaceTextureMode == SurfaceTextureMode::Stretch) {
      switch (hit.face) {
        case ObjectFace::Front:
        case ObjectFace::Back:
          u = normalisedAxis(hit.localPoint.x, hitBox.minimum.x, hitBox.maximum.x);
          v = 1.0f - normalisedAxis(hit.localPoint.z, hitBox.minimum.z, hitBox.maximum.z);
          break;
        case ObjectFace::Left:
        case ObjectFace::Right:
          u = normalisedAxis(hit.localPoint.y, hitBox.minimum.y, hitBox.maximum.y);
          v = 1.0f - normalisedAxis(hit.localPoint.z, hitBox.minimum.z, hitBox.maximum.z);
          break;
        case ObjectFace::Top:
        case ObjectFace::Bottom:
          u = normalisedAxis(hit.localPoint.x, hitBox.minimum.x, hitBox.maximum.x);
          v = 1.0f - normalisedAxis(hit.localPoint.y, hitBox.minimum.y, hitBox.maximum.y);
          break;
      }
      return {u, v};
    }

    const float tileSize = std::max(0.001f, textureWorldUnitsPerTile);
    if (surfaceTextureMode == SurfaceTextureMode::TileWorld) {
      Vec3 facing;
      Vec3 right;
      horizontalBasis(facing, right);
      switch (hit.face) {
        case ObjectFace::Front:
        case ObjectFace::Back:
          u = dot2(hit.worldPoint, right) / tileSize;
          v = -hit.worldPoint.z / tileSize;
          break;
        case ObjectFace::Left:
        case ObjectFace::Right:
          u = dot2(hit.worldPoint, facing) / tileSize;
          v = -hit.worldPoint.z / tileSize;
          break;
        case ObjectFace::Top:
        case ObjectFace::Bottom:
          u = dot2(hit.worldPoint, right) / tileSize;
          v = -dot2(hit.worldPoint, facing) / tileSize;
          break;
      }
    } else {
      switch (hit.face) {
        case ObjectFace::Front:
        case ObjectFace::Back:
          u = hit.localPoint.x / tileSize;
          v = -hit.localPoint.z / tileSize;
          break;
        case ObjectFace::Left:
        case ObjectFace::Right:
          u = hit.localPoint.y / tileSize;
          v = -hit.localPoint.z / tileSize;
          break;
        case ObjectFace::Top:
        case ObjectFace::Bottom:
          u = hit.localPoint.x / tileSize;
          v = -hit.localPoint.y / tileSize;
          break;
      }
    }

    u -= std::floor(u);
    v -= std::floor(v);
    return {u, v};
  }

  // Cheap conservative screen-space reject for parallel camera rays. The
  // projection is cached per object/direction and is rebuilt only when that
  // object actually moves, rotates or resizes. A scene with many dynamic
  // objects can therefore reject almost all per-pixel object tests with four
  // comparisons instead of running the full three-axis slab intersection.
  bool rayMayHit(const Ray& ray) const {
    Vec3 facing;
    Vec3 right;
    horizontalBasis(facing, right);

    const bool geometryChanged =
      !rayProjectionCacheValid_ ||
      cachedProjectionPosition_.x != location.position.x ||
      cachedProjectionPosition_.y != location.position.y ||
      cachedProjectionPosition_.z != location.position.z ||
      cachedProjectionMinimum_.x != hitBox.minimum.x ||
      cachedProjectionMinimum_.y != hitBox.minimum.y ||
      cachedProjectionMinimum_.z != hitBox.minimum.z ||
      cachedProjectionMaximum_.x != hitBox.maximum.x ||
      cachedProjectionMaximum_.y != hitBox.maximum.y ||
      cachedProjectionMaximum_.z != hitBox.maximum.z ||
      cachedProjectionRayDirection_.x != ray.direction.x ||
      cachedProjectionRayDirection_.y != ray.direction.y ||
      cachedProjectionRayDirection_.z != ray.direction.z;

    if (geometryChanged) {
      Vec3 projectedRight = cross({0.0f, 0.0f, 1.0f}, ray.direction);
      const float projectedRightLengthSquared = dot(projectedRight, projectedRight);
      if (projectedRightLengthSquared <= 1e-12f) {
        projectedRight = {1.0f, 0.0f, 0.0f};
      } else {
        projectedRight = projectedRight / std::sqrt(projectedRightLengthSquared);
      }
      Vec3 projectedUp = cross(projectedRight, ray.direction);
      const float projectedUpLengthSquared = dot(projectedUp, projectedUp);
      if (projectedUpLengthSquared <= 1e-12f) return true;
      projectedUp = projectedUp / std::sqrt(projectedUpLengthSquared);

      float minimumX = std::numeric_limits<float>::max();
      float minimumY = std::numeric_limits<float>::max();
      float maximumX = -std::numeric_limits<float>::max();
      float maximumY = -std::numeric_limits<float>::max();
      for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
          for (int z = 0; z < 2; ++z) {
            const Vec3 local(
              x ? hitBox.maximum.x : hitBox.minimum.x,
              y ? hitBox.maximum.y : hitBox.minimum.y,
              z ? hitBox.maximum.z : hitBox.minimum.z
            );
            const Vec3 point = location.position +
              right * local.x + facing * local.y + Vec3(0.0f, 0.0f, local.z);
            const float screenX = dot(point, projectedRight);
            const float screenY = dot(point, projectedUp);
            minimumX = std::min(minimumX, screenX);
            minimumY = std::min(minimumY, screenY);
            maximumX = std::max(maximumX, screenX);
            maximumY = std::max(maximumY, screenY);
          }
        }
      }

      cachedProjectionPosition_ = location.position;
      cachedProjectionMinimum_ = hitBox.minimum;
      cachedProjectionMaximum_ = hitBox.maximum;
      cachedProjectionRayDirection_ = ray.direction;
      cachedProjectionRight_ = projectedRight;
      cachedProjectionUp_ = projectedUp;
      cachedProjectionMinX_ = minimumX;
      cachedProjectionMinY_ = minimumY;
      cachedProjectionMaxX_ = maximumX;
      cachedProjectionMaxY_ = maximumY;
      rayProjectionCacheValid_ = true;
    }

    const float screenX = dot(ray.origin, cachedProjectionRight_);
    const float screenY = dot(ray.origin, cachedProjectionUp_);
    constexpr float epsilon = 1e-5f;
    return screenX >= cachedProjectionMinX_ - epsilon &&
      screenX <= cachedProjectionMaxX_ + epsilon &&
      screenY >= cachedProjectionMinY_ - epsilon &&
      screenY <= cachedProjectionMaxY_ + epsilon;
  }

  bool intersectRay(const Ray& ray, float minimum, float maximum, ObjectRayHit& hit) const {
    if (!rayMayHit(ray)) return false;

    Vec3 facing;
    Vec3 right;
    horizontalBasis(facing, right);
    const Vec3 relativeOrigin = ray.origin - location.position;
    const Vec3 localOrigin(dot2(relativeOrigin, right), dot2(relativeOrigin, facing), relativeOrigin.z);

    if (!rayDirectionCacheValid_ ||
        cachedRayDirection_.x != ray.direction.x ||
        cachedRayDirection_.y != ray.direction.y ||
        cachedRayDirection_.z != ray.direction.z) {
      cachedRayDirection_ = ray.direction;
      cachedLocalRayDirection_ = {
        dot2(ray.direction, right),
        dot2(ray.direction, facing),
        ray.direction.z
      };
      const float directions[3] = {
        cachedLocalRayDirection_.x,
        cachedLocalRayDirection_.y,
        cachedLocalRayDirection_.z
      };
      for (int axis = 0; axis < 3; ++axis) {
        cachedRayParallel_[axis] = std::fabs(directions[axis]) < 1e-7f;
        cachedRayInverse_[axis] = cachedRayParallel_[axis] ? 0.0f : 1.0f / directions[axis];
      }
      rayDirectionCacheValid_ = true;
    }

    const Vec3& localDirection = cachedLocalRayDirection_;
    float nearT = minimum;
    float farT = maximum;
    int nearAxis = -1;
    float nearSign = 0.0f;
    const float origins[3] = {localOrigin.x, localOrigin.y, localOrigin.z};
    const float mins[3] = {hitBox.minimum.x, hitBox.minimum.y, hitBox.minimum.z};
    const float maxs[3] = {hitBox.maximum.x, hitBox.maximum.y, hitBox.maximum.z};

    for (int axis = 0; axis < 3; ++axis) {
      if (cachedRayParallel_[axis]) {
        if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return false;
        continue;
      }
      const float inverse = cachedRayInverse_[axis];
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
    hit.geometricLocalPoint = localOrigin + localDirection * nearT;
    hit.localPoint = hit.geometricLocalPoint;
    if (nearAxis == 0) {
      hit.worldNormal = right * nearSign;
      hit.face = nearSign < 0.0f ? ObjectFace::Left : ObjectFace::Right;
      // Looking inward from the object's left side reverses local +Y on screen.
      if (hit.face == ObjectFace::Left) {
        hit.localPoint.y = hitBox.minimum.y + hitBox.maximum.y - hit.localPoint.y;
      }
    } else if (nearAxis == 1) {
      hit.worldNormal = facing * nearSign;
      hit.face = nearSign < 0.0f ? ObjectFace::Back : ObjectFace::Front;
      // Back already reads correctly; the outward-facing front reverses +X.
      if (hit.face == ObjectFace::Front) {
        hit.localPoint.x = hitBox.minimum.x + hitBox.maximum.x - hit.localPoint.x;
      }
    } else {
      hit.worldNormal = {0.0f, 0.0f, nearSign};
      hit.face = nearSign < 0.0f ? ObjectFace::Bottom : ObjectFace::Top;
      // Keep +Y as glyph-up on both horizontal faces; the underside needs X
      // reversed to retain an outward-facing, readable coordinate frame.
      if (hit.face == ObjectFace::Bottom) {
        hit.localPoint.x = hitBox.minimum.x + hitBox.maximum.x - hit.localPoint.x;
      }
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

  mutable bool rayDirectionCacheValid_ = false;
  mutable Vec3 cachedRayDirection_;
  mutable Vec3 cachedLocalRayDirection_;
  mutable float cachedRayInverse_[3] = {0.0f, 0.0f, 0.0f};
  mutable bool cachedRayParallel_[3] = {false, false, false};

  mutable bool rayProjectionCacheValid_ = false;
  mutable Vec3 cachedProjectionPosition_;
  mutable Vec3 cachedProjectionMinimum_;
  mutable Vec3 cachedProjectionMaximum_;
  mutable Vec3 cachedProjectionRayDirection_;
  mutable Vec3 cachedProjectionRight_;
  mutable Vec3 cachedProjectionUp_;
  mutable float cachedProjectionMinX_ = 0.0f;
  mutable float cachedProjectionMinY_ = 0.0f;
  mutable float cachedProjectionMaxX_ = 0.0f;
  mutable float cachedProjectionMaxY_ = 0.0f;
};

} // namespace engine
} // namespace isoweb
