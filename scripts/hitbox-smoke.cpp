#include <iostream>

#include "demo/DemoWorld.hpp"
#include "engine/world/WorldObject.hpp"

using isoweb::engine::HitBox;
using isoweb::engine::WorldObject;

namespace {

HitBox box(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
  HitBox value;
  value.minimum = {minX, minY, minZ};
  value.maximum = {maxX, maxY, maxZ};
  return value;
}

} // namespace

int main() {
  const HitBox origin = box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  const HitBox overlap = box(0.75f, 0.25f, 0.25f, 1.25f, 0.75f, 0.75f);
  const HitBox separate = box(1.25f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f);
  const HitBox touching = box(1.0f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f);

  if (!origin.intersects(overlap)) return 1;
  if (origin.intersects(separate)) return 2;
  if (origin.intersects(touching)) return 3;

  WorldObject solid;
  solid.solid = true;
  solid.hitBox = origin;
  if (!solid.blocks(overlap)) return 4;

  WorldObject passable = solid;
  passable.solid = false;
  if (passable.blocks(overlap)) return 5;

  if (!origin.contains({0.5f, 0.5f, 0.5f})) return 6;
  if (!origin.contains({1.0f, 1.0f, 1.0f})) return 7;
  if (origin.contains({1.01f, 0.5f, 0.5f})) return 8;

  isoweb::demo::DemoWorld world;
  if (world.activeLevelIndex() != 1 || world.objects().size() != 2) return 9;
  for (const WorldObject& object : world.objects()) {
    if (!object.solid) return 10;
  }

  const HitBox middleCube = box(-1.10f, 0.60f, 0.10f, -1.00f, 0.70f, 0.20f);
  const HitBox openFloor = box(3.50f, 3.50f, 0.10f, 3.60f, 3.60f, 0.20f);
  if (!world.intersectsSolid(middleCube)) return 11;
  if (world.intersectsSolid(openFloor)) return 12;

  if (!world.levelDown() || world.activeLevelIndex() != 0 || world.objects().size() != 2) return 13;
  const HitBox lowerCone = box(-1.35f, -0.85f, 0.10f, -1.25f, -0.75f, 0.20f);
  if (!world.intersectsSolid(lowerCone)) return 14;

  if (!world.levelUp() || !world.levelUp() || world.activeLevelIndex() != 2) return 15;
  if (world.objects().size() != 2) return 16;
  const HitBox upperDodecahedron = box(-1.40f, 0.90f, 0.10f, -1.30f, 1.00f, 0.20f);
  if (!world.intersectsSolid(upperDodecahedron)) return 17;

  std::cout << "World-object hitbox and active-level collision smoke test passed.\n";
  return 0;
}
