#include <cmath>
#include <iostream>
#include <memory>

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"

namespace {

float distance(const isoweb::engine::Vec3& a, const isoweb::engine::Vec3& b) {
  const auto delta = a - b;
  return std::sqrt(isoweb::engine::dot(delta, delta));
}

float brightness(const isoweb::engine::Vec3& value) {
  return value.x + value.y + value.z;
}

} // namespace

int main() {
  using namespace isoweb::engine;

  isoweb::demo::DemoWorld world;
  if (world.liminalObjects().size() != 2) return 1;
  if (world.residentLevelCount() != 1) return 26;
  if (!world.isLevelResident("middle")) return 27;
  if (world.isLevelResident("lower") || world.isLevelResident("upper")) return 28;

  const LiminalObject& stairs = world.liminalObjects().front();
  if (stairs.category != "liminal" || stairs.id.empty()) return 2;
  if (stairs.forwardTraversal.empty() || stairs.reverseTraversal.empty()) return 3;
  if (!stairs.hasViewOffset) return 4;

  const std::size_t sampleIndex = stairs.forwardTraversal.size() / 2;
  EntityLocation liminalLocation;
  liminalLocation.levelId = stairs.fromLevelId;
  liminalLocation.position = stairs.forwardTraversal[sampleIndex];
  liminalLocation.liminalObjectId = stairs.id;

  Vec3 mapped;
  if (!world.mapLiminalPosition(liminalLocation, stairs.toLevelId, mapped)) return 5;
  const Vec3 expected = stairs.reverseTraversal[stairs.reverseTraversal.size() - 1 - sampleIndex];
  if (distance(mapped, expected) > 0.002f) return 6;

  Character character;
  character.location = liminalLocation;
  character.hitBox = {{-0.25f, -0.20f, 0.0f}, {0.25f, 0.20f, 1.60f}};

  // DemoWorld starts on middle. The lower<->middle stair Character must be
  // visible from middle while its simulation still uses lower coordinates.
  Vec3 renderPosition;
  if (world.activeLevelId() != stairs.toLevelId) return 7;
  if (!world.renderPositionFor(character, renderPosition)) return 8;
  if (distance(renderPosition, expected) > 0.002f) return 9;

  // Looking at an unrelated level must not project this connector into it.
  // Only that newly requested level may remain render-resident.
  if (!world.levelUp()) return 10;
  if (world.renderPositionFor(character, renderPosition)) return 11;
  if (world.residentLevelCount() != 1) return 29;
  if (!world.isLevelResident("upper") || world.isLevelResident("middle")) return 30;
  if (!world.levelDown()) return 12;
  if (world.residentLevelCount() != 1) return 31;
  if (!world.isLevelResident("middle") || world.isLevelResident("upper")) return 32;

  world.setLevelLight("middle", {-3.60f, -4.20f, 6.50f});

  // This point lies in the real sphere's shadow volume for the middle-level
  // point light. It is outside the sphere itself, so a shadow ray must be
  // blocked by the actual rendered sphere geometry.
  const Vec3 shadowedPoint(1.80f, 0.385f, 0.80f);
  const Vec3 clearPoint(-3.00f, -3.00f, 0.80f);
  if (world.runtimeLightVisibility("middle", shadowedPoint) > 0.01f) return 13;
  if (world.runtimeLightVisibility("middle", clearPoint) < 0.99f) return 14;

  const Vec3 baseColour(0.8f, 0.7f, 0.6f);
  const Vec3 normal(0.0f, 0.0f, 1.0f);
  const Vec3 shadowed = world.shadeRuntimeSurface("middle", shadowedPoint, normal, baseColour);
  const Vec3 clear = world.shadeRuntimeSurface("middle", clearPoint, normal, baseColour);
  if (brightness(shadowed) >= brightness(clear)) return 15;

  // Reproduce the real interaction: a Character starts on middle, is ordered
  // to lower, enters the shared staircase, and remains visible if the viewed
  // level is switched to lower while it is still physically between levels.
  CharacterSystem characters(world);
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));

  std::unique_ptr<Character> owned(new Character());
  Character* runner = owned.get();
  runner->id = "liminal-runner";
  runner->location = {"demo", "default", "middle", {0.0f, 2.40f, 0.0f}};
  runner->hitBox = {{-0.28f, -0.20f, 0.0f}, {0.28f, 0.20f, 1.65f}};
  runner->forward = {0.0f, 1.0f, 0.0f};
  world.entities().add(std::move(owned));

  EntityLocation lowerDestination = runner->location;
  lowerDestination.levelId = "lower";
  lowerDestination.position = {0.0f, 2.40f, 0.0f};
  if (!characters.command(*runner, lowerDestination)) return 16;

  bool enteredLiminal = false;
  for (int tick = 0; tick < 1200 && runner->moving; ++tick) {
    characters.tick(0.05f, camera);
    // Do not accept the stair landing as proof. The runner must actually be
    // below the middle floor while still using the middle coordinate frame.
    if (
      runner->location.liminalObjectId.empty() ||
      runner->location.levelId != "middle" ||
      runner->location.position.z >= -0.05f
    ) {
      continue;
    }

    enteredLiminal = true;
    if (!world.renderPositionFor(*runner, renderPosition)) return 17;

    // Change only the viewed level. Simulation stays on the connector. The
    // middle render level loses residency and only lower becomes resident.
    if (!world.levelDown()) return 18;
    if (world.activeLevelId() != "lower") return 19;
    if (world.residentLevelCount() != 1) return 33;
    if (!world.isLevelResident("lower") || world.isLevelResident("middle")) return 34;
    if (!world.renderPositionFor(*runner, renderPosition)) return 20;
    break;
  }
  if (!enteredLiminal) return 21;

  // Keep simulating while the lower endpoint is viewed. The Character must
  // complete the same traversal and settle into lower as an ordinary entity.
  for (int tick = 0; tick < 1200 && runner->moving; ++tick) {
    characters.tick(0.05f, camera);
  }
  if (runner->moving) return 22;
  if (runner->location.levelId != "lower") return 23;
  if (!runner->location.liminalObjectId.empty()) return 24;
  if (!world.renderPositionFor(*runner, renderPosition)) return 25;
  if (world.residentLevelCount() != 1 || !world.isLevelResident("lower")) return 35;

  std::cout << "Liminal-space, single-level residency, and runtime-lighting smoke test passed.\n";
  return 0;
}
