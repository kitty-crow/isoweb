#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"
#include "engine/world/Object.hpp"
#include "engine/world/World.hpp"

using namespace isoweb::engine;

namespace {

class FlatLevel final : public IWorldLevel {
public:
  explicit FlatLevel(std::vector<Object> objects = {}) : objects_(std::move(objects)) {
    bounds_.focus = {0.0f, 0.0f, 0.0f};
    bounds_.points = {
      {-6.0f, -6.0f, 0.0f},
      {6.0f, -6.0f, 0.0f},
      {-6.0f, 6.0f, 0.0f},
      {6.0f, 6.0f, 0.0f}
    };
  }

  const WorldBounds& bounds() const override { return bounds_; }
  Vec3 sample(const Ray&, float) const override { return {0.2f, 0.2f, 0.2f}; }

  bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const override {
    if (std::fabs(ray.direction.z) < 1e-7f) return false;
    const float distance = -ray.origin.z / ray.direction.z;
    if (distance <= 0.0f) return false;
    hit.found = true;
    hit.distance = distance;
    hit.point = ray.origin + ray.direction * distance;
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.kind = SceneSurfaceKind::Ground;
    hit.walkable = true;
    return true;
  }

  bool walkableSurfaceAt(float x, float y, SceneSurfaceHit& hit) const override {
    if (x < -6.0f || x > 6.0f || y < -6.0f || y > 6.0f) return false;
    hit.found = true;
    hit.point = {x, y, 0.0f};
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.kind = SceneSurfaceKind::Ground;
    hit.walkable = true;
    return true;
  }

  const std::vector<Object>& objects() const override { return objects_; }

  bool overlapsStatic(std::size_t objectIndex, const Object& candidate) const override {
    return objectIndex < objects_.size() && objects_[objectIndex].overlaps(candidate);
  }

  bool intersectsSolid(const HitBox&) const override { return false; }

private:
  WorldBounds bounds_;
  std::vector<Object> objects_;
};

std::unique_ptr<World> makeWorld(
  const std::vector<std::string>& ids,
  std::vector<std::vector<Object>> staticObjects = {}
) {
  std::vector<std::unique_ptr<IWorldLevel>> levels;
  if (staticObjects.size() < ids.size()) staticObjects.resize(ids.size());
  for (std::size_t index = 0; index < ids.size(); ++index) {
    levels.emplace_back(new FlatLevel(std::move(staticObjects[index])));
  }
  std::unique_ptr<World> world(new World(std::move(levels), 0));
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (!world->setLevelId(index, ids[index])) std::abort();
  }
  return world;
}

Character* addCharacter(
  World& world,
  const std::string& id,
  const std::string& level,
  const Vec3& position
) {
  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = id;
  character->location = {"reliability-world", "default", level, position};
  character->hitBox.minimum = {-0.15f, -0.15f, 0.0f};
  character->hitBox.maximum = {0.15f, 0.15f, 1.20f};
  world.entities().add(std::move(owned));
  return character;
}

Object blocker(const std::string& level, const Vec3& position, float halfX, float halfY) {
  Object object;
  object.id = "static-blocker";
  object.location.levelId = level;
  object.location.position = position;
  object.hitBox.minimum = {-halfX, -halfY, 0.0f};
  object.hitBox.maximum = {halfX, halfY, 1.50f};
  return object;
}

Object ceiling(
  const std::string& level,
  const Vec3& position,
  float halfX,
  float halfY,
  float bottomZ,
  float topZ
) {
  Object object;
  object.id = "static-ceiling";
  object.location.levelId = level;
  object.location.position = position;
  object.hitBox.minimum = {-halfX, -halfY, bottomZ};
  object.hitBox.maximum = {halfX, halfY, topZ};
  return object;
}

NavigationLink connector(
  const std::string& id,
  const std::string& from,
  const std::string& to,
  const Vec3& fromPosition,
  const Vec3& toPosition
) {
  NavigationLink link;
  link.id = id;
  link.fromLevelId = from;
  link.toLevelId = to;
  link.fromPosition = fromPosition;
  link.toPosition = toPosition;
  link.bidirectional = true;
  return link;
}

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "[character-reliability] " << message << '\n';
  std::exit(1);
}

void runUntilSettled(CharacterSystem& characters, Character& character, Camera& camera) {
  for (int tick = 0; tick < 1200 && character.movement.hasDestination; ++tick) {
    characters.tick(0.05f, camera);
  }
}

} // namespace

int main() {
  {
    std::unique_ptr<World> world = makeWorld({"lower", "upper"});
    world->setNavigationLinks({connector(
      "stairs",
      "lower",
      "upper",
      {0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 0.0f}
    )});

    Character* entering = addCharacter(*world, "entering", "lower", {0.0f, 0.0f, 0.0f});
    Character* outside = addCharacter(*world, "outside", "lower", {0.20f, 0.0f, 0.0f});
    entering->location.liminalObjectId = "stairs";
    require(world->collidesWith(*entering, entering), "liminal character ignored overlapping endpoint occupant");
    outside->location.position = {2.0f, 0.0f, 0.0f};
    require(!world->collidesWith(*entering, entering), "liminal collision persisted after separation");
  }

  {
    std::vector<std::vector<Object>> staticObjects(2);
    staticObjects[0].push_back(blocker("A", {0.0f, 0.0f, 0.0f}, 0.65f, 0.65f));
    std::unique_ptr<World> world = makeWorld({"A", "B"}, std::move(staticObjects));

    NavigationLink blocked = connector(
      "blocked-first",
      "A",
      "B",
      {0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 0.0f}
    );
    NavigationLink clear = connector(
      "clear-second",
      "A",
      "B",
      {3.0f, 0.0f, 0.0f},
      {3.0f, 0.0f, 0.0f}
    );
    world->setNavigationLinks({blocked, clear});

    CharacterSystem characters(*world);
    Character* character = addCharacter(*world, "connector-choice", "A", {-3.0f, 0.0f, 0.0f});
    EntityLocation destination = character->location;
    destination.levelId = "B";
    destination.position = {4.0f, 0.0f, 0.0f};
    require(characters.command(*character, destination), "reachable alternative connector was not selected");

    bool usedClearConnector = false;
    for (const CharacterWaypoint& waypoint : character->movement.route) {
      if (
        waypoint.levelTransition &&
        waypoint.location.levelId == "B" &&
        std::fabs(waypoint.location.position.x - 3.0f) < 0.001f
      ) {
        usedClearConnector = true;
      }
    }
    require(usedClearConnector, "route did not use the physically reachable connector");
  }

  {
    std::vector<std::vector<Object>> staticObjects(1);
    staticObjects[0].push_back(blocker("floor", {0.0f, 0.0f, 0.0f}, 0.70f, 0.70f));
    std::unique_ptr<World> world = makeWorld({"floor"}, std::move(staticObjects));
    CharacterSystem characters(*world);
    Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
    Character* runner = addCharacter(*world, "detour-runner", "floor", {-2.5f, 0.0f, 0.0f});

    EntityLocation destination = runner->location;
    destination.position = {2.5f, 0.0f, 0.0f};
    require(characters.command(*runner, destination), "command across a static obstacle was rejected");
    require(!runner->movement.pathBlocked, "planner reported a reachable detour as blocked");
    runUntilSettled(characters, *runner, camera);
    require(!runner->movement.hasDestination, "detour runner never reached the far side of the obstacle");
    require(std::fabs(runner->location.position.x - 2.5f) < 0.05f, "detour runner stopped short of destination");
  }

  {
    std::unique_ptr<World> world = makeWorld({"floor"});
    CharacterSystem characters(*world);
    Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
    Character* runner = addCharacter(*world, "fast-runner", "floor", {0.0f, 0.0f, 0.0f});
    runner->movementSpeedMultiplier = 10.0f;

    EntityLocation destination = runner->location;
    destination.position = {1.45f, 0.0f, 0.0f};
    require(characters.command(*runner, destination), "fast runner route was rejected before blocker insertion");

    std::unique_ptr<Object> dynamicBlocker(new Object());
    dynamicBlocker->id = "late-blocker";
    dynamicBlocker->location = runner->location;
    dynamicBlocker->location.position = {0.70f, 0.0f, 0.0f};
    dynamicBlocker->hitBox.minimum = {-0.05f, -0.80f, 0.0f};
    dynamicBlocker->hitBox.maximum = {0.05f, 0.80f, 1.50f};
    world->entities().add(std::move(dynamicBlocker));

    characters.tick(0.10f, camera);
    require(runner->location.position.x < 0.60f, "fast runner tunnelled through a newly inserted blocker");
    require(!world->collidesWith(*runner, runner), "fast runner ended a tick overlapping the blocker");
    require(runner->movement.hasDestination, "runtime obstruction cancelled the original destination");

    runUntilSettled(characters, *runner, camera);
    require(!runner->movement.hasDestination, "runner failed to replan around a runtime obstruction");
    require(std::fabs(runner->location.position.x - 1.45f) < 0.05f, "replanned runner did not reach its original destination");
  }

  {
    std::vector<std::vector<Object>> staticObjects(1);
    staticObjects[0].push_back(blocker("floor", {0.0f, 0.0f, 0.0f}, 0.25f, 6.50f));
    std::unique_ptr<World> world = makeWorld({"floor"}, std::move(staticObjects));
    CharacterSystem characters(*world);
    Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
    Character* runner = addCharacter(*world, "blocked-runner", "floor", {-2.0f, 0.0f, 0.0f});

    EntityLocation destination = runner->location;
    destination.position = {2.0f, 0.0f, 0.0f};
    require(characters.command(*runner, destination), "valid but unreachable destination was rejected as a command");
    require(runner->movement.hasDestination, "unreachable command did not retain destination intent");
    require(runner->movement.pathBlocked, "unreachable command did not set pathBlocked");
    require(!runner->moving, "blocked runner incorrectly reported physical movement");
    const std::size_t firstFailures = runner->movement.failedPathAttempts;

    characters.tick(0.30f, camera);
    require(runner->movement.hasDestination, "blocked retry discarded destination intent");
    require(runner->movement.pathBlocked, "blocked retry cleared the complaint flag without a route");
    require(runner->movement.failedPathAttempts > firstFailures, "blocked destination was not retried");
    require(std::fabs(runner->movement.destination.position.x - 2.0f) < 0.001f, "blocked retry changed the requested destination");
  }

  {
    std::vector<std::vector<Object>> staticObjects(1);
    // This strip spans beyond the navigable Y bounds, so crossing from left to
    // right is possible only by fitting underneath it. Standing height is 1.2;
    // crouched height 0.8 fits below the 1.0 ceiling.
    staticObjects[0].push_back(ceiling("floor", {0.0f, 0.0f, 0.0f}, 0.45f, 6.50f, 1.0f, 1.35f));
    std::unique_ptr<World> world = makeWorld({"floor"}, std::move(staticObjects));
    CharacterSystem characters(*world);
    Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
    Character* runner = addCharacter(*world, "crouch-runner", "floor", {-2.0f, 0.0f, 0.0f});
    runner->crouchedHeight = 0.80f;

    EntityLocation destination = runner->location;
    destination.position = {2.0f, 0.0f, 0.0f};
    require(characters.command(*runner, destination), "low-clearance command was rejected");
    require(!runner->movement.pathBlocked, "crouchable passage was treated as blocked");

    bool sawCrouch = false;
    bool stoodAfterCrouch = false;
    for (int tick = 0; tick < 1200 && runner->movement.hasDestination; ++tick) {
      characters.tick(0.05f, camera);
      if (runner->crouching) sawCrouch = true;
      if (sawCrouch && !runner->crouching) stoodAfterCrouch = true;
    }
    require(sawCrouch, "Character never crouched under low clearance");
    require(stoodAfterCrouch, "Character did not stand after leaving low clearance");
    require(!runner->crouching, "Character remained crouched in open space");
    require(!runner->movement.hasDestination, "crouching Character did not reach destination");
  }

  {
    std::vector<std::vector<Object>> staticObjects(1);
    staticObjects[0].push_back(ceiling("floor", {0.0f, 0.0f, 0.0f}, 0.45f, 6.50f, 0.65f, 1.35f));
    std::unique_ptr<World> world = makeWorld({"floor"}, std::move(staticObjects));
    CharacterSystem characters(*world);
    Character* runner = addCharacter(*world, "too-tall-runner", "floor", {-2.0f, 0.0f, 0.0f});
    runner->crouchedHeight = 0.80f;

    EntityLocation destination = runner->location;
    destination.position = {2.0f, 0.0f, 0.0f};
    require(characters.command(*runner, destination), "valid low-space destination command was rejected");
    require(runner->movement.pathBlocked, "passage shorter than crouched height was considered navigable");
    require(!runner->moving, "Character tried to enter clearance shorter than crouched height");
  }

  std::cout << "Character reliability regressions passed.\n";
  return 0;
}