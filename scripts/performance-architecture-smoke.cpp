#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"
#include "engine/world/World.hpp"

using namespace isoweb::engine;

namespace {

class CountingLevel final : public IWorldLevel {
public:
  mutable int legacySampleCalls = 0;
  mutable int combinedSampleCalls = 0;
  mutable int traceCalls = 0;
  mutable int occlusionCalls = 0;

  CountingLevel() {
    bounds_.focus = {0.0f, 0.0f, 0.0f};
    bounds_.points = {
      {-10.0f, -10.0f, 0.0f},
      {10.0f, -10.0f, 0.0f},
      {-10.0f, 10.0f, 0.0f},
      {10.0f, 10.0f, 0.0f}
    };
  }

  const WorldBounds& bounds() const override { return bounds_; }

  Vec3 sample(const Ray&, float) const override {
    ++legacySampleCalls;
    return {0.1f, 0.2f, 0.3f};
  }

  Vec3 sampleWithHit(const Ray& ray, float, SceneSurfaceHit& hit) const override {
    ++combinedSampleCalls;
    hit.found = true;
    hit.distance = 10.0f;
    hit.point = ray.origin + ray.direction * hit.distance;
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.colour = {0.1f, 0.2f, 0.3f};
    hit.kind = SceneSurfaceKind::Ground;
    hit.walkable = true;
    return hit.colour;
  }

  bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const override {
    ++traceCalls;
    hit.found = true;
    hit.distance = 10.0f;
    hit.point = ray.origin + ray.direction * hit.distance;
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.kind = SceneSurfaceKind::Ground;
    hit.walkable = true;
    return true;
  }

  bool rayOccluded(const Ray&, float) const override {
    ++occlusionCalls;
    return false;
  }

  bool walkableSurfaceAt(float x, float y, SceneSurfaceHit& hit) const override {
    hit.found = true;
    hit.distance = 1.0f;
    hit.point = {x, y, 0.0f};
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.kind = SceneSurfaceKind::Ground;
    hit.walkable = true;
    return true;
  }

  const std::vector<Object>& objects() const override { return objects_; }
  bool overlapsStatic(std::size_t, const Object&) const override { return false; }
  bool intersectsSolid(const HitBox&) const override { return false; }

private:
  WorldBounds bounds_;
  std::vector<Object> objects_;
};

} // namespace

int main() {
  std::unique_ptr<CountingLevel> counted(new CountingLevel());
  CountingLevel* level = counted.get();
  std::vector<std::unique_ptr<IWorldLevel>> levels;
  levels.push_back(std::move(counted));
  World world(std::move(levels), 0);
  if (!world.setLevelId(0, "test")) return 1;
  world.setLevelLight("test", {5.0f, 0.0f, 6.0f});

  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = "perf-character";
  character->location = {"test-world", "default", "test", {0.0f, 0.0f, 0.0f}};
  character->hitBox.minimum = {-1.0f, -1.0f, 0.0f};
  character->hitBox.maximum = {1.0f, 1.0f, 2.0f};
  world.entities().add(std::move(owned));

  const auto* firstView = &world.entities().characters();
  const auto* secondView = &world.entities().characters();
  if (firstView != secondView || firstView->size() != 1) return 2;

  CharacterSystem characters(world);
  world.prepareRenderFrame({0.0f, 0.0f, -1.0f});
  const Vec3 colour = world.sample({{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}}, 0.5f);
  (void)colour;

  // One camera sample must traverse the static scene exactly once. The old
  // path traced for environmentDistance and then traced again for colour.
  if (level->combinedSampleCalls != 1) return 3;
  if (level->legacySampleCalls != 0) return 4;
  if (level->traceCalls != 0) return 5;

  // Runtime Character lighting must ask the level's any-hit shadow path rather
  // than performing another fully-described closest-hit scene traversal.
  if (level->occlusionCalls != 1) return 6;

  // Dynamic boxes now maintain a cached orthographic screen-space broad phase.
  // A ray through the centre of 257 widely-spaced moving objects should only
  // consider the one object whose projected bounds cover that ray.
  const Ray centreRay{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}};
  std::vector<Object> probes(257);
  int possibleHits = 0;
  for (int index = 0; index < static_cast<int>(probes.size()); ++index) {
    Object& probe = probes[static_cast<std::size_t>(index)];
    probe.location.position = {
      static_cast<float>(index - 128) * 2.0f,
      0.0f,
      0.0f
    };
    probe.hitBox.minimum = {-0.40f, -0.40f, 0.0f};
    probe.hitBox.maximum = {0.40f, 0.40f, 1.0f};
    if (probe.rayMayHit(centreRay)) ++possibleHits;
  }
  if (possibleHits != 1) return 7;

  // Moving an object invalidates its cached projection immediately. This is
  // essential for moving hazards: broad-phase speed cannot come from stale
  // bounds or missed collisions.
  Object movingProbe;
  movingProbe.hitBox.minimum = {-0.40f, -0.40f, 0.0f};
  movingProbe.hitBox.maximum = {0.40f, 0.40f, 1.0f};
  movingProbe.location.position = {0.0f, 0.0f, 0.0f};
  if (!movingProbe.rayMayHit(centreRay)) return 8;
  movingProbe.location.position = {50.0f, 0.0f, 0.0f};
  if (movingProbe.rayMayHit(centreRay)) return 9;

  // Tiled surface coordinates are based on a fixed world-unit tile size, not
  // the current dimensions of the hit box. Resizing a moving wall therefore
  // cannot stretch a future texture.
  Object tiled;
  tiled.surfaceTextureMode = SurfaceTextureMode::TileLocal;
  tiled.textureWorldUnitsPerTile = 0.50f;
  tiled.hitBox.minimum = {-1.0f, -0.10f, 0.0f};
  tiled.hitBox.maximum = {1.0f, 0.10f, 1.0f};
  ObjectRayHit tiledHit;
  tiledHit.face = ObjectFace::Front;
  tiledHit.localPoint = {0.125f, 0.10f, 0.375f};
  tiledHit.worldPoint = tiled.localToWorld(tiledHit.localPoint);
  const SurfaceUV beforeResize = tiled.surfaceTextureUV(tiledHit);
  tiled.hitBox.minimum.x = -8.0f;
  tiled.hitBox.maximum.x = 8.0f;
  const SurfaceUV afterResize = tiled.surfaceTextureUV(tiledHit);
  if (
    std::fabs(beforeResize.u - afterResize.u) > 1e-6f ||
    std::fabs(beforeResize.v - afterResize.v) > 1e-6f
  ) {
    return 10;
  }

  std::cout << "Performance architecture smoke test passed: one primary trace, any-hit shadows, cached entity views, dynamic broad phase and fixed-density tiled UVs.\n";
  return 0;
}
