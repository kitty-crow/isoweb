#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "engine/world/BehaviourSystem.hpp"
#include "demo/DemoWorld.hpp"
#include "engine/character/CharacterSystem.hpp"

namespace {

bool contains(const std::vector<std::string>& values, const std::string& expected) {
  return std::find(values.begin(), values.end(), expected) != values.end();
}

isoweb::engine::Character& addPlayer(
  isoweb::engine::World& world,
  const char* levelId,
  const isoweb::engine::Vec3& position
) {
  std::unique_ptr<isoweb::engine::Character> player(new isoweb::engine::Character());
  player->id = "obstacle-test-player";
  player->location.worldId = "demo";
  player->location.timelineId = "default";
  player->location.levelId = levelId;
  player->location.position = position;
  player->forward = {0.0f, 1.0f, 0.0f};
  player->hitBox.minimum = {-0.25f, -0.15f, 0.0f};
  player->hitBox.maximum = {0.25f, 0.15f, 1.65f};
  player->solid = true;
  player->controllable = true;
  player->collisionTags.push_back("character");
  return static_cast<isoweb::engine::Character&>(world.entities().add(std::move(player)));
}

float barrierOpeningCentre(
  const isoweb::engine::Character& left,
  const isoweb::engine::Character& right
) {
  const float leftInner = left.localToWorld({left.hitBox.maximum.x, 0.0f, 0.0f}).x;
  const float rightInner = right.localToWorld({right.hitBox.minimum.x, 0.0f, 0.0f}).x;
  return (leftInner + rightInner) * 0.5f;
}

float barrierOpeningWidth(
  const isoweb::engine::Character& left,
  const isoweb::engine::Character& right
) {
  const float leftInner = left.localToWorld({left.hitBox.maximum.x, 0.0f, 0.0f}).x;
  const float rightInner = right.localToWorld({right.hitBox.minimum.x, 0.0f, 0.0f}).x;
  return rightInner - leftInner;
}

void configureBehaviours(isoweb::engine::BehaviourSystem& behaviours) {
  using namespace isoweb::engine;
  auto box = [](const Vec3& minimum, const Vec3& maximum) {
    HitBox result;
    result.minimum = minimum;
    result.maximum = maximum;
    return result;
  };
  auto entity = [&](const char* id, const char* level, const Vec3& position, const Vec3& forward,
                    const HitBox& hitBox, SurfaceTextureMode textureMode = SurfaceTextureMode::TileLocal) {
    DynamicEntityDefinition definition;
    definition.id = id;
    definition.worldId = "demo";
    definition.timelineId = "default";
    definition.levelId = level;
    definition.position = position;
    definition.forward = forward;
    definition.hitBox = hitBox;
    definition.textureMode = textureMode;
    definition.textureWorldUnitsPerTile = 0.5f;
    definition.collisionTags.push_back("world-behaviour");
    behaviours.addEntity(definition);
  };

  entity("demo-obstacle-barrier-left", "middle", {0.0f, 1.90f, 0.0f}, {0.0f, 1.0f, 0.0f},
         box({-0.10f, -0.11f, 0.0f}, {0.10f, 0.11f, 1.30f}), SurfaceTextureMode::TileWorld);
  entity("demo-obstacle-barrier-right", "middle", {0.0f, 1.90f, 0.0f}, {0.0f, 1.0f, 0.0f},
         box({-0.10f, -0.11f, 0.0f}, {0.10f, 0.11f, 1.30f}), SurfaceTextureMode::TileWorld);
  entity("demo-obstacle-guillotine", "upper", {0.0f, 2.15f, 2.25f}, {0.0f, 1.0f, 0.0f},
         box({-4.48f, -0.12f, -0.12f}, {4.48f, 0.12f, 0.12f}));
  entity("demo-obstacle-blade-a", "lower", {2.15f, -3.82f, 0.0f}, {0.0f, 1.0f, 0.0f},
         box({-0.11f, 0.16f, 0.08f}, {0.11f, 1.08f, 0.28f}));
  entity("demo-obstacle-blade-b", "lower", {2.15f, -3.82f, 0.0f}, {0.0f, -1.0f, 0.0f},
         box({-0.11f, 0.16f, 0.08f}, {0.11f, 1.08f, 0.28f}));

  OscillatingGateDefinition gate;
  gate.leftId = "demo-obstacle-barrier-left";
  gate.rightId = "demo-obstacle-barrier-right";
  gate.base = {0.0f, 1.90f, 0.0f};
  gate.halfSpan = 4.48f;
  gate.gap = 2.35f;
  gate.sweep = 2.15f;
  gate.angularSpeed = 0.24f;
  gate.halfThickness = 0.11f;
  gate.height = 1.30f;
  behaviours.addGate(gate);

  VerticalCycleDefinition guillotine;
  guillotine.entityId = "demo-obstacle-guillotine";
  guillotine.base = {0.0f, 2.15f, 0.0f};
  guillotine.upZ = 2.25f;
  guillotine.downZ = 0.28f;
  guillotine.period = 2.60f;
  guillotine.blockOnSafeContact = true;
  guillotine.lethalFace = HazardFace::Bottom;
  guillotine.contactTolerance = 0.028f;
  behaviours.addVerticalCycle(guillotine);

  for (const auto& item : std::vector<std::pair<std::string, float>>{
         {"demo-obstacle-blade-a", 1.0f}, {"demo-obstacle-blade-b", -1.0f}}) {
    RotationDefinition rotation;
    rotation.entityIds.push_back(item.first);
    rotation.directionMultipliers.push_back(item.second);
    rotation.angularSpeed = 2.35f;
    behaviours.addRotation(rotation);
  }

  auto hazard = [&](const char* id, HazardFace face) {
    HazardDefinition definition;
    definition.entityId = id;
    definition.face = face;
    definition.tolerance = 0.028f;
    behaviours.addHazard(definition);
  };
  hazard("demo-obstacle-barrier-left", HazardFace::Right);
  hazard("demo-obstacle-barrier-right", HazardFace::Left);
  hazard("demo-obstacle-guillotine", HazardFace::Bottom);
  hazard("demo-obstacle-blade-a", HazardFace::Any);
  hazard("demo-obstacle-blade-b", HazardFace::Any);
}

} // namespace

int main() {
  using namespace isoweb;

  demo::DemoWorld world;
  engine::CharacterSystem characters(world);
  engine::BehaviourSystem obstacles(world);
  configureBehaviours(obstacles);

  if (obstacles.enabled() || obstacles.partCount() != 0) return 1;

  engine::Character& player = addPlayer(world, "middle", {0.0f, 2.40f, 0.0f});
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
  auto* bladeB = dynamic_cast<engine::Character*>(
    world.entities().find("demo-obstacle-blade-b")
  );
  if (!barrierLeft || !barrierRight || !guillotine || !bladeA || !bladeB) return 4;

  // Three obstacle types belong to three different levels. Only the two wall
  // segments share a level because together they are one moving-opening gate.
  if (barrierLeft->location.levelId != "middle" || barrierRight->location.levelId != "middle") return 5;
  if (guillotine->location.levelId != "upper") return 6;
  if (bladeA->location.levelId != "lower" || bladeB->location.levelId != "lower") return 7;

  // There are no invisible conservative navigation blockers.
  if (world.entities().find("demo-obstacle-nav-barrier")) return 8;
  if (world.entities().find("demo-obstacle-nav-guillotine")) return 9;
  if (world.entities().find("demo-obstacle-nav-blades")) return 10;

  // The middle-level wall still seals both outside edges, but its opening is
  // now several Character widths wide.
  const float leftOuter = barrierLeft->localToWorld({barrierLeft->hitBox.minimum.x, 0.0f, 0.0f}).x;
  const float rightOuter = barrierRight->localToWorld({barrierRight->hitBox.maximum.x, 0.0f, 0.0f}).x;
  if (leftOuter > -4.40f || rightOuter < 4.40f) return 11;
  if (barrierOpeningWidth(*barrierLeft, *barrierRight) < 2.20f) return 12;
  if (world.collidesWith(*barrierLeft, barrierLeft)) return 13;
  if (world.collidesWith(*barrierRight, barrierRight)) return 14;

  // Future wall textures must tile at fixed world density. Changing the wall's
  // length/opening cannot change the UV at a fixed world point.
  if (
    barrierLeft->surfaceTextureMode != engine::SurfaceTextureMode::TileWorld ||
    barrierRight->surfaceTextureMode != engine::SurfaceTextureMode::TileWorld
  ) {
    return 15;
  }
  engine::ObjectRayHit textureHit;
  textureHit.face = engine::ObjectFace::Front;
  textureHit.worldPoint = {-4.0f, 1.90f, 0.50f};
  const engine::SurfaceUV uvBefore = barrierLeft->surfaceTextureUV(textureHit);

  // At phase zero sine has its maximum slope. This first step therefore tests
  // the worst-case opening speed against the default Character speed.
  const float openingBefore = barrierOpeningCentre(*barrierLeft, *barrierRight);
  const engine::Vec3 bladeForwardBefore = bladeA->forward;
  constexpr float movementSampleSeconds = 0.10f;
  obstacles.tick(movementSampleSeconds);
  barrierLeft = dynamic_cast<engine::Character*>(world.entities().find("demo-obstacle-barrier-left"));
  barrierRight = dynamic_cast<engine::Character*>(world.entities().find("demo-obstacle-barrier-right"));
  bladeA = dynamic_cast<engine::Character*>(world.entities().find("demo-obstacle-blade-a"));
  if (!barrierLeft || !barrierRight || !bladeA) return 16;
  const float openingAfter = barrierOpeningCentre(*barrierLeft, *barrierRight);
  const float openingSpeed = std::fabs(openingAfter - openingBefore) / movementSampleSeconds;
  if (openingSpeed >= characters.defaults().baseMovementSpeed * 0.50f) return 17;
  if (
    std::fabs(bladeA->forward.x - bladeForwardBefore.x) < 1e-5f &&
    std::fabs(bladeA->forward.y - bladeForwardBefore.y) < 1e-5f
  ) {
    return 18;
  }
  const engine::SurfaceUV uvAfter = barrierLeft->surfaceTextureUV(textureHit);
  if (std::fabs(uvAfter.u - uvBefore.u) > 1e-5f || std::fabs(uvAfter.v - uvBefore.v) > 1e-5f) return 19;

  // A normal Character can actually plan through the moving opening from the
  // demo spawn side. This is the regression for the previously impossible gap.
  engine::EntityLocation destination = player.location;
  destination.position = {0.0f, 1.20f, 0.0f};
  if (!characters.command(player, destination)) return 20;
  if (player.movement.pathBlocked || player.movement.route.empty()) return 21;
  characters.stop(player);

  // Long front/back wall faces remain solid but harmless.
  player.location.levelId = "middle";
  player.location.position = barrierLeft->location.position + engine::Vec3(0.0f, 0.25f, 0.0f);
  player.forward = {0.0f, 1.0f, 0.0f};
  if (contains(obstacles.tick(0.0f), player.id)) return 22;

  // The short inner face bordering the moving opening remains lethal.
  const engine::Vec3 innerFace = barrierLeft->localToWorld({
    barrierLeft->hitBox.maximum.x,
    0.0f,
    0.0f
  });
  player.location.position = innerFace + engine::Vec3(0.24f, 0.0f, 0.0f);
  if (!contains(obstacles.tick(0.0f), player.id)) return 23;

  // On the upper level the lowered guillotine spans the complete width, so a
  // route across its Y position is blocked instead of escaping around an end.
  player.location.levelId = "upper";
  player.location.position = {0.0f, 2.85f, 0.0f};
  player.forward = {0.0f, -1.0f, 0.0f};
  characters.stop(player);
  obstacles.tick(1.20f);
  guillotine = dynamic_cast<engine::Character*>(world.entities().find("demo-obstacle-guillotine"));
  if (!guillotine || guillotine->location.position.z > 0.40f) return 24;
  const float guillotineLeft = guillotine->localToWorld({guillotine->hitBox.minimum.x, 0.0f, 0.0f}).x;
  const float guillotineRight = guillotine->localToWorld({guillotine->hitBox.maximum.x, 0.0f, 0.0f}).x;
  if (guillotineLeft > -4.40f || guillotineRight < 4.40f) return 25;
  if (world.collidesWith(*guillotine, guillotine)) return 26;

  destination = player.location;
  destination.position = {0.0f, 1.45f, 0.0f};
  if (!characters.command(player, destination)) return 27;
  if (!player.movement.pathBlocked) return 28;
  characters.stop(player);

  // Reset the cycle and let the blade genuinely fall onto a Character. Only
  // that lower face is the deadly guillotine surface.
  obstacles.setEnabled(false);
  obstacles.setEnabled(true);
  guillotine = dynamic_cast<engine::Character*>(world.entities().find("demo-obstacle-guillotine"));
  if (!guillotine) return 29;
  player.location.levelId = "upper";
  player.location.position = {0.0f, 2.15f, 0.0f};
  bool guillotineHit = false;
  for (int step = 0; step < 80 && !guillotineHit; ++step) {
    guillotineHit = contains(obstacles.tick(0.05f), player.id);
  }
  if (!guillotineHit) return 30;

  // The rotating pair belongs to the lower stair approach and still rotates
  // independently of which level is currently rendered.
  bladeA = dynamic_cast<engine::Character*>(world.entities().find("demo-obstacle-blade-a"));
  bladeB = dynamic_cast<engine::Character*>(world.entities().find("demo-obstacle-blade-b"));
  if (!bladeA || !bladeB) return 31;
  if (bladeA->location.levelId != "lower" || bladeB->location.levelId != "lower") return 32;
  if (world.collidesWith(*bladeA, bladeA) || world.collidesWith(*bladeB, bladeB)) return 33;

  obstacles.setEnabled(false);
  if (obstacles.enabled() || obstacles.partCount() != 0) return 34;
  if (world.entities().all().size() != baselineEntityCount) return 35;

  return 0;
}
