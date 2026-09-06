#pragma once

#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {

class World;
struct NavigationLink;

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

class LevelTransitionPolicy {
public:
  virtual ~LevelTransitionPolicy() = default;

  // Games may veto particular navigation links for a Character without
  // changing the navigation algorithm itself.
  virtual bool canTraverse(
    const Character& character,
    const NavigationLink& link,
    bool reverse
  ) const;

  // Games may customise the contextual/physical state applied at the level
  // boundary. The default lands at the link endpoint in the adjacent level.
  virtual EntityLocation arrival(
    const Character& character,
    const NavigationLink& link,
    bool reverse
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
    const LevelTransitionPolicy& transitions,
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
    const LevelTransitionPolicy& transitions,
    CharacterMovementState& route
  ) const override;
};

} // namespace engine
} // namespace isoweb
