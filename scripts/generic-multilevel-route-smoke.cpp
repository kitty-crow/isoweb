#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"
#include "engine/world/World.hpp"

using namespace isoweb::engine;

namespace {

class FlatLevel final : public IWorldLevel {
public:
  FlatLevel() {
    bounds_.focus = {0.0f, 0.0f, 0.0f};
    bounds_.points = {
      {-10.0f, -10.0f, 0.0f},
      {10.0f, -10.0f, 0.0f},
      {-10.0f, 10.0f, 0.0f},
      {10.0f, 10.0f, 0.0f}
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
    if (x < -10.0f || x > 10.0f || y < -10.0f || y > 10.0f) return false;
    hit.found = true;
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

std::unique_ptr<World> makeWorld(
  const std::vector<std::string>& ids,
  const std::vector<NavigationLink>& links,
  std::size_t defaultIndex = 0
) {
  std::vector<std::unique_ptr<IWorldLevel>> levels;
  for (std::size_t index = 0; index < ids.size(); ++index) {
    levels.emplace_back(new FlatLevel());
  }
  std::unique_ptr<World> world(new World(std::move(levels), defaultIndex));
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (!world->setLevelId(index, ids[index])) std::abort();
  }
  world->setNavigationLinks(links);
  return world;
}

Character* addCharacter(World& world, const std::string& id, const std::string& level) {
  std::unique_ptr<Character> owned(new Character());
  Character* character = owned.get();
  character->id = id;
  character->location = {"generic-world", "default", level, {0.0f, 0.0f, 0.0f}};
  character->hitBox.minimum = {-0.18f, -0.18f, 0.0f};
  character->hitBox.maximum = {0.18f, 0.18f, 1.20f};
  world.entities().add(std::move(owned));
  return character;
}

NavigationLink link(
  const std::string& id,
  const std::string& from,
  const std::string& to,
  bool bidirectional = true
) {
  NavigationLink value;
  value.id = id;
  value.fromLevelId = from;
  value.toLevelId = to;
  value.fromPosition = {0.0f, 0.0f, 0.0f};
  value.toPosition = {0.0f, 0.0f, 0.0f};
  value.bidirectional = bidirectional;
  return value;
}

std::vector<NavigationLink> chain(const std::vector<std::string>& ids) {
  std::vector<NavigationLink> links;
  for (std::size_t index = 1; index < ids.size(); ++index) {
    links.push_back(link(
      "edge-" + std::to_string(index - 1) + "-" + std::to_string(index),
      ids[index - 1],
      ids[index]
    ));
  }
  return links;
}

std::size_t transitionCount(const Character& character) {
  std::size_t count = 0;
  for (const CharacterWaypoint& waypoint : character.movement.route) {
    if (waypoint.levelTransition) ++count;
  }
  return count;
}

bool commandRoute(
  const std::vector<std::string>& ids,
  const std::vector<NavigationLink>& links,
  const std::string& from,
  const std::string& to,
  std::size_t expectedTransitions
) {
  std::unique_ptr<World> world = makeWorld(ids, links);
  CharacterSystem characters(*world);
  Character* character = addCharacter(*world, "route-character", from);
  EntityLocation destination = character->location;
  destination.levelId = to;

  if (!characters.command(*character, destination)) return false;
  return !character->movement.pathBlocked && transitionCount(*character) == expectedTransitions;
}

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "[generic-multilevel-route] " << message << '\n';
  std::exit(1);
}

} // namespace

int main() {
  const std::vector<std::string> one = {"only::level"};
  require(commandRoute(one, {}, one.front(), one.front(), 0), "single arbitrary level failed");

  const std::vector<std::string> two = {"alpha-room", "omega-room"};
  require(commandRoute(two, chain(two), two.front(), two.back(), 1), "two-level forward route failed");
  require(commandRoute(two, chain(two), two.back(), two.front(), 1), "two-level reverse route failed");

  const std::vector<std::string> five = {
    "basement-A", "mezzanine.7", "gallery/x", "roof garden", "observatory"
  };
  require(commandRoute(five, chain(five), five.front(), five.back(), 4), "five-level route failed");

  const std::vector<std::string> twelve = {
    "n11", "n03", "n27", "n04", "n51", "n06",
    "n72", "n08", "n99", "n10", "n42", "n12"
  };
  const std::vector<NavigationLink> twelveLinks = chain(twelve);
  require(commandRoute(twelve, twelveLinks, twelve.front(), twelve.back(), 11), "twelve-level forward route failed");
  require(commandRoute(twelve, twelveLinks, twelve.back(), twelve.front(), 11), "twelve-level reverse route failed");

  const std::vector<std::string> shuffled = {"L3", "L0", "L2", "L1"};
  std::vector<NavigationLink> graph;
  graph.push_back(link("01", "L0", "L1"));
  graph.push_back(link("12", "L1", "L2"));
  graph.push_back(link("23", "L2", "L3"));
  graph.push_back(link("02-shortcut", "L0", "L2"));
  graph.push_back(link("20-cycle", "L2", "L0"));
  require(commandRoute(shuffled, graph, "L0", "L3", 2), "branch/cycle shortest route failed");
  require(commandRoute(shuffled, graph, "L3", "L0", 2), "reverse branch/cycle route failed");

  const std::vector<std::string> oneWayIds = {"west", "east"};
  const std::vector<NavigationLink> oneWayLinks = {link("west-east", "west", "east", false)};
  require(commandRoute(oneWayIds, oneWayLinks, "west", "east", 1), "one-way forward route failed");
  {
    std::unique_ptr<World> world = makeWorld(oneWayIds, oneWayLinks);
    CharacterSystem characters(*world);
    Character* character = addCharacter(*world, "one-way-reverse", "east");
    EntityLocation destination = character->location;
    destination.levelId = "west";
    require(characters.command(*character, destination), "one-way reverse command was rejected");
    require(character->movement.hasDestination, "one-way reverse did not retain destination intent");
    require(character->movement.pathBlocked, "one-way reverse did not report an unavailable route");
    require(transitionCount(*character) == 0, "one-way reverse invented a forbidden transition");
  }

  {
    const std::vector<std::string> disconnectedIds = {"island-a", "island-b", "island-c"};
    const std::vector<NavigationLink> disconnectedLinks = {link("ab", "island-a", "island-b")};
    std::unique_ptr<World> world = makeWorld(disconnectedIds, disconnectedLinks);
    CharacterSystem characters(*world);
    Character* character = addCharacter(*world, "disconnected", "island-a");
    EntityLocation destination = character->location;
    destination.levelId = "island-c";
    require(characters.command(*character, destination), "disconnected command was rejected");
    require(character->movement.hasDestination, "disconnected command did not retain destination intent");
    require(character->movement.pathBlocked, "disconnected destination did not report an unavailable route");
    require(transitionCount(*character) == 0, "disconnected destination invented a transition");
  }

  {
    std::unique_ptr<World> world = makeWorld(twelve, twelveLinks, 5);
    CharacterSystem characters(*world);
    Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
    Character* character = addCharacter(*world, "off-view", twelve.front());
    EntityLocation destination = character->location;
    destination.levelId = twelve.back();
    require(characters.command(*character, destination), "off-view route command failed");
    require(world->residentLevelCount() == 1, "world did not start with exactly one resident level");
    const std::string activeBefore = world->activeLevelId();
    for (int tick = 0; tick < 5000 && character->moving; ++tick) {
      characters.tick(0.05f, camera);
    }
    require(!character->moving, "off-view route never settled");
    require(character->location.levelId == twelve.back(), "off-view character stopped on wrong level");
    require(world->residentLevelCount() == 1, "off-view simulation changed render residency count");
    require(world->activeLevelId() == activeBefore, "off-view simulation changed active render level");
  }

  std::cout << "Generic arbitrary-level routing regression passed.\n";
  return 0;
}