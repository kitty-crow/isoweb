#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"

using namespace isoweb::engine;

namespace {

Character* addRunner(isoweb::demo::DemoWorld& world, const char* id, float x, float y) {
  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = id;
  character->location = {"demo", "default", "middle", {x, y, 0.0f}};
  character->hitBox.minimum = {-0.28f, -0.20f, 0.0f};
  character->hitBox.maximum = {0.28f, 0.20f, 1.65f};
  character->forward = {0.0f, -1.0f, 0.0f};
  world.entities().add(std::move(owned));
  return character;
}

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "[demo-solid-collision] " << message << "\n";
  std::exit(1);
}

void runCubeCrossing() {
  isoweb::demo::DemoWorld world;
  CharacterSystem characters(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
  Character* runner = addRunner(world, "cube-runner", -1.05f, 2.40f);

  EntityLocation destination = runner->location;
  destination.position = {-1.05f, -1.40f, 0.0f};
  require(characters.command(*runner, destination), "cube crossing command rejected");

  for (int tick = 0; tick < 2400 && characters.needsTick(); ++tick) {
    characters.tick(0.05f, camera);
    require(!world.collidesWith(*runner, runner), "runtime character overlaps a solid while routing around cube");

    const Vec3& p = runner->location.position;
    const bool centreInsideCube =
      p.x > -1.85f + 0.01f && p.x < -0.25f - 0.01f &&
      p.y > -0.15f + 0.01f && p.y < 1.45f - 0.01f;
    require(!centreInsideCube, "character centre entered blue cube footprint");
  }

  require(!runner->moving, "cube crossing never settled");
  require(!runner->movement.hasDestination, "cube crossing retained destination intent");
}

void runSphereCrossing() {
  isoweb::demo::DemoWorld world;
  CharacterSystem characters(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
  Character* runner = addRunner(world, "sphere-runner", 1.05f, 2.40f);

  EntityLocation destination = runner->location;
  destination.position = {1.05f, -1.80f, 0.0f};
  require(characters.command(*runner, destination), "sphere crossing command rejected");

  for (int tick = 0; tick < 2400 && characters.needsTick(); ++tick) {
    characters.tick(0.05f, camera);
    require(!world.collidesWith(*runner, runner), "runtime character overlaps a solid while routing around sphere");

    const float dx = runner->location.position.x - 1.05f;
    const float dy = runner->location.position.y + 0.25f;
    const float centreDistance = std::sqrt(dx * dx + dy * dy);
    require(centreDistance >= 0.89f, "character centre entered orange sphere footprint");
  }

  require(!runner->moving, "sphere crossing never settled");
  require(!runner->movement.hasDestination, "sphere crossing retained destination intent");
}

} // namespace

int main() {
  runCubeCrossing();
  runSphereCrossing();
  std::cout << "Demo cube and sphere remain solid throughout Character routing.\n";
  return 0;
}
