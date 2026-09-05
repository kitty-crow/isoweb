#pragma once

#include <vector>

#include "engine/math/Vec3.hpp"
#include "engine/world/Character.hpp"
#include "engine/world/IWorld.hpp"

namespace isoweb {
namespace engine {

struct NavigationPath {
  std::vector<Vec3> waypoints;
  Vec3 resolvedDestination;
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
    const Vec3& requestedDestination
  ) const;
};

} // namespace engine
} // namespace isoweb
