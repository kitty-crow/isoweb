#include <cmath>
#include <iostream>
#include <memory>

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"
#include "engine/world/SceneSurface.hpp"

using isoweb::engine::Camera;
using isoweb::engine::CameraConfig;
using isoweb::engine::Character;
using isoweb::engine::CharacterSystem;
using isoweb::engine::EntityLocation;
using isoweb::engine::Ray;
using isoweb::engine::SceneSurfaceHit;
using isoweb::engine::SceneSurfaceKind;

namespace {

bool near(float a, float b, float tolerance = 0.03f) {
  return std::fabs(a - b) <= tolerance;
}

} // namespace

int main() {
  isoweb::demo::DemoWorld world;

  // This ray lies inside the old sphere AABB but outside the actual sphere.
  // The authoritative world must therefore see the real floor, not a fake box.
  SceneSurfaceHit sphereCorner;
  const Ray outsideSphereInsideOldProxy{{1.80f, 0.50f, 5.0f}, {0.0f, 0.0f, -1.0f}};
  if (!world.traceEnvironment(outsideSphereInsideOldProxy, sphereCorner)) return 1;
  if (sphereCorner.kind != SceneSurfaceKind::Ground) return 2;
  if (!near(sphereCorner.point.z, 0.0f)) return 3;

  // A point in the middle->lower opening must stand on the rendered descending
  // staircase, below z=0. It must never resolve to an imaginary floor over the hole.
  SceneSurfaceHit stairSupport;
  if (!world.walkableSurfaceAt("middle", 2.15f, -2.25f, stairSupport)) return 4;
  if (stairSupport.kind != SceneSurfaceKind::Stair) return 5;
  if (stairSupport.point.z >= -0.05f) return 6;

  SceneSurfaceHit ordinaryFloor;
  if (!world.walkableSurfaceAt("middle", 0.0f, 2.40f, ordinaryFloor)) return 7;
  if (ordinaryFloor.kind != SceneSurfaceKind::Ground || !near(ordinaryFloor.point.z, 0.0f)) return 8;

  CharacterSystem characters(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));

  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = "geometry-runner";
  character->location = {"demo", "default", "middle", {0.0f, 2.40f, 0.0f}};
  character->hitBox.minimum = {-0.28f, -0.20f, 0.0f};
  character->hitBox.maximum = {0.28f, 0.20f, 1.65f};
  character->forward = {0.0f, 1.0f, 0.0f};
  world.entities().add(std::move(owned));

  EntityLocation lowerDestination = character->location;
  lowerDestination.levelId = "lower";
  lowerDestination.position = {0.0f, 2.40f, 0.0f};
  if (!characters.command(*character, lowerDestination)) return 9;

  bool routeDescendsInMiddle = false;
  bool routeTransitionsLower = false;
  for (const auto& waypoint : character->movement.route) {
    if (waypoint.location.levelId == "middle" && waypoint.location.position.z < -0.05f) {
      routeDescendsInMiddle = true;
    }
    if (waypoint.levelTransition && waypoint.location.levelId == "lower") {
      routeTransitionsLower = true;
    }
  }
  if (!routeDescendsInMiddle) return 10;
  if (!routeTransitionsLower) return 11;

  bool physicallyDescended = false;
  for (int tick = 0; tick < 1600 && character->moving; ++tick) {
    characters.tick(0.05f, camera);
    if (character->location.levelId == "middle" && character->location.position.z < -0.05f) {
      physicallyDescended = true;
      SceneSurfaceHit support;
      if (!world.walkableSurfaceAt(
        "middle",
        character->location.position.x,
        character->location.position.y,
        support
      )) {
        return 12;
      }
      if (std::fabs(character->location.position.z - support.point.z) > 0.26f) return 13;
    }
  }

  if (!physicallyDescended) return 14;
  if (character->location.levelId != "lower") return 15;
  if (character->moving) return 16;

  std::cout << "Unified world geometry and physical stair traversal smoke test passed.\n";
  return 0;
}
