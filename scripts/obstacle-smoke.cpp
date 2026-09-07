#include <algorithm>
#include <cmath>
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
  if (world.entities().all().size() != baselineEntityCount + 5) return 3;

  auto* barrierLeft = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-barrier-left")
  );
  auto* barrierRight = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-barrier-right")
  );
  auto* guillotine = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-guillotine")
  );
  auto* bladeA = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-blade-a")
  );
  if (!barrierLeft || !barrierRight || !guillotine || !bladeA) return 4;

  // There are no invisible conservative blockers anymore. The planner sees
  // the actual moving obstacle geometry.
  if (world.entities().find("demo-obstacle-nav-barrier")) return 5;
  if (world.entities().find("demo-obstacle-nav-guillotine")) return 6;
  if (world.entities().find("demo-obstacle-nav-blades")) return 7;

  // The sliding wall spans beyond both playable X edges, leaving exactly one
  // moving opening. It must not intersect the existing demo scenery.
  const float leftOuter = barrierLeft->localToWorld({barrierLeft->hitBox.minimum.x, 0.0f, 0.0f}).x;
  const float rightOuter = barrierRight->localToWorld({barrierRight->hitBox.maximum.x, 0.0f, 0.0f}).x;
  if (leftOuter > -4.40f || rightOuter < 4.40f) return 8;
  if (world.collidesWith(*barrierLeft, barrierLeft)) return 9;
  if (world.collidesWith(*barrierRight, barrierRight)) return 10;
  if (world.collidesWith(*guillotine, guillotine)) return 11;

  const engine::Vec3 barrierBefore = barrierLeft->location.position;
  const engine::Vec3 bladeForwardBefore = bladeA->forward;
  obstacles.tick(0.10f);
  if (std::fabs(barrierLeft->location.position.x - barrierBefore.x) < 1e-5f) return 12;
  if (
    std::fabs(bladeA->forward.x - bladeForwardBefore.x) < 1e-5f &&
    std::fabs(bladeA->forward.y - bladeForwardBefore.y) < 1e-5f
  ) return 13;

  // A Character starting on the spawn side can plan through the wall's real
  // opening. There is no route around either outside end of the wall.
  engine::EntityLocation destination = player.location;
  destination.position = {0.0f, 1.20f, 0.0f};
  if (!characters.command(player, destination)) return 14;
  if (player.movement.pathBlocked || player.movement.route.empty()) return 15;
  characters.stop(player);

  // The long front/back faces are solid but harmless.
  barrierLeft = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-barrier-left")
  );
  if (!barrierLeft) return 16;
  player.location.position = barrierLeft->location.position + engine::Vec3(0.0f, 0.25f, 0.0f);
  player.forward = {0.0f, 1.0f, 0.0f};
  if (contains(obstacles.tick(0.0f), player.id)) return 17;

  // The short inner face bordering the moving opening is lethal.
  const engine::Vec3 innerFace = barrierLeft->localToWorld({
    barrierLeft->hitBox.maximum.x,
    0.0f,
    0.0f
  });
  player.location.position = innerFace + engine::Vec3(0.24f, 0.0f, 0.0f);
  if (!contains(obstacles.tick(0.0f), player.id)) return 18;

  // Move the player between the two gates, then lower the guillotine. Its
  // blade spans the complete level, so a destination on the far side must be
  // retained as blocked intent rather than routed around an end.
  player.location.position = {0.0f, -0.75f, 0.0f};
  player.forward = {0.0f, -1.0f, 0.0f};
  obstacles.tick(1.40f);
  guillotine = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-guillotine")
  );
  if (!guillotine || guillotine->location.position.z > 0.40f) return 19;
  const float guillotineLeft = guillotine->localToWorld({guillotine->hitBox.minimum.x, 0.0f, 0.0f}).x;
  const float guillotineRight = guillotine->localToWorld({guillotine->hitBox.maximum.x, 0.0f, 0.0f}).x;
  if (guillotineLeft > -4.40f || guillotineRight < 4.40f) return 20;
  if (world.collidesWith(*guillotine, guillotine)) return 21;

  destination = player.location;
  destination.position = {0.0f, -2.00f, 0.0f};
  if (!characters.command(player, destination)) return 22;
  if (!player.movement.pathBlocked) return 23;
  characters.stop(player);

  // Once the guillotine rises, exactly the same cross-level request becomes
  // physically routable. Blocked-intent retry in the live engine uses this
  // same geometry and will recover automatically.
  player.location.position = {0.0f, -0.75f, 0.0f};
  obstacles.tick(1.35f);
  guillotine = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-guillotine")
  );
  if (!guillotine || guillotine->location.position.z < 2.0f) return 24;
  destination = player.location;
  destination.position = {0.0f, -2.00f, 0.0f};
  if (!characters.command(player, destination)) return 25;
  if (player.movement.pathBlocked || player.movement.route.empty()) return 26;
  characters.stop(player);

  // Test the deadly face as a real impact: start beneath the raised blade and
  // let it descend. Teleporting into an already-lowered blade would not tell us
  // which face made contact.
  player.location.position = {0.0f, -1.38f, 0.0f};
  bool guillotineHit = false;
  for (int step = 0; step < 80 && !guillotineHit; ++step) {
    guillotineHit = contains(obstacles.tick(0.05f), player.id);
  }
  if (!guillotineHit) return 27;

  obstacles.setEnabled(false);
  if (obstacles.enabled() || obstacles.partCount() != 0) return 28;
  if (world.entities().all().size() != baselineEntityCount) return 29;

  return 0;
}
