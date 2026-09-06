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

  std::cout << "Performance architecture smoke test passed: one primary trace, any-hit shadows, cached entity views.\n";
  return 0;
}
