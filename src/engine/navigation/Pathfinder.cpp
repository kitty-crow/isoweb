#include "engine/navigation/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
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

PlannerGrid makeGrid(const IWorld& world, const Character& character) {
  float minimumX = 1e9f;
  float maximumX = -1e9f;
  float minimumY = 1e9f;
  float maximumY = -1e9f;

  for (const Vec3& point : world.bounds().points) {
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
  const Vec3& position,
  const Vec3& forward
) {
  if (!inside(grid, position)) return false;

  Character candidate = original;
  candidate.location.position = position;
  candidate.forward = forward;
  return !world.collidesWith(candidate);
}

bool segmentClear(
  const IWorld& world,
  const Character& character,
  const PlannerGrid& grid,
  const Vec3& from,
  const Vec3& to
) {
  const float distance = distance2D(from, to);
  if (distance < 1e-6f) {
    return canOccupy(world, character, grid, to, character.forward);
  }

  const Vec3 direction = horizontalDirection(from, to, character.forward);
  const int steps = std::max(
    1,
    static_cast<int>(std::ceil(distance / (grid.step * 0.45f)))
  );

  for (int index = 1; index <= steps; ++index) {
    const float t = static_cast<float>(index) / steps;
    const Vec3 point(
      from.x + (to.x - from.x) * t,
      from.y + (to.y - from.y) * t,
      from.z + (to.z - from.z) * t
    );
    if (!canOccupy(world, character, grid, point, direction)) return false;
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

std::vector<Vec3> simplify(
  const IWorld& world,
  const Character& character,
  const PlannerGrid& grid,
  const std::vector<Vec3>& raw
) {
  if (raw.size() < 2) return raw;

  std::vector<Vec3> result;
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

} // namespace

NavigationPath Pathfinder::findPath(
  const IWorld& world,
  const Character& character,
  const Vec3& requestedDestination
) const {
  NavigationPath result;

  // This first generic planner resolves paths inside one currently active level.
  // Cross-level connections can be layered on top without changing Character.
  if (
    !character.location.levelId.empty() &&
    character.location.levelId != world.activeLevelId()
  ) {
    return result;
  }

  const PlannerGrid grid = makeGrid(world, character);
  const Vec3 start = character.location.position;
  Vec3 requested = requestedDestination;
  requested.z = start.z;

  if (!inside(grid, start)) return result;

  if (segmentClear(world, character, grid, start, requested)) {
    result.waypoints.push_back(requested);
    result.resolvedDestination = requested;
    result.reachedRequestedDestination = true;
    return result;
  }

  const int nodeCount = grid.width * grid.height;
  const int startIndex = nearestIndex(grid, start);
  const int targetIndex = nearestIndex(grid, requested);
  std::vector<float> cost(nodeCount, std::numeric_limits<float>::infinity());
  std::vector<int> parent(nodeCount, -1);
  std::vector<unsigned char> closed(nodeCount, 0);
  std::priority_queue<QueueNode> open;

  cost[startIndex] = 0.0f;
  open.push({startIndex, distance2D(grid.point(startIndex, start.z), requested)});

  int best = startIndex;
  float bestDistance = distance2D(start, requested);
  const int neighbourOffsets[8][2] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1},
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
  };

  while (!open.empty()) {
    const int current = open.top().index;
    open.pop();
    if (closed[current]) continue;
    closed[current] = 1;

    const Vec3 currentPoint = grid.point(current, start.z);
    const float destinationDistance = distance2D(currentPoint, requested);
    if (destinationDistance < bestDistance) {
      bestDistance = destinationDistance;
      best = current;
    }

    if (
      current == targetIndex &&
      canOccupy(
        world,
        character,
        grid,
        currentPoint,
        horizontalDirection(start, currentPoint, character.forward)
      )
    ) {
      best = current;
      break;
    }

    const int currentX = current % grid.width;
    const int currentY = current / grid.width;

    for (const auto& offset : neighbourOffsets) {
      const int nextX = currentX + offset[0];
      const int nextY = currentY + offset[1];
      if (!grid.valid(nextX, nextY)) continue;

      const int next = grid.index(nextX, nextY);
      if (closed[next]) continue;

      const Vec3 nextPoint = grid.point(next, start.z);
      const Vec3 direction = horizontalDirection(currentPoint, nextPoint, character.forward);
      if (!canOccupy(world, character, grid, nextPoint, direction)) continue;
      if (!segmentClear(world, character, grid, currentPoint, nextPoint)) continue;

      const float edgeCost = distance2D(currentPoint, nextPoint);
      const float nextCost = cost[current] + edgeCost;
      if (nextCost + 1e-6f >= cost[next]) continue;

      cost[next] = nextCost;
      parent[next] = current;
      open.push({next, nextCost + distance2D(nextPoint, requested)});
    }
  }

  if (
    best == startIndex &&
    bestDistance >= distance2D(start, requested) - 1e-5f
  ) {
    return result;
  }

  std::vector<Vec3> reversePath;
  for (int node = best; node >= 0 && node != startIndex; node = parent[node]) {
    reversePath.push_back(grid.point(node, start.z));
    if (parent[node] < 0) break;
  }
  if (reversePath.empty()) return result;

  std::reverse(reversePath.begin(), reversePath.end());
  std::vector<Vec3> rawPath;
  rawPath.push_back(start);
  rawPath.insert(rawPath.end(), reversePath.begin(), reversePath.end());

  std::vector<Vec3> smoothPath = simplify(world, character, grid, rawPath);
  if (
    !smoothPath.empty() &&
    distance2D(smoothPath.front(), start) < 1e-5f
  ) {
    smoothPath.erase(smoothPath.begin());
  }
  if (smoothPath.empty()) return result;

  result.waypoints = std::move(smoothPath);
  result.resolvedDestination = result.waypoints.back();
  result.reachedRequestedDestination =
    distance2D(result.resolvedDestination, requested) <= grid.step * 0.75f;
  return result;
}

} // namespace engine
} // namespace isoweb
