#include <cmath>
#include <iostream>

#include "demo/DemoWorld.hpp"
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

  // Looking at an unrelated level must not load/project this connector.
  if (!world.levelUp()) return 10;
  if (world.renderPositionFor(character, renderPosition)) return 11;
  if (!world.levelDown()) return 12;

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

  std::cout << "Liminal-space and runtime-lighting smoke test passed.\n";
  return 0;
}
