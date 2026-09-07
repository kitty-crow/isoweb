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

} // namespace

int main() {
  isoweb::demo::DemoWorld world;
  CharacterSystem characters(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));

  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = "three-level-runner";
  character->location = {"demo", "default", "lower", {0.0f, 2.40f, 0.0f}};
  character->hitBox.minimum = {-0.28f, -0.20f, 0.0f};
  character->hitBox.maximum = {0.28f, 0.20f, 1.65f};
  character->forward = {0.0f, 1.0f, 0.0f};
  world.entities().add(std::move(owned));

  EntityLocation upperDestination = character->location;
  upperDestination.levelId = "upper";
  upperDestination.position = {0.0f, 2.40f, 0.0f};

  require(characters.command(*character, upperDestination), "lower -> upper command was rejected");
  require(transitionCount(*character) == 2, "lower -> upper route did not contain both level transitions");
  runUntilSettled(characters, *character, camera);
  require(!character->moving, "lower -> upper route never settled");
  require(character->location.levelId == "upper", "lower -> upper route stopped before the upper level");

  EntityLocation lowerDestination = character->location;
  lowerDestination.levelId = "lower";
  lowerDestination.position = {0.0f, 2.40f, 0.0f};

  require(characters.command(*character, lowerDestination), "upper -> lower command was rejected");
  require(transitionCount(*character) == 2, "upper -> lower route did not contain both level transitions");
  runUntilSettled(characters, *character, camera);
  require(!character->moving, "upper -> lower route never settled");
  require(character->location.levelId == "lower", "upper -> lower route stopped before the lower level");

  std::cout << "Three-level Character routing smoke test passed.\n";
  return 0;
}
