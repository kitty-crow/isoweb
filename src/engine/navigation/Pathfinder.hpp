#pragma once

#include <vector>

#include "engine/world/Character.hpp"
#include "engine/world/IWorld.hpp"

namespace isoweb {
namespace engine {

struct NavigationPath {
  std::vector<EntityLocation> waypoints;
  EntityLocation resolvedDestination;
  bool reachedRequestedDestination = false;

  bool empty() const {
    return waypoints.empty();
  }
};

class Pathfinder {
public:
  NavigationPath findPath(
    const IWorld& world,
    const Character& character,
    const EntityLocation& requestedDestination
  ) const;
};

} // namespace engine
} // namespace isoweb
