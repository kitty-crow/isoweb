#include <iostream>
#include <type_traits>

#include "demo/DemoWorld.hpp"
#include "engine/world/Character.hpp"
#include "engine/world/Object.hpp"
#include "engine/world/WorldObject.hpp"

using isoweb::engine::Character;
using isoweb::engine::HitBox;
using isoweb::engine::Object;
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
    -height * 0.5f,
    width * 0.5f,
    depth * 0.5f,
    height * 0.5f
  );
  return object;
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

  Object ghost = localBox(1.0f, 1.0f, 1.0f);
  Object ward = localBox(1.0f, 1.0f, 1.0f);
  ghost.id = "ghost";
  ghost.collisionTags.push_back("spectral");
  ghost.solid = false;
  ward.id = "ward";
  ward.solid = false;
  if (ghost.blocks(ward) || ward.blocks(ghost)) return 11;
  ward.mustCollideWith.push_back("spectral");
  if (!ghost.blocks(ward) || !ward.blocks(ghost)) return 12;

  Character character;
  character.id = "character";
  character.location.worldId = "demo";
  character.location.timelineId = "default";
  character.location.levelId = "middle";
  character.location.position = {1.0f, 2.0f, 0.0f};
  character.forward = {0.7071067f, 0.7071067f, 0.0f};
  character.hitBox = box(-0.25f, -0.15f, 0.0f, 0.25f, 0.15f, 1.7f);
  character.movementSpeedMultiplier = 1.25f;
  if (character.hasArtwork()) return 13;
  if (character.npc || !character.controllable || !character.solid) return 14;

  character.sprites.still.front.resource = "front-still.webp";
  character.sprites.still.back.resource = "back-still.webp";
  character.sprites.still.left.resource = "left-still.webp";
  character.sprites.moving.front.resource = "front-moving.webp";
  character.sprites.moving.back.resource = "back-moving.webp";
  character.sprites.moving.left.resource = "left-moving.webp";
  if (!character.hasArtwork() || !character.hasRequiredMovementArtwork()) return 15;
  if (character.sprites.still.hasExplicitRight()) return 16;
  character.sprites.still.right.resource = "right-still.webp";
  if (!character.sprites.still.hasExplicitRight()) return 17;
  character.sprites.actions["wave"].front.resource = "wave-front.webp";

  isoweb::demo::DemoWorld world;
  if (world.activeLevelIndex() != 1 || world.objects().size() != 2) return 18;
  for (const Object& object : world.objects()) {
    if (!object.solid) return 19;
  }

  Object middleCube;
  middleCube.hitBox = box(-1.10f, 0.60f, 0.10f, -1.00f, 0.70f, 0.20f);
  Object openFloor;
  openFloor.hitBox = box(3.50f, 3.50f, 0.10f, 3.60f, 3.60f, 0.20f);
  if (!world.collidesWith(middleCube)) return 20;
  if (world.collidesWith(openFloor)) return 21;

  if (!world.levelDown() || world.activeLevelIndex() != 0 || world.objects().size() != 2) return 22;
  Object lowerCone;
  lowerCone.hitBox = box(-1.35f, -0.85f, 0.10f, -1.25f, -0.75f, 0.20f);
  if (!world.collidesWith(lowerCone)) return 23;

  if (!world.levelUp() || !world.levelUp() || world.activeLevelIndex() != 2) return 24;
  if (world.objects().size() != 2) return 25;
  Object upperDodecahedron;
  upperDodecahedron.hitBox = box(-1.40f, 0.90f, 0.10f, -1.30f, 1.00f, 0.20f);
  if (!world.collidesWith(upperDodecahedron)) return 26;

  std::cout << "Generic object, character, orientation, and collision smoke test passed.\n";
  return 0;
}
