#include "engine/navigation/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace isoweb {
namespace engine {
namespace {

struct QueueNode {
  int index;
  float score;

  bool operator<(const QueueNode& other) const {
    return score > other.score;
  }
};

struct PlannerGrid {
  float minimumX;
  float maximumX;
  float minimumY;
  float maximumY;
  float step;
  int width;
  int height;

  Vec3 point(int index, float z) const {
    const int x = index % width;
    const int y = index / width;
    return {
      minimumX + x * step,
      minimumY + y * step,
      z
    };
  }

  int index(int x, int y) const {
    return y * width + x;
  }

  bool valid(int x, int y) const {
    return x >= 0 && y >= 0 && x < width && y < height;
  }
};

struct LocalPathResult {
  std::vector<EntityLocation> waypoints;
  EntityLocation resolvedDestination;
  bool reachedRequestedDestination = false;
};

struct ContextState {
  EntityLocation location;
  int parent = -1;
  int connection = -1;
  bool reverse = false;
};

float distance2D(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

Vec3 horizontalDirection(const Vec3& from, const Vec3& to, const Vec3& fallback) {
  Vec3 direction(to.x - from.x, to.y - from.y, 0.0f);
  const float magnitude = std::sqrt(direction.x * direction.x + direction.y * direction.y);
  if (magnitude > 1e-6f) return direction / magnitude;
  return fallback;
}

EntityLocation resolveContext(const EntityLocation& base, EntityLocation value) {
  if (value.worldId.empty()) value.worldId = base.worldId;
  if (value.timelineId.empty()) value.timelineId = base.timelineId;
  if (value.levelId.empty()) value.levelId = base.levelId;
  return value;
}

bool sameContext(const EntityLocation& left, const EntityLocation& right) {
  const EntityLocation resolved = resolveContext(left, right);
  return left.worldId == resolved.worldId &&
    left.timelineId == resolved.timelineId &&
    left.levelId == resolved.levelId;
}

std::string contextKey(const EntityLocation& value) {
  return value.worldId + "\x1f" + value.timelineId + "\x1f" + value.levelId;
}

PlannerGrid makeGrid(
  const IWorld& world,
  const Character& character,
  const std::string& levelId
) {
  float minimumX = 1e9f;
  float maximumX = -1e9f;
  float minimumY = 1e9f;
  float maximumY = -1e9f;

  for (const Vec3& point : world.boundsForLevel(levelId).points) {
    minimumX = std::min(minimumX, point.x);
    maximumX = std::max(maximumX, point.x);
    minimumY = std::min(minimumY, point.y);
    maximumY = std::max(maximumY, point.y);
  }

  if (minimumX > maximumX || minimumY > maximumY) {
    minimumX = -4.0f;
    maximumX = 4.0f;
    minimumY = -4.0f;
    maximumY = 4.0f;
  }

  const Vec3 half = character.hitBox.halfExtent();
  const float horizontalRadius = std::sqrt(half.x * half.x + half.y * half.y);
  minimumX += horizontalRadius;
  maximumX -= horizontalRadius;
  minimumY += horizontalRadius;
  maximumY -= horizontalRadius;

  const Vec3 dimensions = character.hitBox.size();
  const float smallestHorizontalDimension = std::max(
    0.05f,
    std::min(std::fabs(dimensions.x), std::fabs(dimensions.y))
  );
  const float step = std::max(0.14f, std::min(0.28f, smallestHorizontalDimension * 0.5f));
  const int width = std::max(1, static_cast<int>(std::floor((maximumX - minimumX) / step)) + 1);
  const int height = std::max(1, static_cast<int>(std::floor((maximumY - minimumY) / step)) + 1);

  return {
    minimumX,
    maximumX,
    minimumY,
    maximumY,
    step,
    width,
    height
  };
}

bool inside(const PlannerGrid& grid, const Vec3& point) {
  return point.x >= grid.minimumX && point.x <= grid.maximumX &&
    point.y >= grid.minimumY && point.y <= grid.maximumY;
}

bool canOccupy(
  const IWorld& world,
  const Character& original,
  const PlannerGrid& grid,
  const EntityLocation& location,
  const Vec3& forward
) {
  if (!inside(grid, location.position)) return false;

  Character candidate = original;
  candidate.location = location;
  candidate.forward = forward;
  return world.navigationAllows(candidate);
}

bool segmentClear(
  const IWorld& world,
  const Character& character,
  const PlannerGrid& grid,
  const EntityLocation& from,
  const EntityLocation& to
) {
  if (!sameContext(from, to)) return false;

  const float distance = length(to.position - from.position);
  if (distance < 1e-6f) {
    return canOccupy(world, character, grid, to, character.forward);
  }

  const Vec3 direction = horizontalDirection(from.position, to.position, character.forward);
  const int steps = std::max(
    1,
    static_cast<int>(std::ceil(distance / (grid.step * 0.45f)))
  );

  for (int index = 1; index <= steps; ++index) {
    const float t = static_cast<float>(index) / steps;
    EntityLocation location = from;
    location.position = from.position + (to.position - from.position) * t;
    if (!canOccupy(world, character, grid, location, direction)) return false;
  }
  return true;
}

int nearestIndex(const PlannerGrid& grid, const Vec3& point) {
  const int x = std::max(
    0,
    std::min(
      grid.width - 1,
      static_cast<int>(std::lround((point.x - grid.minimumX) / grid.step))
    )
  );
  const int y = std::max(
    0,
    std::min(
      grid.height - 1,
      static_cast<int>(std::lround((point.y - grid.minimumY) / grid.step))
    )
  );
  return grid.index(x, y);
}

EntityLocation gridLocation(
  const PlannerGrid& grid,
  int index,
  const EntityLocation& context,
  float z
) {
  EntityLocation location = context;
  location.position = grid.point(index, z);
  return location;
}

std::vector<EntityLocation> simplify(
  const IWorld& world,
  const Character& character,
  const PlannerGrid& grid,
  const std::vector<EntityLocation>& raw
) {
  if (raw.size() < 2) return raw;

  std::vector<EntityLocation> result;
  result.push_back(raw.front());
  std::size_t anchor = 0;

  while (anchor + 1 < raw.size()) {
    std::size_t furthest = anchor + 1;
    for (std::size_t probe = raw.size() - 1; probe > anchor + 1; --probe) {
      if (segmentClear(world, character, grid, raw[anchor], raw[probe])) {
        furthest = probe;
        break;
      }
    }
    result.push_back(raw[furthest]);
    anchor = furthest;
  }
  return result;
}

LocalPathResult findLocalPath(
  const IWorld& world,
  const Character& character,
  const EntityLocation& start,
  const EntityLocation& requestedValue
) {
  LocalPathResult result;
  EntityLocation requested = resolveContext(start, requestedValue);
  if (!sameContext(start, requested)) return result;

  const PlannerGrid grid = makeGrid(world, character, start.levelId);
  if (!inside(grid, start.position)) return result;

  if (segmentClear(world, character, grid, start, requested)) {
    result.waypoints.push_back(requested);
    result.resolvedDestination = requested;
    result.reachedRequestedDestination = true;
    return result;
  }

  const int nodeCount = grid.width * grid.height;
  const int startIndex = nearestIndex(grid, start.position);
  const int targetIndex = nearestIndex(grid, requested.position);
  std::vector<float> cost(nodeCount, std::numeric_limits<float>::infinity());
  std::vector<int> parent(nodeCount, -1);
  std::vector<unsigned char> closed(nodeCount, 0);
  std::priority_queue<QueueNode> open;

  cost[startIndex] = 0.0f;
  open.push({
    startIndex,
    distance2D(grid.point(startIndex, start.position.z), requested.position)
  });

  int best = -1;
  float bestDistance = std::numeric_limits<float>::infinity();
  const int neighbourOffsets[8][2] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1},
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
  };

  while (!open.empty()) {
    const int current = open.top().index;
    open.pop();
    if (closed[current]) continue;
    closed[current] = 1;

    const EntityLocation currentLocation = gridLocation(
      grid,
      current,
      start,
      start.position.z
    );
    const Vec3 currentForward = horizontalDirection(
      start.position,
      currentLocation.position,
      character.forward
    );
    if (!canOccupy(world, character, grid, currentLocation, currentForward)) continue;

    const float destinationDistance = distance2D(currentLocation.position, requested.position);
    if (destinationDistance < bestDistance) {
      bestDistance = destinationDistance;
      best = current;
    }

    if (current == targetIndex) break;

    const int currentX = current % grid.width;
    const int currentY = current / grid.width;

    for (const auto& offset : neighbourOffsets) {
      const int nextX = currentX + offset[0];
      const int nextY = currentY + offset[1];
      if (!grid.valid(nextX, nextY)) continue;

      const int next = grid.index(nextX, nextY);
      if (closed[next]) continue;

      const EntityLocation nextLocation = gridLocation(
        grid,
        next,
        start,
        start.position.z
      );
      const Vec3 direction = horizontalDirection(
        currentLocation.position,
        nextLocation.position,
        character.forward
      );
      if (!canOccupy(world, character, grid, nextLocation, direction)) continue;
      if (!segmentClear(world, character, grid, currentLocation, nextLocation)) continue;

      const float edgeCost = distance2D(currentLocation.position, nextLocation.position);
      const float nextCost = cost[current] + edgeCost;
      if (nextCost + 1e-6f >= cost[next]) continue;

      cost[next] = nextCost;
      parent[next] = current;
      open.push({
        next,
        nextCost + distance2D(nextLocation.position, requested.position)
      });
    }
  }

  if (best < 0 || best == startIndex) return result;

  std::vector<EntityLocation> reversePath;
  for (int node = best; node >= 0 && node != startIndex; node = parent[node]) {
    reversePath.push_back(gridLocation(grid, node, start, start.position.z));
    if (parent[node] < 0) break;
  }
  if (reversePath.empty()) return result;

  std::reverse(reversePath.begin(), reversePath.end());
  std::vector<EntityLocation> rawPath;
  rawPath.push_back(start);
  rawPath.insert(rawPath.end(), reversePath.begin(), reversePath.end());

  std::vector<EntityLocation> smoothPath = simplify(world, character, grid, rawPath);
  if (
    !smoothPath.empty() &&
    distance2D(smoothPath.front().position, start.position) < 1e-5f
  ) {
    smoothPath.erase(smoothPath.begin());
  }
  if (smoothPath.empty()) return result;

  result.waypoints = std::move(smoothPath);
  result.resolvedDestination = result.waypoints.back();
  result.reachedRequestedDestination =
    distance2D(result.resolvedDestination.position, requested.position) <= grid.step * 0.75f;
  return result;
}

bool appendLocalPath(
  NavigationPath& destination,
  const LocalPathResult& local,
  bool requireExactDestination
) {
  if (local.waypoints.empty()) return false;
  if (requireExactDestination && !local.reachedRequestedDestination) return false;

  destination.waypoints.insert(
    destination.waypoints.end(),
    local.waypoints.begin(),
    local.waypoints.end()
  );
  destination.resolvedDestination = local.resolvedDestination;
  return true;
}

std::vector<EntityLocation> resolvedConnectionPoints(
  const NavigationConnection& connection,
  bool reverse,
  const EntityLocation& current
) {
  std::vector<EntityLocation> raw = connection.points;
  if (reverse) std::reverse(raw.begin(), raw.end());

  std::vector<EntityLocation> result;
  result.reserve(raw.size());
  EntityLocation context = current;
  for (EntityLocation point : raw) {
    point = resolveContext(context, point);
    result.push_back(point);
    context = point;
  }
  return result;
}

bool findConnectionSequence(
  const IWorld& world,
  const EntityLocation& start,
  const EntityLocation& requestedValue,
  std::vector<std::pair<int, bool>>& steps
) {
  const EntityLocation requested = resolveContext(start, requestedValue);
  if (sameContext(start, requested)) return true;

  std::vector<ContextState> states;
  std::queue<int> pending;
  std::unordered_set<std::string> visited;

  states.push_back({start, -1, -1, false});
  pending.push(0);
  visited.insert(contextKey(start));

  int destinationState = -1;
  const std::vector<NavigationConnection>& connections = world.navigationConnections();

  while (!pending.empty() && destinationState < 0) {
    const int stateIndex = pending.front();
    pending.pop();
    const EntityLocation current = states[stateIndex].location;

    if (sameContext(current, requested)) {
      destinationState = stateIndex;
      break;
    }

    for (std::size_t connectionIndex = 0; connectionIndex < connections.size(); ++connectionIndex) {
      const NavigationConnection& connection = connections[connectionIndex];
      if (!connection.valid()) continue;

      const EntityLocation first = resolveContext(current, connection.points.front());
      const EntityLocation lastFromFirst = resolveContext(first, connection.points.back());

      const auto tryEdge = [&](const EntityLocation& entry, const EntityLocation& exit, bool reverse) {
        if (!sameContext(current, entry)) return;
        const EntityLocation next = resolveContext(current, exit);
        const std::string key = contextKey(next);
        if (visited.find(key) != visited.end()) return;

        visited.insert(key);
        states.push_back({
          next,
          stateIndex,
          static_cast<int>(connectionIndex),
          reverse
        });
        const int nextIndex = static_cast<int>(states.size() - 1);
        pending.push(nextIndex);
        if (sameContext(next, requested)) destinationState = nextIndex;
      };

      tryEdge(first, lastFromFirst, false);
      if (connection.bidirectional) {
        const EntityLocation last = resolveContext(current, connection.points.back());
        const EntityLocation firstFromLast = resolveContext(last, connection.points.front());
        tryEdge(last, firstFromLast, true);
      }

      if (destinationState >= 0) break;
    }
  }

  if (destinationState < 0) return false;

  std::vector<std::pair<int, bool>> reversed;
  for (int index = destinationState; index >= 0 && states[index].parent >= 0; index = states[index].parent) {
    reversed.push_back({states[index].connection, states[index].reverse});
  }
  std::reverse(reversed.begin(), reversed.end());
  steps = std::move(reversed);
  return true;
}

} // namespace

NavigationPath Pathfinder::findPath(
  const IWorld& world,
  const Character& character,
  const EntityLocation& requestedValue
) const {
  NavigationPath result;
  EntityLocation requested = resolveContext(character.location, requestedValue);

  if (sameContext(character.location, requested)) {
    const LocalPathResult local = findLocalPath(
      world,
      character,
      character.location,
      requested
    );
    if (!appendLocalPath(result, local, false)) return result;
    result.reachedRequestedDestination = local.reachedRequestedDestination;
    return result;
  }

  std::vector<std::pair<int, bool>> connectionSteps;
  if (!findConnectionSequence(world, character.location, requested, connectionSteps)) {
    return result;
  }

  EntityLocation current = character.location;
  const std::vector<NavigationConnection>& connections = world.navigationConnections();

  for (const auto& step : connectionSteps) {
    const NavigationConnection& connection = connections[step.first];
    const std::vector<EntityLocation> points = resolvedConnectionPoints(
      connection,
      step.second,
      current
    );
    if (points.size() < 2) return NavigationPath();

    const LocalPathResult toEntry = findLocalPath(world, character, current, points.front());
    if (!appendLocalPath(result, toEntry, true)) return NavigationPath();

    for (std::size_t index = 1; index < points.size(); ++index) {
      result.waypoints.push_back(points[index]);
      result.resolvedDestination = points[index];
    }
    current = points.back();
  }

  const LocalPathResult finalPath = findLocalPath(world, character, current, requested);
  if (!appendLocalPath(result, finalPath, false)) return NavigationPath();
  result.reachedRequestedDestination = finalPath.reachedRequestedDestination;
  return result;
}

} // namespace engine
} // namespace isoweb
