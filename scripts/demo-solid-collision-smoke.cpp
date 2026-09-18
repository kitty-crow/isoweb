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

float dot2(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y;
}

void characterBasis(const Character& character, Vec3& forward, Vec3& right) {
  const float magnitude = std::sqrt(
    character.forward.x * character.forward.x +
    character.forward.y * character.forward.y
  );
  forward = magnitude > 1e-6f
    ? Vec3(character.forward.x / magnitude, character.forward.y / magnitude, 0.0f)
    : Vec3(0.0f, 1.0f, 0.0f);
  right = {forward.y, -forward.x, 0.0f};
}

bool independentlyOverlapsCube(const Character& character) {
  const Vec3 cubeCentre(-1.05f, 0.65f, 0.0f);
  const float cubeHalf = 0.80f;
  const float charHalfX = 0.28f;
  const float charHalfY = 0.20f;

  Vec3 forward;
  Vec3 right;
  characterBasis(character, forward, right);
  const Vec3 delta = character.location.position - cubeCentre;
  const Vec3 axes[4] = {
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    right,
    forward
  };

  for (const Vec3& axis : axes) {
    const float centreDistance = std::fabs(dot2(delta, axis));
    const float cubeRadius = cubeHalf * (std::fabs(axis.x) + std::fabs(axis.y));
    const float characterRadius =
      charHalfX * std::fabs(dot2(right, axis)) +
      charHalfY * std::fabs(dot2(forward, axis));
    if (centreDistance >= cubeRadius + characterRadius - 1e-4f) return false;
  }
  return true;
}

bool independentlyOverlapsSphere(const Character& character) {
  const Vec3 sphereCentre(1.05f, -0.25f, 0.0f);
  const float radius = 0.90f;
  const float charHalfX = 0.28f;
  const float charHalfY = 0.20f;

  Vec3 forward;
  Vec3 right;
  characterBasis(character, forward, right);
  const Vec3 delta = sphereCentre - character.location.position;
  const float localX = dot2(delta, right);
  const float localY = dot2(delta, forward);
  const float closestX = std::max(-charHalfX, std::min(charHalfX, localX));
  const float closestY = std::max(-charHalfY, std::min(charHalfY, localY));
  const float dx = localX - closestX;
  const float dy = localY - closestY;
  return dx * dx + dy * dy < radius * radius - 1e-4f;
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

    require(!independentlyOverlapsCube(*runner), "character footprint penetrated blue cube");
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

    require(!independentlyOverlapsSphere(*runner), "character footprint penetrated orange sphere");
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
