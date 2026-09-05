#pragma once

#include <string>
#include <vector>

#include "engine/world/Object.hpp"

namespace isoweb {
namespace engine {

// A world-supplied traversable route between places. The engine intentionally
// does not attach semantics such as staircase, ramp, bridge, ladder, portal,
// or doorway to it. Games may expose any geometry or mechanic they want and
// advertise the resulting traversable route through this structure.
struct NavigationConnection {
  std::string id;
  std::vector<EntityLocation> points;
  bool enabled = true;
  bool bidirectional = true;
  float traversalCostMultiplier = 1.0f;

  bool valid() const {
    return enabled && points.size() >= 2;
  }
};

} // namespace engine
} // namespace isoweb
