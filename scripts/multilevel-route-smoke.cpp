#include <cstdlib>
#include <iostream>
#include <memory>

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"

using isoweb::engine::Camera;
using isoweb::engine::CameraConfig;
using isoweb::engine::Character;
using isoweb::engine::CharacterSystem;
using isoweb::engine::EntityLocation;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "[multilevel-route] " << message << '\n';
    std::exit(1);
  }
}

void runUntilSettled(CharacterSystem& characters, Character& character, Camera& camera) {
  for (int tick = 0; tick < 4000 && character.moving; ++tick) {
    characters.tick(0.05f, camera);
  }
}

std::size_t transitionCount(const Character& character) {
  std::size_t count = 0;
  for (const auto& waypoint : character.movement.route) {
    if (waypoint.levelTransition) ++count;
  }
  return count;
}

void verifyRoute(
  const char* label,
  const char* fromLevel,
  const char* toLevel,
  const char* rejectedMessage,
  const char* transitionMessage,
  const char* settledMessage,
  const char* arrivalMessage
) {
  isoweb::demo::DemoWorld world;
  CharacterSystem characters(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));

  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = label;
  character->location = {"demo", "default", fromLevel, {0.0f, 2.40f, 0.0f}};
  character->hitBox.minimum = {-0.28f, -0.20f, 0.0f};
  character->hitBox.maximum = {0.28f, 0.20f, 1.65f};
  character->forward = {0.0f, 1.0f, 0.0f};
  world.entities().add(std::move(owned));

  EntityLocation destination = character->location;
  destination.levelId = toLevel;
  destination.position = {0.0f, 2.40f, 0.0f};

  require(characters.command(*character, destination), rejectedMessage);
  require(transitionCount(*character) == 2, transitionMessage);
  runUntilSettled(characters, *character, camera);
  require(!character->moving, settledMessage);
  require(character->location.levelId == toLevel, arrivalMessage);
}

} // namespace

int main() {
  verifyRoute(
    "upper-to-lower-runner",
    "upper",
    "lower",
    "fresh upper -> lower command was rejected",
    "fresh upper -> lower route did not contain both level transitions",
    "fresh upper -> lower route never settled",
    "fresh upper -> lower route stopped before the lower level"
  );

  verifyRoute(
    "lower-to-upper-runner",
    "lower",
    "upper",
    "fresh lower -> upper command was rejected",
    "fresh lower -> upper route did not contain both level transitions",
    "fresh lower -> upper route never settled",
    "fresh lower -> upper route stopped before the upper level"
  );

  std::cout << "Three-level Character routing smoke test passed.\n";
  return 0;
}
