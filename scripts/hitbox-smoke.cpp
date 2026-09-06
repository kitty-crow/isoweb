#include <cmath>
#include <iostream>
#include <memory>
#include <type_traits>

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterPresentation.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/character/SpriteAtlas.hpp"
#include "engine/world/Character.hpp"
#include "engine/world/Object.hpp"
#include "engine/world/WorldObject.hpp"

using isoweb::engine::Camera;
using isoweb::engine::CameraConfig;
using isoweb::engine::Character;
using isoweb::engine::CharacterFacing;
using isoweb::engine::CharacterSystem;
using isoweb::engine::DefaultCharacterPresentationPolicy;
using isoweb::engine::EntityLocation;
using isoweb::engine::HitBox;
using isoweb::engine::NavigationLink;
using isoweb::engine::Object;
using isoweb::engine::SelectionMode;
using isoweb::engine::SpriteAnimation;
using isoweb::engine::SpriteAtlasRegistry;
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

bool near(float a, float b, float tolerance = 0.0001f) {
  return std::fabs(a - b) <= tolerance;
}

bool samePosition(const isoweb::engine::Vec3& a, const isoweb::engine::Vec3& b) {
  return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

void assignAllDirections(isoweb::engine::DirectionalSpriteSet& set, const SpriteAnimation& animation) {
  set.front = animation;
  set.back = animation;
  set.left = animation;
  set.right = animation;
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
  world.setLevelId(0, "lower");
  world.setLevelId(1, "middle");
  world.setLevelId(2, "upper");
  if (world.activeLevelIndex() != 1 || world.objects().size() != 2) return 18;
  for (const Object& object : world.objects()) {
    if (!object.solid) return 19;
  }

  Object middleCube;
  middleCube.hitBox = box(-1.10f, 0.60f, 0.10f, -1.00f, 0.70f, 0.20f);
  if (!world.collidesWith(middleCube)) return 20;

  Object openFloor;
  openFloor.location.levelId = "middle";
  openFloor.location.position = {3.50f, 3.50f, 0.0f};
  openFloor.hitBox = box(-0.05f, -0.05f, 0.10f, 0.05f, 0.05f, 0.20f);
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
  world.resetLevel();

  CharacterSystem system(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));

  std::unique_ptr<Character> runnerOwned(new Character());
  Character* runner = runnerOwned.get();
  runner->id = "runner";
  runner->location = {"demo", "default", "middle", {2.8f, 2.8f, 0.0f}};
  runner->hitBox = box(-0.25f, -0.15f, 0.0f, 0.25f, 0.15f, 1.70f);
  runner->forward = {0.0f, 1.0f, 0.0f};
  world.entities().add(std::move(runnerOwned));
  if (dynamic_cast<Character*>(world.entities().find("runner")) != runner) return 27;

  std::unique_ptr<Character> blockerOwned(new Character());
  Character* blocker = blockerOwned.get();
  blocker->id = "blocker";
  blocker->location = runner->location;
  blocker->hitBox = runner->hitBox;
  world.entities().add(std::move(blockerOwned));
  if (!world.collidesWith(*runner, runner)) return 28;
  blocker->location.position = {-3.7f, 3.7f, 0.0f};
  if (world.collidesWith(*runner, runner)) return 29;

  if (!system.selection().select(*runner, true)) return 30;
  if (!system.selection().select(*blocker, true)) return 31;
  if (system.selection().ids().size() != 2) return 32;
  world.levelDown();
  if (!system.selection().selected("runner") || !system.selection().selected("blocker")) return 33;
  world.resetLevel();
  system.setSelectionMode(SelectionMode::Single);
  if (!system.selection().ids().empty()) return 34;
  system.selection().select(*runner, true);
  system.selection().select(*blocker, true);
  if (system.selection().ids().size() != 1 || !system.selection().selected("blocker")) return 35;
  system.setSelectionMode(SelectionMode::Multiple);
  system.clearSelection();

  SpriteAnimation idle;
  idle.resource = "idle.webp";
  idle.frameCount = 4;
  idle.columns = 4;
  idle.nominalFramesPerSecond = 4.0f;
  assignAllDirections(runner->sprites.still, idle);
  system.updatePresentation(camera);
  if (!system.needsTick()) return 36;
  system.tick(0.26f, camera);
  if (runner->animation.frame != 1) return 37;

  SpriteAnimation walking = idle;
  walking.resource = "walking.webp";
  walking.nominalFramesPerSecond = 6.0f;
  assignAllDirections(runner->sprites.moving, walking);
  runner->moving = true;
  runner->movementSpeedMultiplier = 2.0f;
  DefaultCharacterPresentationPolicy presentationPolicy;
  const float scaledFps = presentationPolicy.framesPerSecond(
    *runner,
    walking,
    system.effectiveSpeed(*runner),
    system.defaults().baseMovementSpeed
  );
  if (!near(scaledFps, 12.0f)) return 38;
  runner->moving = false;
  runner->animation.reset();

  const std::uint8_t atlasPixels[8] = {255, 0, 0, 255, 0, 255, 0, 255};
  SpriteAtlasRegistry atlases;
  if (!atlases.registerRgba("two-frame.webp", 2, 1, atlasPixels, sizeof(atlasPixels))) return 39;
  SpriteAnimation twoFrame;
  twoFrame.resource = "two-frame.webp";
  twoFrame.frameCount = 2;
  twoFrame.columns = 2;
  if (atlases.sample(twoFrame, 0, 0.5f, 0.5f, false).colour.x < 0.9f) return 40;
  if (atlases.sample(twoFrame, 1, 0.5f, 0.5f, false).colour.y < 0.9f) return 41;

  runner->sprites = isoweb::engine::CharacterSpriteSet();
  runner->movementSpeedMultiplier = 1.0f;
  runner->location.position = {2.8f, 2.8f, 0.0f};
  runner->location.levelId = "middle";

  EntityLocation firstDestination = runner->location;
  firstDestination.position = {-2.8f, 2.8f, 0.0f};
  if (!system.command(*runner, firstDestination)) return 42;
  system.tick(0.10f, camera);
  const isoweb::engine::Vec3 redirectedFrom = runner->location.position;
  if (samePosition(redirectedFrom, {2.8f, 2.8f, 0.0f})) return 43;

  EntityLocation secondDestination = runner->location;
  secondDestination.position = {2.8f, -0.8f, 0.0f};
  if (!system.command(*runner, secondDestination)) return 44;
  if (!samePosition(runner->location.position, redirectedFrom)) return 45;
  if (!samePosition(runner->movement.destination.position, secondDestination.position)) return 46;

  EntityLocation blockedDestination = runner->location;
  blockedDestination.position = {-1.05f, 0.65f, 0.0f};
  if (!system.command(*runner, blockedDestination)) return 47;
  if (samePosition(runner->movement.destination.position, blockedDestination.position)) return 48;

  NavigationLink middleUpper;
  middleUpper.fromLevelId = "middle";
  middleUpper.toLevelId = "upper";
  middleUpper.fromPosition = {3.2f, 2.8f, 0.0f};
  middleUpper.toPosition = {3.2f, 2.8f, 0.0f};
  world.setNavigationLinks({middleUpper});
  runner->location.position = {2.8f, 2.8f, 0.0f};
  runner->location.levelId = "middle";
  EntityLocation upperDestination = runner->location;
  upperDestination.levelId = "upper";
  upperDestination.position = {3.5f, 3.2f, 0.0f};
  if (!system.command(*runner, upperDestination)) return 49;
  bool hasTransition = false;
  for (const auto& waypoint : runner->movement.route) {
    if (waypoint.levelTransition && waypoint.location.levelId == "upper") hasTransition = true;
  }
  if (!hasTransition) return 50;

  world.levelDown();
  runner->location.levelId = "middle";
  runner->location.position = {2.8f, 2.8f, 0.0f};
  EntityLocation offscreenDestination = runner->location;
  offscreenDestination.position = {2.2f, 2.8f, 0.0f};
  if (!system.command(*runner, offscreenDestination)) return 51;
  const auto beforeOffscreenTick = runner->location.position;
  system.tick(0.10f, camera);
  if (samePosition(beforeOffscreenTick, runner->location.position)) return 52;

  std::cout << "Generic object and full character-system smoke test passed.\n";
  return 0;
}
