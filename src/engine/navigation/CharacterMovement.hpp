#pragma once

#include "engine/navigation/Pathfinder.hpp"

namespace isoweb {
namespace engine {

class CharacterMovement {
public:
  bool setDestination(
    const IWorld& world,
    Character& character,
    const EntityLocation& destination
  ) const;

  // Advances one character using world base speed multiplied by the
  // character's movementSpeedMultiplier. Returns true when runtime state
  // changed and a new render is needed.
  bool advance(const IWorld& world, Character& character, float deltaSeconds) const;

private:
  Pathfinder pathfinder_;
};

} // namespace engine
} // namespace isoweb
