#include <cmath>
#include <iostream>
#include <type_traits>

#include "demo/DemoWorld.hpp"
#include "engine/navigation/CharacterMovement.hpp"
#include "engine/navigation/Pathfinder.hpp"
#include "engine/world/Character.hpp"
#include "engine/world/Object.hpp"
#include "engine/world/WorldObject.hpp"

using isoweb::engine::Character;
using isoweb::engine::CharacterMovement;
using isoweb::engine::EntityLocation;
using isoweb::engine::HitBox;
using isoweb::engine::NavigationPath;
using isoweb::engine::Object;
using isoweb::engine::ObjectRayHit;
using isoweb::engine::Pathfinder;
using isoweb::engine::Ray;
using isoweb::engine::WorldObject;

namespace {

HitBox box(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
  HitBox value;
  value.minimum = {minX, minY, minZ};
  value.maximum = {maxX, maxY, maxZ};
  return value;
}

Object localBox(float width, float depth, float height) {
  Object object;
  object.hitBox = box(
    -width * 0.5f,
    -depth * 0.5f,
    0.0f,
    width * 0.5f,
    depth * 0.5f,
    height
  );
  return object;
}

Character demoCharacter() {
  Character character;
  character.id = "walker";
  character.location.worldId = "demo";
  character.location.timelineId = "default";
  character.location.levelId = "middle";
  character.location.position = {0.0f, 2.15f, 0.0f};
  character.forward = {0.0f, -1.0f, 0.0f};
  character.hitBox = box(-0.26f, -0.19f, 0.0f, 0.26f, 0.19f, 1.45f);
  character.movementSpeedMultiplier = 1.0f;
  return character;
}

float horizontalDistance(const isoweb::engine::Vec3& a, const isoweb::engine::Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

} // namespace

int main() {
  static_assert(std::is_base_of<Object, Character>::value, "Character must extend Object");
  static_assert(std::is_same<WorldObject, Object>::value, "WorldObject must remain a compatibility alias");

  const HitBox origin = box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  const HitBox overlap = box(0.75f, 0.25f, 0.25f, 1.25f, 0.75f, 0.75f);
  const HitBox separate = box(1.25f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f);
  const HitBox touching = box(1.0f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f);

  if (!origin.intersects(overlap)) return 1;
  if (origin.intersects(separate)) return 2;
  if (origin.intersects(touching)) return 3;

  Object solid;
  solid.solid = true;
  solid.hitBox = origin;
  if (!solid.blocks(overlap)) return 4;

  Object passable = solid;
  passable.solid = false;
  if (passable.blocks(overlap)) return 5;

  if (!origin.contains({0.5f, 0.5f, 0.5f})) return 6;
  if (!origin.contains({1.0f, 1.0f, 1.0f})) return 7;
  if (origin.contains({1.01f, 0.5f, 0.5f})) return 8;

  Object longBox = localBox(2.0f, 0.2f, 1.0f);
  Object rotatedBox = localBox(2.0f, 0.2f, 1.0f);
  rotatedBox.location.position = {0.0f, 0.5f, 0.0f};
  if (longBox.overlaps(rotatedBox)) return 9;
  rotatedBox.forward = {1.0f, 0.0f, 0.0f};
  if (!longBox.overlaps(rotatedBox)) return 10;

  ObjectRayHit rayHit;
  const Ray ray{{0.0f, -2.0f, 0.5f}, {0.0f, 1.0f, 0.0f}};
  if (!longBox.rayIntersection(ray, 0.001f, 10.0f, rayHit)) return 11;
  if (!rayHit.found || rayHit.t <= 0.0f) return 12;

  Object ghost = localBox(1.0f, 1.0f, 1.0f);
  Object ward = localBox(1.0f, 1.0f, 1.0f);
  ghost.id = "ghost";
  ghost.collisionTags.push_back("spectral");
  ghost.solid = false;
  ward.id = "ward";
  ward.solid = false;
  if (ghost.blocks(ward) || ward.blocks(ghost)) return 13;
  ward.mustCollideWith.push_back("spectral");
  if (!ghost.blocks(ward) || !ward.blocks(ghost)) return 14;

  Character character;
  character.id = "character";
  character.location.worldId = "demo";
  character.location.timelineId = "default";
  character.location.levelId = "middle";
  character.location.position = {1.0f, 2.0f, 0.0f};
  character.forward = {0.7071067f, 0.7071067f, 0.0f};
  character.hitBox = box(-0.25f, -0.15f, 0.0f, 0.25f, 0.15f, 1.7f);
  character.movementSpeedMultiplier = 1.25f;
  if (character.hasArtwork()) return 15;
  if (character.npc || !character.controllable || !character.solid) return 16;

  character.sprites.still.front.resource = "front-still.webp";
  character.sprites.still.back.resource = "back-still.webp";
  character.sprites.still.left.resource = "left-still.webp";
  character.sprites.moving.front.resource = "front-moving.webp";
  character.sprites.moving.back.resource = "back-moving.webp";
  character.sprites.moving.left.resource = "left-moving.webp";
  if (!character.hasArtwork() || !character.hasRequiredMovementArtwork()) return 17;
  if (character.sprites.still.hasExplicitRight()) return 18;
  character.sprites.still.right.resource = "right-still.webp";
  if (!character.sprites.still.hasExplicitRight()) return 19;
  character.sprites.actions["wave"].front.resource = "wave-front.webp";

  isoweb::demo::DemoWorld world;
  world.configureNavigationConnections();
  if (world.activeLevelIndex() != 1 || world.objects().size() != 2) return 20;
  if (world.activeLevelId() != "middle") return 21;
  if (world.navigationConnections().size() != 2) return 22;
  for (const Object& object : world.objects()) {
    if (!object.solid) return 23;
  }

  Object middleCube;
  middleCube.location.levelId = "middle";
  middleCube.hitBox = box(-1.10f, 0.60f, 0.10f, -1.00f, 0.70f, 0.20f);
  Object openFloor;
  openFloor.location.levelId = "middle";
  openFloor.hitBox = box(3.50f, 3.50f, 0.10f, 3.60f, 3.60f, 0.20f);
  if (!world.collidesWith(middleCube)) return 24;
  if (world.collidesWith(openFloor)) return 25;

  Pathfinder pathfinder;
  Character walker = demoCharacter();

  EntityLocation blockedDestination = walker.location;
  blockedDestination.position = {-1.05f, 0.65f, 0.0f};
  const NavigationPath blockedPath = pathfinder.findPath(world, walker, blockedDestination);
  if (blockedPath.empty()) return 26;
  if (blockedPath.reachedRequestedDestination) return 27;
  Character resolved = walker;
  resolved.location = blockedPath.resolvedDestination;
  if (world.collidesWith(resolved)) return 28;

  EntityLocation lowerDestination = walker.location;
  lowerDestination.levelId = "lower";
  lowerDestination.position = {0.0f, 0.0f, 0.0f};
  const NavigationPath crossLevel = pathfinder.findPath(world, walker, lowerDestination);
  if (crossLevel.empty() || !crossLevel.reachedRequestedDestination) return 29;
  if (crossLevel.resolvedDestination.levelId != "lower") return 30;

  bool sawElevatedConnectionPoint = false;
  bool sawLowerLevel = false;
  for (const EntityLocation& waypoint : crossLevel.waypoints) {
    if (waypoint.position.z > 0.5f) sawElevatedConnectionPoint = true;
    if (waypoint.levelId == "lower") sawLowerLevel = true;
  }
  if (!sawElevatedConnectionPoint || !sawLowerLevel) return 31;

  CharacterMovement movement;
  Character& runtimeWalker = world.addCharacter(walker);
  if (!movement.setDestination(world, runtimeWalker, lowerDestination)) return 32;

  for (int step = 0; step < 1200 && runtimeWalker.moving; ++step) {
    movement.advance(world, runtimeWalker, 0.05f);
  }
  if (runtimeWalker.moving) return 33;
  if (runtimeWalker.location.levelId != "lower") return 34;
  if (horizontalDistance(runtimeWalker.location.position, lowerDestination.position) > 0.30f) return 35;
  if (std::fabs(runtimeWalker.forward.x) < 1e-5f && std::fabs(runtimeWalker.forward.y) < 1e-5f) return 36;

  std::cout << "Generic object, character, collision, navigation-link, and movement smoke test passed.\n";
  return 0;
}
