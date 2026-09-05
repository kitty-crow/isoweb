#include "engine/navigation/CharacterMovement.hpp"

#include <algorithm>
#include <cmath>

namespace isoweb {
namespace engine {
namespace {

float distance3D(const Vec3& a, const Vec3& b) {
  return length(b - a);
}

Vec3 horizontalFacing(const Vec3& from, const Vec3& to, const Vec3& fallback) {
  Vec3 direction(to.x - from.x, to.y - from.y, 0.0f);
  const float magnitude = std::sqrt(direction.x * direction.x + direction.y * direction.y);
  return magnitude > 1e-7f ? direction / magnitude : fallback;
}

bool sameContext(const EntityLocation& a, const EntityLocation& b) {
  return a.worldId == b.worldId &&
    a.timelineId == b.timelineId &&
    a.levelId == b.levelId;
}

} // namespace

bool CharacterMovement::setDestination(
  const IWorld& world,
  Character& character,
  const EntityLocation& destination
) const {
  const NavigationPath path = pathfinder_.findPath(world, character, destination);

  character.navigation.route = path.waypoints;
  character.navigation.nextWaypoint = 0;
  character.navigation.requestedDestination = destination;
  character.navigation.hasDestination = !path.empty();
  character.moving = !path.empty();
  return !path.empty();
}

bool CharacterMovement::advance(
  const IWorld& world,
  Character& character,
  float deltaSeconds
) const {
  if (!character.moving || !character.navigation.hasDestination) return false;

  const float safeDelta = std::max(0.0f, std::min(0.10f, deltaSeconds));
  float remaining = world.baseMovementSpeed() *
    std::max(0.0f, character.movementSpeedMultiplier) * safeDelta;
  if (remaining <= 0.0f) return false;

  bool changed = false;
  const float epsilon = 1e-4f;

  while (
    remaining > epsilon &&
    character.navigation.nextWaypoint < character.navigation.route.size()
  ) {
    const EntityLocation waypoint =
      character.navigation.route[character.navigation.nextWaypoint];
    const float distance = distance3D(character.location.position, waypoint.position);

    if (distance <= epsilon) {
      character.location = waypoint;
      ++character.navigation.nextWaypoint;
      changed = true;
      continue;
    }

    character.forward = horizontalFacing(
      character.location.position,
      waypoint.position,
      character.forward
    );

    if (remaining >= distance) {
      remaining -= distance;
      character.location = waypoint;
      ++character.navigation.nextWaypoint;
      changed = true;
      continue;
    }

    const Vec3 direction = (waypoint.position - character.location.position) / distance;
    character.location.position = character.location.position + direction * remaining;
    // Keep the current context while traversing a segment. A context change
    // occurs only when the connection waypoint itself is reached.
    if (sameContext(character.location, waypoint)) {
      character.location.worldId = waypoint.worldId;
      character.location.timelineId = waypoint.timelineId;
      character.location.levelId = waypoint.levelId;
    }
    remaining = 0.0f;
    changed = true;
  }

  if (character.navigation.nextWaypoint >= character.navigation.route.size()) {
    character.navigation.clear();
    character.moving = false;
    changed = true;
  }

  return changed;
}

} // namespace engine
} // namespace isoweb
