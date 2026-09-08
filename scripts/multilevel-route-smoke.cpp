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

Character* addCharacter(isoweb::demo::DemoWorld& world, const char* label, const char* level) {
  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = label;
  character->location = {"demo", "default", level, {0.0f, 2.40f, 0.0f}};
  character->hitBox.minimum = {-0.28f, -0.20f, 0.0f};
  character->hitBox.maximum = {0.28f, 0.20f, 1.65f};
  character->forward = {0.0f, 1.0f, 0.0f};
  world.entities().add(std::move(owned));
  return character;
}

void verifyRoute(
  const char* label,
  const char* fromLevel,
  const char* toLevel,
  std::size_t expectedTransitions
) {
  isoweb::demo::DemoWorld world;
  CharacterSystem characters(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
  Character* character = addCharacter(world, label, fromLevel);

  EntityLocation destination = character->location;
  destination.levelId = toLevel;
  destination.position = {0.0f, 2.40f, 0.0f};

  if (!characters.command(*character, destination)) {
    std::cerr << "[multilevel-route] " << fromLevel << " -> " << toLevel
              << " command was rejected\n";
    std::exit(1);
  }
  if (transitionCount(*character) != expectedTransitions) {
    std::cerr << "[multilevel-route] " << fromLevel << " -> " << toLevel
              << " expected " << expectedTransitions << " transition(s), got "
              << transitionCount(*character) << '\n';
    std::exit(1);
  }

  runUntilSettled(characters, *character, camera);
  if (character->moving) {
    std::cerr << "[multilevel-route] " << fromLevel << " -> " << toLevel
              << " never settled\n";
    std::exit(1);
  }
  if (character->location.levelId != toLevel) {
    std::cerr << "[multilevel-route] " << fromLevel << " -> " << toLevel
              << " stopped on " << character->location.levelId << '\n';
    std::exit(1);
  }
}

} // namespace

int main() {
  verifyRoute("lower-middle", "lower", "middle", 1);
  verifyRoute("middle-lower", "middle", "lower", 1);
  verifyRoute("middle-upper", "middle", "upper", 1);
  verifyRoute("upper-middle", "upper", "middle", 1);
  verifyRoute("lower-upper", "lower", "upper", 2);
  verifyRoute("upper-lower", "upper", "lower", 2);

  std::cout << "Adjacent and three-level Character routing smoke test passed.\n";
  return 0;
}
