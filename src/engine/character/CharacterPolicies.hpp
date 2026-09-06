#pragma once

#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {

class World;

class CollisionPolicy {
public:
  virtual ~CollisionPolicy() = default;
  virtual bool shouldCollide(const Object& a, const Object& b) const {
    return a.collisionEnabledWith(b);
  }
};

class DestinationPolicy {
public:
  virtual ~DestinationPolicy() = default;
  virtual EntityLocation resolve(const World& world, const Character& character, const EntityLocation& requested) const;
};

class MovementPolicy {
public:
  virtual ~MovementPolicy() = default;
  virtual float effectiveSpeed(const Character& character, float baseMovementSpeed) const {
    return baseMovementSpeed * character.movementSpeedMultiplier;
  }
};

struct CharacterEngineDefaults {
  float baseMovementSpeed = 1.45f;
  float navigationCellSize = 0.25f;
  float arrivalEpsilon = 0.025f;
  float destinationSearchRadius = 2.5f;
};

} // namespace engine
} // namespace isoweb
