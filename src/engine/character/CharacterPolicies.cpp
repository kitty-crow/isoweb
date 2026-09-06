#include "engine/character/CharacterPolicies.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <utility>

#include "engine/world/World.hpp"

namespace isoweb {
namespace engine {
namespace {

struct GridPoint {
  int x = 0;
  int y = 0;

  bool operator<(const GridPoint& other) const {
    return x < other.x || (x == other.x && y < other.y);
  }

  bool operator==(const GridPoint& other) const {
    return x == other.x && y == other.y;
  }
};

struct QueueNode {
  GridPoint point;
  float score = 0.0f;

  bool operator<(const QueueNode& other) const {
    return score > other.score;
  }
};

struct DirectedLink {
  const NavigationLink* link = nullptr;
  bool reverse = false;
};

float distance2(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

Vec3 horizontalDirection(const Vec3& from, const Vec3& to, const Vec3& fallback) {
  const Vec3 delta(to.x - from.x, to.y - from.y, 0.0f);
  const float magnitude = std::sqrt(delta.x * delta.x + delta.y * delta.y);
  return magnitude > 1e-6f ? delta / magnitude : fallback;
}

void boundsXY(const WorldBounds& bounds, float& minX, float& minY, float& maxX, float& maxY) {
  minX = minY = std::numeric_limits<float>::max();
  maxX = maxY = -std::numeric_limits<float>::max();
  for (const Vec3& point : bounds.points) {
    minX = std::min(minX, point.x);
    minY = std::min(minY, point.y);
    maxX = std::max(maxX, point.x);
    maxY = std::max(maxY, point.y);
  }
}

GridPoint toGrid(const Vec3& point, float minX, float minY, float cell) {
  return {
    static_cast<int>(std::round((point.x - minX) / cell)),
    static_cast<int>(std::round((point.y - minY) / cell))
  };
}

Vec3 fromGrid(const GridPoint& point, float minX, float minY, float cell, float z) {
  return {minX + point.x * cell, minY + point.y * cell, z};
}

bool positionBlocked(
  const World& world,
  const Character& character,
  const std::string& levelId,
  const Vec3& position,
  const Vec3& facing
) {
  Character candidate = character;
  candidate.location.levelId = levelId;
  candidate.location.position = position;
  candidate.forward = facing;
  return world.collidesWith(candidate, &character);
}

bool segmentClear(
  const World& world,
  const Character& character,
  const std::string& levelId,
  const Vec3& from,
  const Vec3& to,
  float cell
) {
  const float length = distance2(from, to);
  if (length <= 1e-6f) return true;
  const int steps = std::max(1, static_cast<int>(std::ceil(length / std::max(0.05f, cell * 0.5f))));
  const Vec3 facing = horizontalDirection(from, to, character.forward);
  for (int i = 1; i <= steps; ++i) {
    const float t = static_cast<float>(i) / steps;
    const Vec3 point = from * (1.0f - t) + to * t;
    if (!world.containsPosition(levelId, point) || positionBlocked(world, character, levelId, point, facing)) return false;
  }
  return true;
}

bool sameLevelPath(
  const World& world,
  const Character& character,
  const std::string& levelId,
  const Vec3& start,
  const Vec3& destination,
  const CharacterEngineDefaults& defaults,
  std::vector<CharacterWaypoint>& output
) {
  if (segmentClear(world, character, levelId, start, destination, defaults.navigationCellSize)) {
    CharacterWaypoint waypoint;
    waypoint.location = character.location;
    waypoint.location.levelId = levelId;
    waypoint.location.position = destination;
    output.push_back(waypoint);
    return true;
  }

  float minX, minY, maxX, maxY;
  boundsXY(world.bounds(levelId), minX, minY, maxX, maxY);
  const float cell = std::max(0.08f, defaults.navigationCellSize);
  const int maxGridX = static_cast<int>(std::ceil((maxX - minX) / cell));
  const int maxGridY = static_cast<int>(std::ceil((maxY - minY) / cell));
  const GridPoint startGrid = toGrid(start, minX, minY, cell);
  const GridPoint goalGrid = toGrid(destination, minX, minY, cell);

  std::priority_queue<QueueNode> open;
  std::map<GridPoint, float> cost;
  std::map<GridPoint, GridPoint> parent;
  std::set<GridPoint> closed;
  open.push({startGrid, 0.0f});
  cost[startGrid] = 0.0f;

  const int directions[8][2] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1},
    {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
  };

  bool found = false;
  while (!open.empty()) {
    const GridPoint current = open.top().point;
    open.pop();
    if (closed.find(current) != closed.end()) continue;
    closed.insert(current);
    if (current == goalGrid) {
      found = true;
      break;
    }

    for (const auto& direction : directions) {
      GridPoint next{current.x + direction[0], current.y + direction[1]};
      if (next.x < 0 || next.y < 0 || next.x > maxGridX || next.y > maxGridY) continue;
      if (closed.find(next) != closed.end()) continue;

      const Vec3 currentPoint = fromGrid(current, minX, minY, cell, start.z);
      const Vec3 nextPoint = fromGrid(next, minX, minY, cell, start.z);
      const Vec3 facing = horizontalDirection(currentPoint, nextPoint, character.forward);
      if (!world.containsPosition(levelId, nextPoint) || positionBlocked(world, character, levelId, nextPoint, facing)) continue;

      const float step = (direction[0] != 0 && direction[1] != 0) ? 1.41421356f : 1.0f;
      const float nextCost = cost[current] + step;
      auto existing = cost.find(next);
      if (existing != cost.end() && existing->second <= nextCost) continue;
      cost[next] = nextCost;
      parent[next] = current;
      const float heuristic = static_cast<float>(std::abs(goalGrid.x - next.x) + std::abs(goalGrid.y - next.y));
      open.push({next, nextCost + heuristic});
    }
  }

  if (!found) return false;

  std::vector<Vec3> reversed;
  GridPoint cursor = goalGrid;
  while (!(cursor == startGrid)) {
    reversed.push_back(fromGrid(cursor, minX, minY, cell, start.z));
    const auto it = parent.find(cursor);
    if (it == parent.end()) return false;
    cursor = it->second;
  }
  std::reverse(reversed.begin(), reversed.end());
  if (!reversed.empty()) reversed.back() = destination;

  // Cheap line-of-sight smoothing keeps visible movement continuous rather
  // than exposing the pathfinder's grid.
  Vec3 anchor = start;
  std::size_t index = 0;
  while (index < reversed.size()) {
    std::size_t furthest = index;
    for (std::size_t candidate = index; candidate < reversed.size(); ++candidate) {
      if (segmentClear(world, character, levelId, anchor, reversed[candidate], cell)) furthest = candidate;
      else break;
    }
    CharacterWaypoint waypoint;
    waypoint.location = character.location;
    waypoint.location.levelId = levelId;
    waypoint.location.position = reversed[furthest];
    output.push_back(waypoint);
    anchor = reversed[furthest];
    index = furthest + 1;
  }
  return true;
}

std::vector<DirectedLink> levelRoute(const World& world, const std::string& from, const std::string& to) {
  if (from == to) return {};
  struct Parent {
    std::string previous;
    DirectedLink link;
  };

  std::queue<std::string> pending;
  std::set<std::string> visited;
  std::map<std::string, Parent> parents;
  pending.push(from);
  visited.insert(from);

  while (!pending.empty()) {
    const std::string level = pending.front();
    pending.pop();
    for (const NavigationLink& link : world.navigationLinks()) {
      if (link.fromLevelId == level && visited.insert(link.toLevelId).second) {
        parents[link.toLevelId] = {level, {&link, false}};
        if (link.toLevelId == to) break;
        pending.push(link.toLevelId);
      }
      if (link.bidirectional && link.toLevelId == level && visited.insert(link.fromLevelId).second) {
        parents[link.fromLevelId] = {level, {&link, true}};
        if (link.fromLevelId == to) break;
        pending.push(link.fromLevelId);
      }
    }
    if (visited.find(to) != visited.end()) break;
  }

  if (visited.find(to) == visited.end()) return {};
  std::vector<DirectedLink> reversed;
  std::string cursor = to;
  while (cursor != from) {
    const auto parent = parents.find(cursor);
    if (parent == parents.end()) return {};
    reversed.push_back(parent->second.link);
    cursor = parent->second.previous;
  }
  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

void appendTraversal(
  const Character& character,
  const std::string& levelId,
  const std::vector<Vec3>& points,
  std::vector<CharacterWaypoint>& output
) {
  for (const Vec3& point : points) {
    CharacterWaypoint waypoint;
    waypoint.location = character.location;
    waypoint.location.levelId = levelId;
    waypoint.location.position = point;
    output.push_back(waypoint);
  }
}

} // namespace

EntityLocation DestinationPolicy::resolve(
  const World& world,
  const Character& character,
  const EntityLocation& requested,
  const CharacterEngineDefaults& defaults
) const {
  EntityLocation resolved = requested;
  if (resolved.worldId.empty()) resolved.worldId = character.location.worldId;
  if (resolved.timelineId.empty()) resolved.timelineId = character.location.timelineId;
  if (resolved.levelId.empty()) resolved.levelId = character.location.levelId;

  Character probe = character;
  probe.location = resolved;
  if (world.containsPosition(resolved.levelId, resolved.position) && !world.collidesWith(probe, &character)) return resolved;

  const float step = std::max(0.08f, defaults.navigationCellSize);
  const int rings = std::max(1, static_cast<int>(std::ceil(defaults.destinationSearchRadius / step)));
  for (int ring = 1; ring <= rings; ++ring) {
    const int radius = ring;
    for (int x = -radius; x <= radius; ++x) {
      for (int y = -radius; y <= radius; ++y) {
        if (std::max(std::abs(x), std::abs(y)) != radius) continue;
        resolved.position = requested.position + Vec3(x * step, y * step, 0.0f);
        probe.location = resolved;
        if (world.containsPosition(resolved.levelId, resolved.position) && !world.collidesWith(probe, &character)) return resolved;
      }
    }
  }
  return character.location;
}

bool DefaultNavigationPolicy::buildRoute(
  const World& world,
  const Character& character,
  const EntityLocation& destination,
  const CharacterEngineDefaults& defaults,
  CharacterMovementState& route
) const {
  route.clear();
  route.destination = destination;

  if (character.location.worldId != destination.worldId || character.location.timelineId != destination.timelineId) return false;

  std::vector<CharacterWaypoint> waypoints;
  std::string currentLevel = character.location.levelId;
  Vec3 currentPosition = character.location.position;

  if (currentLevel != destination.levelId) {
    const std::vector<DirectedLink> links = levelRoute(world, currentLevel, destination.levelId);
    if (links.empty()) return false;

    for (const DirectedLink& directed : links) {
      const NavigationLink& link = *directed.link;
      const Vec3 approach = directed.reverse ? link.toPosition : link.fromPosition;
      const std::string nextLevel = directed.reverse ? link.fromLevelId : link.toLevelId;
      const Vec3 arrival = directed.reverse ? link.fromPosition : link.toPosition;
      const std::vector<Vec3>& traversal = directed.reverse ? link.reverseTraversal : link.forwardTraversal;

      if (!sameLevelPath(world, character, currentLevel, currentPosition, approach, defaults, waypoints)) return false;
      appendTraversal(character, currentLevel, traversal, waypoints);

      CharacterWaypoint transition;
      transition.location = character.location;
      transition.location.levelId = nextLevel;
      transition.location.position = arrival;
      transition.levelTransition = true;
      waypoints.push_back(transition);

      currentLevel = nextLevel;
      currentPosition = arrival;
    }
  }

  if (!sameLevelPath(world, character, currentLevel, currentPosition, destination.position, defaults, waypoints)) return false;
  route.route = std::move(waypoints);
  route.nextWaypoint = 0;
  route.hasDestination = !route.route.empty();
  return route.hasDestination;
}

} // namespace engine
} // namespace isoweb
