from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"expected source block not found in {path}: {old[:140]!r}")
    p.write_text(text.replace(old, new, 1))

p = 'src/engine/world/RuntimeWorld.cpp'

replace_once(
    p,
    '''struct Hit {\n  bool found = false;\n  float t = FAR_DISTANCE;\n  Vec3 point;\n  Vec3 normal;\n  Vec3 colour;\n  SceneSurfaceKind kind = SceneSurfaceKind::Object;\n  bool walkable = false;\n};\n''',
    '''struct Hit {\n  bool found = false;\n  float t = FAR_DISTANCE;\n  Vec3 point;\n  Vec3 normal;\n  Vec3 colour;\n  SceneSurfaceKind kind = SceneSurfaceKind::Object;\n  bool walkable = false;\n};\n\nenum class StaticAccelKind : std::uint8_t {\n  Object,\n  RoomWall,\n  Staircase\n};\n\nstruct StaticAccelItem {\n  StaticAccelKind kind = StaticAccelKind::Object;\n  std::size_t index = 0;\n  std::size_t authoredOrder = 0;\n  Vec3 minimum;\n  Vec3 maximum;\n};\n\nstruct StaticBvhNode {\n  Vec3 minimum;\n  Vec3 maximum;\n  int left = -1;\n  int right = -1;\n  std::size_t start = 0;\n  std::size_t count = 0;\n};\n'''
)

# cstdint is required for the compact enum underlying type.
replace_once(
    p,
    '''#include <cmath>\n#include <memory>''',
    '''#include <cmath>\n#include <cstdint>\n#include <memory>'''
)

replace_once(
    p,
    '''    buildCollisionObjects();\n    buildStairSteps();\n    buildBounds();''',
    '''    buildCollisionObjects();\n    buildStairSteps();\n    buildStaticAcceleration();\n    buildBounds();'''
)

# Insert BVH construction/query helpers before bounds construction. The BVH is
# a candidate accelerator only; exact existing intersection routines remain the
# authority for all visible and shadow hits.
replace_once(
    p,
    '''  void buildBounds() {''',
    r'''  static float axisValue(const Vec3& value, int axis) {
    return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
  }

  static Vec3 minimumVec(const Vec3& a, const Vec3& b) {
    return {
      std::min(a.x, b.x),
      std::min(a.y, b.y),
      std::min(a.z, b.z)
    };
  }

  static Vec3 maximumVec(const Vec3& a, const Vec3& b) {
    return {
      std::max(a.x, b.x),
      std::max(a.y, b.y),
      std::max(a.z, b.z)
    };
  }

  int buildStaticBvhNode(std::size_t start, std::size_t end) {
    StaticBvhNode node;
    node.start = start;
    node.count = end - start;
    node.minimum = {FAR_DISTANCE, FAR_DISTANCE, FAR_DISTANCE};
    node.maximum = {-FAR_DISTANCE, -FAR_DISTANCE, -FAR_DISTANCE};
    Vec3 centroidMinimum = node.minimum;
    Vec3 centroidMaximum = node.maximum;

    for (std::size_t offset = start; offset < end; ++offset) {
      const StaticAccelItem& item = staticAccelItems_[staticBvhOrder_[offset]];
      node.minimum = minimumVec(node.minimum, item.minimum);
      node.maximum = maximumVec(node.maximum, item.maximum);
      const Vec3 centroid = (item.minimum + item.maximum) * 0.5f;
      centroidMinimum = minimumVec(centroidMinimum, centroid);
      centroidMaximum = maximumVec(centroidMaximum, centroid);
    }

    const int nodeIndex = static_cast<int>(staticBvhNodes_.size());
    staticBvhNodes_.push_back(node);
    if (end - start <= 4) return nodeIndex;

    const Vec3 span = centroidMaximum - centroidMinimum;
    int axis = 0;
    if (span.y > span.x) axis = 1;
    if (axisValue(span, 2) > axisValue(span, axis)) axis = 2;
    if (axisValue(span, axis) <= 1.0e-6f) return nodeIndex;

    const std::size_t middle = start + (end - start) / 2;
    std::nth_element(
      staticBvhOrder_.begin() + static_cast<std::ptrdiff_t>(start),
      staticBvhOrder_.begin() + static_cast<std::ptrdiff_t>(middle),
      staticBvhOrder_.begin() + static_cast<std::ptrdiff_t>(end),
      [&](std::size_t leftIndex, std::size_t rightIndex) {
        const StaticAccelItem& left = staticAccelItems_[leftIndex];
        const StaticAccelItem& right = staticAccelItems_[rightIndex];
        const float leftCentre = axisValue((left.minimum + left.maximum) * 0.5f, axis);
        const float rightCentre = axisValue((right.minimum + right.maximum) * 0.5f, axis);
        if (leftCentre != rightCentre) return leftCentre < rightCentre;
        return left.authoredOrder < right.authoredOrder;
      }
    );

    const int left = buildStaticBvhNode(start, middle);
    const int right = buildStaticBvhNode(middle, end);
    staticBvhNodes_[static_cast<std::size_t>(nodeIndex)].left = left;
    staticBvhNodes_[static_cast<std::size_t>(nodeIndex)].right = right;
    staticBvhNodes_[static_cast<std::size_t>(nodeIndex)].count = 0;
    return nodeIndex;
  }

  void buildStaticAcceleration() {
    staticAccelItems_.clear();
    staticBvhOrder_.clear();
    staticBvhNodes_.clear();
    staticBvhRoot_ = -1;

    const std::size_t itemCount =
      definition_.objects.size() + roomWalls_.size() + stairSteps_.size();
    staticAccelItems_.reserve(itemCount);
    std::size_t authoredOrder = 0;

    for (std::size_t index = 0; index < definition_.objects.size(); ++index) {
      const RuntimePrimitive& object = definition_.objects[index];
      const Vec3 extent = objectExtent(object);
      StaticAccelItem item;
      item.kind = StaticAccelKind::Object;
      item.index = index;
      item.authoredOrder = authoredOrder++;
      item.minimum = object.position - extent;
      item.maximum = object.position + extent;
      staticAccelItems_.push_back(item);
    }

    for (std::size_t index = 0; index < roomWalls_.size(); ++index) {
      const RoomWallBox& wall = roomWalls_[index];
      StaticAccelItem item;
      item.kind = StaticAccelKind::RoomWall;
      item.index = index;
      item.authoredOrder = authoredOrder++;
      item.minimum = wall.centre - wall.halfExtent;
      item.maximum = wall.centre + wall.halfExtent;
      staticAccelItems_.push_back(item);
    }

    for (std::size_t index = 0; index < stairSteps_.size(); ++index) {
      StaticAccelItem item;
      item.kind = StaticAccelKind::Staircase;
      item.index = index;
      item.authoredOrder = authoredOrder++;
      item.minimum = {FAR_DISTANCE, FAR_DISTANCE, FAR_DISTANCE};
      item.maximum = {-FAR_DISTANCE, -FAR_DISTANCE, -FAR_DISTANCE};
      for (const StairStep& step : stairSteps_[index]) {
        item.minimum = minimumVec(item.minimum, step.centre - step.halfExtent);
        item.maximum = maximumVec(item.maximum, step.centre + step.halfExtent);
      }
      staticAccelItems_.push_back(item);
    }

    // Tiny scenes are cheaper to traverse directly. Larger scenes get a
    // median-split BVH. Item bounds are padded slightly so boundary rounding can
    // only admit extra exact tests, never suppress a hit.
    if (staticAccelItems_.size() < 8) return;
    for (StaticAccelItem& item : staticAccelItems_) {
      const Vec3 padding(1.0e-4f, 1.0e-4f, 1.0e-4f);
      item.minimum = item.minimum - padding;
      item.maximum = item.maximum + padding;
    }
    staticBvhOrder_.resize(staticAccelItems_.size());
    for (std::size_t index = 0; index < staticBvhOrder_.size(); ++index) {
      staticBvhOrder_[index] = index;
    }
    staticBvhNodes_.reserve(staticAccelItems_.size() * 2);
    staticBvhRoot_ = buildStaticBvhNode(0, staticBvhOrder_.size());
  }

  bool rayBoundsEntry(
    const Ray& ray,
    const Vec3& minimumBounds,
    const Vec3& maximumBounds,
    float minimumDistance,
    float maximumDistance,
    float& entryDistance
  ) const {
    float nearDistance = minimumDistance;
    float farDistance = maximumDistance;
    const float origins[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float directions[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float minimums[3] = {minimumBounds.x, minimumBounds.y, minimumBounds.z};
    const float maximums[3] = {maximumBounds.x, maximumBounds.y, maximumBounds.z};

    for (int axis = 0; axis < 3; ++axis) {
      if (std::fabs(directions[axis]) < 1.0e-8f) {
        if (origins[axis] < minimums[axis] || origins[axis] > maximums[axis]) return false;
        continue;
      }
      const float inverse = 1.0f / directions[axis];
      float t0 = (minimums[axis] - origins[axis]) * inverse;
      float t1 = (maximums[axis] - origins[axis]) * inverse;
      if (t0 > t1) std::swap(t0, t1);
      nearDistance = std::max(nearDistance, t0);
      farDistance = std::min(farDistance, t1);
      if (farDistance < nearDistance) return false;
    }
    entryDistance = nearDistance;
    return true;
  }

  bool intersectAccelItem(
    const StaticAccelItem& item,
    const Ray& ray,
    float minimum,
    float maximum,
    bool visualWalls,
    Hit& hit
  ) const {
    switch (item.kind) {
      case StaticAccelKind::Object:
        return intersectObject(ray, definition_.objects[item.index], minimum, maximum, hit);
      case StaticAccelKind::RoomWall: {
        const RoomWallBox& wall = roomWalls_[item.index];
        const bool found = visualWalls
          ? intersectRoomWallVisual(ray, wall, minimum, maximum, hit)
          : intersectAxisAlignedBox(ray, wall.centre, wall.halfExtent, minimum, maximum, hit);
        if (found && visualWalls) {
          hit.colour = wall.colour;
          hit.kind = SceneSurfaceKind::Object;
          hit.walkable = false;
        }
        return found;
      }
      case StaticAccelKind::Staircase:
        return intersectRuntimeStaircase(ray, item.index, minimum, maximum, hit);
    }
    return false;
  }

  void traceStaticBvhClosest(
    int nodeIndex,
    const Ray& ray,
    float minimum,
    float& maximum,
    Hit& result,
    std::size_t& bestAuthoredOrder
  ) const {
    if (nodeIndex < 0) return;
    const StaticBvhNode& node = staticBvhNodes_[static_cast<std::size_t>(nodeIndex)];
    float nodeEntry = 0.0f;
    if (!rayBoundsEntry(ray, node.minimum, node.maximum, minimum, maximum, nodeEntry)) return;

    if (node.left < 0 && node.right < 0) {
      for (std::size_t offset = node.start; offset < node.start + node.count; ++offset) {
        const StaticAccelItem& item = staticAccelItems_[staticBvhOrder_[offset]];
        float itemEntry = 0.0f;
        if (!rayBoundsEntry(ray, item.minimum, item.maximum, minimum, maximum, itemEntry)) continue;
        Hit hit;
        if (!intersectAccelItem(item, ray, minimum, maximum, true, hit)) continue;
        if (
          !result.found ||
          hit.t < maximum ||
          (hit.t == maximum && item.authoredOrder > bestAuthoredOrder)
        ) {
          result = hit;
          maximum = hit.t;
          bestAuthoredOrder = item.authoredOrder;
        }
      }
      return;
    }

    float leftEntry = FAR_DISTANCE;
    float rightEntry = FAR_DISTANCE;
    const bool hitLeft = node.left >= 0 && rayBoundsEntry(
      ray,
      staticBvhNodes_[static_cast<std::size_t>(node.left)].minimum,
      staticBvhNodes_[static_cast<std::size_t>(node.left)].maximum,
      minimum,
      maximum,
      leftEntry
    );
    const bool hitRight = node.right >= 0 && rayBoundsEntry(
      ray,
      staticBvhNodes_[static_cast<std::size_t>(node.right)].minimum,
      staticBvhNodes_[static_cast<std::size_t>(node.right)].maximum,
      minimum,
      maximum,
      rightEntry
    );

    if (hitLeft && hitRight) {
      if (leftEntry <= rightEntry) {
        traceStaticBvhClosest(node.left, ray, minimum, maximum, result, bestAuthoredOrder);
        traceStaticBvhClosest(node.right, ray, minimum, maximum, result, bestAuthoredOrder);
      } else {
        traceStaticBvhClosest(node.right, ray, minimum, maximum, result, bestAuthoredOrder);
        traceStaticBvhClosest(node.left, ray, minimum, maximum, result, bestAuthoredOrder);
      }
    } else if (hitLeft) {
      traceStaticBvhClosest(node.left, ray, minimum, maximum, result, bestAuthoredOrder);
    } else if (hitRight) {
      traceStaticBvhClosest(node.right, ray, minimum, maximum, result, bestAuthoredOrder);
    }
  }

  bool traceStaticBvhAny(
    int nodeIndex,
    const Ray& ray,
    float minimum,
    float maximum
  ) const {
    if (nodeIndex < 0) return false;
    const StaticBvhNode& node = staticBvhNodes_[static_cast<std::size_t>(nodeIndex)];
    float nodeEntry = 0.0f;
    if (!rayBoundsEntry(ray, node.minimum, node.maximum, minimum, maximum, nodeEntry)) return false;

    if (node.left < 0 && node.right < 0) {
      for (std::size_t offset = node.start; offset < node.start + node.count; ++offset) {
        const StaticAccelItem& item = staticAccelItems_[staticBvhOrder_[offset]];
        float itemEntry = 0.0f;
        if (!rayBoundsEntry(ray, item.minimum, item.maximum, minimum, maximum, itemEntry)) continue;
        Hit hit;
        if (intersectAccelItem(item, ray, minimum, maximum, false, hit)) return true;
      }
      return false;
    }

    float leftEntry = FAR_DISTANCE;
    float rightEntry = FAR_DISTANCE;
    const bool hitLeft = node.left >= 0 && rayBoundsEntry(
      ray,
      staticBvhNodes_[static_cast<std::size_t>(node.left)].minimum,
      staticBvhNodes_[static_cast<std::size_t>(node.left)].maximum,
      minimum,
      maximum,
      leftEntry
    );
    const bool hitRight = node.right >= 0 && rayBoundsEntry(
      ray,
      staticBvhNodes_[static_cast<std::size_t>(node.right)].minimum,
      staticBvhNodes_[static_cast<std::size_t>(node.right)].maximum,
      minimum,
      maximum,
      rightEntry
    );
    if (hitLeft && hitRight) {
      if (leftEntry <= rightEntry) {
        if (traceStaticBvhAny(node.left, ray, minimum, maximum)) return true;
        return traceStaticBvhAny(node.right, ray, minimum, maximum);
      }
      if (traceStaticBvhAny(node.right, ray, minimum, maximum)) return true;
      return traceStaticBvhAny(node.left, ray, minimum, maximum);
    }
    if (hitLeft) return traceStaticBvhAny(node.left, ray, minimum, maximum);
    if (hitRight) return traceStaticBvhAny(node.right, ray, minimum, maximum);
    return false;
  }

  void buildBounds() {'''
)

# Replace brute-force first three categories with BVH, retaining a byte-for-byte
# fallback on tiny scenes and preserving the original authored tie order.
replace_once(
    p,
    r'''  Hit traceClosest(const Ray& ray, float minimum, float maximum) const {
    Hit result;
    for (const RuntimePrimitive& object : definition_.objects) {
      Hit hit;
      if (intersectObject(ray, object, minimum, maximum, hit)) {
        result = hit;
        maximum = hit.t;
      }
    }

    for (const RoomWallBox& wall : roomWalls_) {
      Hit hit;
      if (intersectRoomWallVisual(ray, wall, minimum, maximum, hit)) {
        hit.colour = wall.colour;
        hit.kind = SceneSurfaceKind::Object;
        hit.walkable = false;
        result = hit;
        maximum = hit.t;
      }
    }

    for (std::size_t index = 0; index < stairSteps_.size(); ++index) {
      Hit hit;
      if (intersectRuntimeStaircase(ray, index, minimum, maximum, hit)) {
        result = hit;
        maximum = hit.t;
      }
    }
''',
    r'''  Hit traceClosest(const Ray& ray, float minimum, float maximum) const {
    Hit result;
    if (staticBvhRoot_ >= 0) {
      std::size_t bestAuthoredOrder = 0;
      traceStaticBvhClosest(
        staticBvhRoot_,
        ray,
        minimum,
        maximum,
        result,
        bestAuthoredOrder
      );
    } else {
      for (const RuntimePrimitive& object : definition_.objects) {
        Hit hit;
        if (intersectObject(ray, object, minimum, maximum, hit)) {
          result = hit;
          maximum = hit.t;
        }
      }

      for (const RoomWallBox& wall : roomWalls_) {
        Hit hit;
        if (intersectRoomWallVisual(ray, wall, minimum, maximum, hit)) {
          hit.colour = wall.colour;
          hit.kind = SceneSurfaceKind::Object;
          hit.walkable = false;
          result = hit;
          maximum = hit.t;
        }
      }

      for (std::size_t index = 0; index < stairSteps_.size(); ++index) {
        Hit hit;
        if (intersectRuntimeStaircase(ray, index, minimum, maximum, hit)) {
          result = hit;
          maximum = hit.t;
        }
      }
    }
'''
)

replace_once(
    p,
    r'''  bool traceAny(const Ray& ray, float minimum, float maximum) const {
    Hit hit;
    for (const RuntimePrimitive& object : definition_.objects) {
      if (intersectObject(ray, object, minimum, maximum, hit)) return true;
    }
    for (const RoomWallBox& wall : roomWalls_) {
      if (intersectAxisAlignedBox(ray, wall.centre, wall.halfExtent, minimum, maximum, hit)) return true;
    }
    for (std::size_t index = 0; index < stairSteps_.size(); ++index) {
      if (intersectRuntimeStaircase(ray, index, minimum, maximum, hit)) return true;
    }
''',
    r'''  bool traceAny(const Ray& ray, float minimum, float maximum) const {
    Hit hit;
    if (staticBvhRoot_ >= 0) {
      if (traceStaticBvhAny(staticBvhRoot_, ray, minimum, maximum)) return true;
    } else {
      for (const RuntimePrimitive& object : definition_.objects) {
        if (intersectObject(ray, object, minimum, maximum, hit)) return true;
      }
      for (const RoomWallBox& wall : roomWalls_) {
        if (intersectAxisAlignedBox(ray, wall.centre, wall.halfExtent, minimum, maximum, hit)) return true;
      }
      for (std::size_t index = 0; index < stairSteps_.size(); ++index) {
        if (intersectRuntimeStaircase(ray, index, minimum, maximum, hit)) return true;
      }
    }
'''
)

replace_once(
    p,
    '''  std::vector<WorldObject> worldObjects_;\n  std::vector<RoomWallBox> roomWalls_;\n  std::vector<std::array<StairStep, STAIR_STEP_COUNT>> stairSteps_;''',
    '''  std::vector<WorldObject> worldObjects_;\n  std::vector<RoomWallBox> roomWalls_;\n  std::vector<std::array<StairStep, STAIR_STEP_COUNT>> stairSteps_;\n  std::vector<StaticAccelItem> staticAccelItems_;\n  std::vector<std::size_t> staticBvhOrder_;\n  std::vector<StaticBvhNode> staticBvhNodes_;\n  int staticBvhRoot_ = -1;'''
)

print('Applied exact static BVH candidate acceleration.')
