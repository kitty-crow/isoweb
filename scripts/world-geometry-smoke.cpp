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
using isoweb::engine::Object;
using isoweb::engine::Ray;
using isoweb::engine::Vec3;
using isoweb::engine::SceneSurfaceHit;
using isoweb::engine::SceneSurfaceKind;

namespace {

bool near(float a, float b, float tolerance = 0.03f) {
  return std::fabs(a - b) <= tolerance;
}

} // namespace

int main() {
  isoweb::demo::DemoWorld world;


  const auto* lowerRooms = world.roomLayout("lower");
  const auto* middleRooms = world.roomLayout("middle");
  const auto* upperRooms = world.roomLayout("upper");
  if (!lowerRooms || !middleRooms || !upperRooms) return 18;
  if (lowerRooms->rooms.size() != 5 || lowerRooms->connections.size() != 4) return 19;
  if (middleRooms->rooms.size() != 5 || middleRooms->connections.size() != 4) return 20;
  if (upperRooms->rooms.size() != 5 || upperRooms->connections.size() != 4) return 21;
  if (!lowerRooms->connected("centre", "north") || !lowerRooms->connected("centre", "west")) return 22;
  if (!middleRooms->connected("north", "north-west") || !middleRooms->connected("south", "south-east")) return 23;
  if (!upperRooms->connected("centre", "south") || !upperRooms->connected("south", "far-south")) return 24;
  if (world.lowerLevelPreviewDepth() != 2) return 25;
  if (world.residentLevelCount() != 2 || !world.isLevelResident("lower") || !world.isLevelResident("middle")) return 26;
  const float nominalCharacterHeight = 1.65f;
  const float storeyHeight = world.levelViewOrigin("middle").z - world.levelViewOrigin("lower").z;
  if (storeyHeight + 0.001f < nominalCharacterHeight * 1.25f) return 36;
  if (lowerRooms->rooms.front().wallHeight <= nominalCharacterHeight) return 37;

  const auto rayThrough = [](const Vec3& point, const Vec3& direction) {
    return Ray{point - direction * 6.0f, direction};
  };
  const Vec3 defaultView = isoweb::engine::normalise({1.0f, 1.0f, -1.0f});
  world.prepareRenderFrame(defaultView);

  // Default camera is south-west of the map. The centre room's west wall is
  // therefore view-facing: its broad upper centre must be cut away so the real
  // floor behind it wins the ray, while the low sill and end posts remain.
  SceneSurfaceHit cutawayCentre;
  if (!world.traceEnvironment(rayThrough({-4.40f, 0.0f, 1.05f}, defaultView), cutawayCentre)) return 38;
  if (cutawayCentre.kind != SceneSurfaceKind::Ground || !near(cutawayCentre.point.z, 0.0f)) return 39;

  SceneSurfaceHit cutawaySill;
  if (!world.traceEnvironment(rayThrough({-4.40f, 0.0f, 0.20f}, defaultView), cutawaySill)) return 40;
  if (cutawaySill.kind != SceneSurfaceKind::Object) return 41;

  SceneSurfaceHit cutawayEnd;
  if (!world.traceEnvironment(rayThrough({-4.40f, -4.15f, 1.05f}, defaultView), cutawayEnd)) return 42;
  if (cutawayEnd.kind != SceneSurfaceKind::Object) return 43;

  Object wallProbe;
  wallProbe.location.levelId = "middle";
  wallProbe.location.position = {-4.35f, 0.0f, 0.0f};
  wallProbe.hitBox.minimum = {-0.18f, -0.18f, 0.0f};
  wallProbe.hitBox.maximum = {0.18f, 0.18f, 1.65f};
  if (!world.collidesWith(wallProbe)) return 44;

  // Rotating 180 degrees moves the cutaway to the opposite camera-facing wall.
  const Vec3 oppositeView = isoweb::engine::normalise({-1.0f, -1.0f, -1.0f});
  world.prepareRenderFrame(oppositeView);
  SceneSurfaceHit rotatedCutaway;
  if (!world.traceEnvironment(rayThrough({4.40f, 0.0f, 1.05f}, oppositeView), rotatedCutaway)) return 45;
  if (rotatedCutaway.kind != SceneSurfaceKind::Ground || !near(rotatedCutaway.point.z, 0.0f)) return 46;
  world.prepareRenderFrame(defaultView);

  // The west arm of the lower cross is not covered by the middle Z. A visible
  // ray there must hit the real lower room at its stacked height and pick a
  // destination in lower-level local coordinates rather than middle.
  const Ray exposedLowerRay{{-8.80f, 0.0f, 8.0f}, {0.0f, 0.0f, -1.0f}};
  SceneSurfaceHit exposedLower;
  if (!world.traceEnvironment(exposedLowerRay, exposedLower)) return 27;
  if (exposedLower.kind != SceneSurfaceKind::Ground || !near(exposedLower.point.z, -storeyHeight)) return 28;
  EntityLocation exposedDestination;
  if (!world.pickWalkableDestination(exposedLowerRay, exposedDestination)) return 29;
  if (exposedDestination.levelId != "lower") return 30;
  if (!near(exposedDestination.position.x, -8.80f) || !near(exposedDestination.position.z, 0.0f)) return 31;

  if (!world.levelUp()) return 32;
  if (world.activeLevelId() != "upper" || world.residentLevelCount() != 3) return 33;
  if (!world.isLevelResident("middle") || !world.isLevelResident("lower")) return 34;
  if (!world.levelDown()) return 35;

  // This ray lies inside the old sphere AABB but outside the actual sphere.
  // The authoritative world must therefore see the real floor, not a fake box.
  SceneSurfaceHit sphereCorner;
  const Ray outsideSphereInsideOldProxy{{1.80f, 0.50f, 5.0f}, {0.0f, 0.0f, -1.0f}};
  if (!world.traceEnvironment(outsideSphereInsideOldProxy, sphereCorner)) return 1;
  if (sphereCorner.kind != SceneSurfaceKind::Ground) return 2;
  if (!near(sphereCorner.point.z, 0.0f)) return 3;

  // Sample the centre of a tread in the middle->lower opening. This must stand
  // on the rendered descending staircase, below z=0, rather than depending on
  // a tread boundary that changes when the authored step count changes.
  SceneSurfaceHit stairSupport;
  if (!world.walkableSurfaceAt("middle", 2.15f, -2.355f, stairSupport)) return 4;
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

  // A normal visible floor point must remain commandable after replacing the
  // old infinite z=0 click plane with actual scene-surface picking.
  EntityLocation sameLevelDestination = character->location;
  sameLevelDestination.position = {3.0f, 3.0f, 0.0f};
  if (!characters.command(*character, sameLevelDestination)) return 9;
  characters.stop(*character);
  character->location = {"demo", "default", "middle", {0.0f, 2.40f, 0.0f}};

  EntityLocation lowerDestination = character->location;
  lowerDestination.levelId = "lower";
  lowerDestination.position = {0.0f, 2.40f, 0.0f};
  if (!characters.command(*character, lowerDestination)) return 10;

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
  if (!routeDescendsInMiddle) return 11;
  if (!routeTransitionsLower) return 12;

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
        return 13;
      }
      if (std::fabs(character->location.position.z - support.point.z) > 0.26f) return 14;
    }
  }

  if (!physicallyDescended) return 15;
  if (character->location.levelId != "lower") return 16;
  if (character->moving) return 17;

  std::cout << "Room graphs, stacked previews, preview picking, and physical stair traversal smoke test passed.\n";
  return 0;
}
