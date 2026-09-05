#include "demo/DemoWorld.hpp"

#include <string>

#include "engine/navigation/NavigationConnection.hpp"

namespace isoweb {
namespace demo {
namespace {

constexpr float STAIR_RISE = 1.40f;
constexpr float STAIR_LOW_Y = -3.30f;
constexpr float STAIR_HIGH_Y = -1.20f;
constexpr float LOWER_MIDDLE_X = 2.15f;
constexpr float MIDDLE_UPPER_X = 3.20f;
constexpr int STEP_COUNT = 7;

engine::EntityLocation location(
  const std::string& level,
  float x,
  float y,
  float z
) {
  engine::EntityLocation value;
  value.worldId = "demo";
  value.timelineId = "default";
  value.levelId = level;
  value.position = {x, y, z};
  return value;
}

engine::NavigationConnection upwardConnection(
  const std::string& id,
  const std::string& lowerLevel,
  const std::string& upperLevel,
  float x
) {
  engine::NavigationConnection connection;
  connection.id = id;
  connection.bidirectional = true;
  connection.enabled = true;
  connection.traversalCostMultiplier = 1.0f;

  for (int step = 0; step <= STEP_COUNT; ++step) {
    const float t = static_cast<float>(step) / STEP_COUNT;
    connection.points.push_back(location(
      lowerLevel,
      x,
      STAIR_LOW_Y + (STAIR_HIGH_Y - STAIR_LOW_Y) * t,
      STAIR_RISE * t
    ));
  }

  // Same physical endpoint expressed in the upper level's local coordinates.
  // This context switch is zero-distance. Navigation only sees a traversable
  // path; the fact that the demo renders this particular path as steps is not
  // encoded in the engine.
  connection.points.push_back(location(upperLevel, x, STAIR_HIGH_Y, 0.0f));
  return connection;
}

} // namespace

void DemoWorld::configureNavigationConnections() {
  setLevelId(0, "lower");
  setLevelId(1, "middle");
  setLevelId(2, "upper");

  clearNavigationConnections();
  addNavigationConnection(upwardConnection(
    "lower-middle-link",
    "lower",
    "middle",
    LOWER_MIDDLE_X
  ));
  addNavigationConnection(upwardConnection(
    "middle-upper-link",
    "middle",
    "upper",
    MIDDLE_UPPER_X
  ));
}

} // namespace demo
} // namespace isoweb
