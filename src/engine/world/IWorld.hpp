#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "engine/math/Vec3.hpp"
#include "engine/navigation/NavigationConnection.hpp"
#include "engine/render/Ray.hpp"
#include "engine/world/Character.hpp"
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
  virtual const WorldBounds& boundsForLevel(const std::string& levelId) const = 0;
  virtual Vec3 sample(const Ray& ray, float backgroundY) const = 0;
  virtual const std::vector<Object>& objects() const = 0;
  virtual const std::vector<Object>& objectsForLevel(const std::string& levelId) const = 0;
  virtual const std::vector<Character>& characters() const = 0;
  virtual const std::vector<NavigationConnection>& navigationConnections() const = 0;
  virtual bool intersectsSolid(const HitBox& hitBox) const = 0;
  virtual bool collidesWith(const Object& candidate) const = 0;
  virtual bool navigationAllows(const Object& candidate) const = 0;

  virtual std::size_t levelCount() const = 0;
  virtual std::size_t activeLevelIndex() const = 0;
  virtual std::size_t defaultLevelIndex() const = 0;
  virtual const std::string& activeLevelId() const = 0;

  virtual float baseMovementSpeed() const = 0;
};

} // namespace engine
} // namespace isoweb
