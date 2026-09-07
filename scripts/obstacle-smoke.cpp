#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "demo/DemoObstacles.hpp"
#include "demo/DemoWorld.hpp"
#include "engine/character/CharacterSystem.hpp"

namespace {

bool contains(const std::vector<std::string>& values, const std::string& expected) {
  return std::find(values.begin(), values.end(), expected) != values.end();
}

isoweb::engine::Character& addPlayer(
  isoweb::engine::World& world,
  const isoweb::engine::Vec3& position
) {
  std::unique_ptr<isoweb::engine::Character> player(new isoweb::engine::Character());
  player->id = "obstacle-test-player";
  player->location.worldId = "demo";
  player->location.timelineId = "default";
  player->location.levelId = "middle";
  player->location.position = position;
  player->forward = {0.0f, 1.0f, 0.0f};
  player->hitBox.minimum = {-0.25f, -0.15f, 0.0f};
  player->hitBox.maximum = {0.25f, 0.15f, 1.65f};
  player->solid = true;
  player->controllable = true;
  player->collisionTags.push_back("character");
  return static_cast<isoweb::engine::Character&>(world.entities().add(std::move(player)));
}

} // namespace

int main() {
  using namespace isoweb;

  demo::DemoWorld world;
  engine::CharacterSystem characters(world);
  demo::DemoObstacleSystem obstacles(world);

  if (obstacles.enabled() || obstacles.partCount() != 0) return 1;

  engine::Character& player = addPlayer(world, {0.0f, 2.40f, 0.0f});
  const std::size_t baselineEntityCount = world.entities().all().size();

  obstacles.setEnabled(true);
  if (!obstacles.enabled() || obstacles.partCount() != 5) return 2;
  if (world.entities().all().size() != baselineEntityCount + 8) return 3;

  auto* barrierLeft = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-barrier-left")
  );
  auto* bladeA = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-blade-a")
  );
  if (!barrierLeft || !bladeA) return 4;
  if (!world.entities().find("demo-obstacle-nav-barrier")) return 5;

  const engine::Vec3 barrierBefore = barrierLeft->location.position;
  const engine::Vec3 bladeForwardBefore = bladeA->forward;
  obstacles.tick(0.10f);
  if (barrierLeft->location.position.x == barrierBefore.x) return 6;
  if (
    bladeA->forward.x == bladeForwardBefore.x &&
    bladeA->forward.y == bladeForwardBefore.y
  ) return 7;

  engine::EntityLocation destination = player.location;
  destination.position = {2.15f, -0.95f, 0.0f};
  if (!characters.command(player, destination)) return 8;
  if (player.movement.pathBlocked || player.movement.route.empty()) return 9;
  characters.stop(player);

  // The long front/back faces of the sliding barrier are solid but harmless.
  barrierLeft = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-barrier-left")
  );
  if (!barrierLeft) return 10;
  player.location.position = barrierLeft->location.position + engine::Vec3(0.0f, 0.25f, 0.0f);
  player.forward = {0.0f, 1.0f, 0.0f};
  if (contains(obstacles.tick(0.0f), player.id)) return 11;

  // The short face bordering the opening is lethal.
  player.location.position = barrierLeft->location.position + engine::Vec3(0.875f, 0.0f, 0.0f);
  if (!contains(obstacles.tick(0.0f), player.id)) return 12;

  // The guillotine only hurts when its lower face reaches the Character.
  player.location.position = {0.15f, -0.05f, 0.0f};
  bool guillotineHit = false;
  for (int step = 0; step < 80 && !guillotineHit; ++step) {
    guillotineHit = contains(obstacles.tick(0.05f), player.id);
  }
  if (!guillotineHit) return 13;

  obstacles.setEnabled(false);
  if (obstacles.enabled() || obstacles.partCount() != 0) return 14;
  if (world.entities().all().size() != baselineEntityCount) return 15;
  if (world.entities().find("demo-obstacle-nav-blades")) return 16;

  return 0;
}
