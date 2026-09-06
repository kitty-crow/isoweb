#pragma once

#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {

class World;

struct CharacterEngineDefaults {
  float baseMovementSpeed = 1.45f;
  float navigationCellSize = 0.25f;
  float arrivalEpsilon = 0.025f;
  float destinationSearchRadius = 2.5f;
};

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
  virtual EntityLocation resolve(
    const World& world,
    const Character& character,
    const EntityLocation& requested,
    const CharacterEngineDefaults& defaults
  ) const;
};

class NavigationPolicy {
public:
  virtual ~NavigationPolicy() = default;
  virtual bool buildRoute(
    const World& world,
    const Character& character,
    const EntityLocation& destination,
    const CharacterEngineDefaults& defaults,
    CharacterMovementState& route
  ) const = 0;
};

class MovementPolicy {
public:
  virtual ~MovementPolicy() = default;
  virtual float effectiveSpeed(const Character& character, float baseMovementSpeed) const {
    return baseMovementSpeed * character.movementSpeedMultiplier;
  }
};

class InteractionPolicy {
public:
  virtual ~InteractionPolicy() = default;
  virtual bool canCommand(const Character& character) const {
    return character.controllable;
  }
};

class DefaultNavigationPolicy final : public NavigationPolicy {
public:
  bool buildRoute(
    const World& world,
    const Character& character,
    const EntityLocation& destination,
    const CharacterEngineDefaults& defaults,
    CharacterMovementState& route
  ) const override;
};

} // namespace engine
} // namespace isoweb
