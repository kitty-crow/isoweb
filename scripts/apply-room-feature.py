from pathlib import Path

ROOT = Path('.')

def read(path):
    return (ROOT / path).read_text()

def write(path, text):
    p = ROOT / path
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)

def replace_once(path, old, new):
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected exactly one match, found {count}: {old[:120]!r}')
    write(path, text.replace(old, new, 1))

def insert_after(path, marker, addition):
    text = read(path)
    count = text.count(marker)
    if count != 1:
        raise RuntimeError(f'{path}: expected exactly one marker, found {count}: {marker[:120]!r}')
    write(path, text.replace(marker, marker + addition, 1))

# Generic engine room graph. Layout names and demo shapes deliberately do not
# live here: rooms are positioned/sized explicitly and connections are a graph.
write('src/engine/world/Room.hpp', r'''#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "engine/math/Vec3.hpp"

namespace isoweb {
namespace engine {

enum class RoomSide {
  North,
  South,
  East,
  West
};

struct Room {
  std::string id;
  Vec3 centre;
  float width = 1.0f;
  float depth = 1.0f;
  float floorZ = 0.0f;
  float wallHeight = 1.0f;
  float wallThickness = 0.10f;

  bool containsXY(float x, float y, float tolerance = 0.0f) const {
    return
      x >= centre.x - width * 0.5f - tolerance &&
      x <= centre.x + width * 0.5f + tolerance &&
      y >= centre.y - depth * 0.5f - tolerance &&
      y <= centre.y + depth * 0.5f + tolerance;
  }
};

struct RoomPortal {
  std::string roomId;
  RoomSide side = RoomSide::North;
  float offset = 0.0f;
  float width = 1.0f;
};

struct RoomConnection {
  std::string id;
  RoomPortal a;
  RoomPortal b;

  // Structural connectivity is separate from future door state. For now the
  // demo connections are plain openings. A door system can later own whether
  // a connection is currently traversable without changing the room graph.
  bool openPassage = true;
};

struct RoomLayout {
  std::vector<Room> rooms;
  std::vector<RoomConnection> connections;

  const Room* room(const std::string& id) const {
    for (const Room& candidate : rooms) {
      if (candidate.id == id) return &candidate;
    }
    return nullptr;
  }

  bool connected(const std::string& first, const std::string& second) const {
    for (const RoomConnection& connection : connections) {
      if (!connection.openPassage) continue;
      if (
        (connection.a.roomId == first && connection.b.roomId == second) ||
        (connection.a.roomId == second && connection.b.roomId == first)
      ) {
        return true;
      }
    }
    return false;
  }
};

} // namespace engine
} // namespace isoweb
''')

# World API: optional room layout metadata plus a configurable lower-level
# preview stack. Default preview depth remains zero for existing worlds/tests.
replace_once(
    'src/engine/world/World.hpp',
    '#include "engine/world/LiminalObject.hpp"\n',
    '#include "engine/world/LiminalObject.hpp"\n#include "engine/world/Room.hpp"\n'
)
replace_once(
    'src/engine/world/World.hpp',
    '  virtual bool walkableSurfaceAt(float x, float y, SceneSurfaceHit& hit) const = 0;\n  virtual const std::vector<Object>& objects() const = 0;\n',
    '  virtual bool walkableSurfaceAt(float x, float y, SceneSurfaceHit& hit) const = 0;\n  virtual const std::vector<Object>& objects() const = 0;\n  virtual const RoomLayout* roomLayout() const { return nullptr; }\n'
)
old_sample = '''  Vec3 sampleEnvironment(\n    const Ray& ray,\n    float backgroundY,\n    float& environmentDistance\n  ) const override {\n    SceneSurfaceHit hit;\n    const Vec3 colour = activeLevel().sampleWithHit(ray, backgroundY, hit);\n    environmentDistance = hit.found\n      ? hit.distance\n      : std::numeric_limits<float>::max();\n    return colour;\n  }\n'''
new_sample = '''  Vec3 sampleEnvironment(\n    const Ray& ray,\n    float backgroundY,\n    float& environmentDistance\n  ) const override;\n'''
replace_once('src/engine/world/World.hpp', old_sample, new_sample)
insert_after(
    'src/engine/world/World.hpp',
    '  const WorldBounds& bounds(const std::string& levelId) const;\n',
    '''  const RoomLayout* roomLayout(const std::string& levelId) const;\n\n  // Preview depth is configuration, not a fixed engine limit. A depth of two\n  // means active + first lower + second lower may participate in the static\n  // environment render, with upper layers taking visual precedence.\n  void setLowerLevelPreviewDepth(std::size_t depth);\n  std::size_t lowerLevelPreviewDepth() const { return lowerLevelPreviewDepth_; }\n  bool setLevelViewOrigin(const std::string& levelId, const Vec3& origin);\n  Vec3 levelViewOrigin(const std::string& levelId) const;\n\n'''
)
insert_after(
    'src/engine/world/World.hpp',
    '  bool pickWalkableSurface(const Ray& ray, SceneSurfaceHit& hit) const;\n',
    '''  bool pickWalkableDestination(\n    const Ray& ray,\n    EntityLocation& destination,\n    SceneSurfaceHit* hit = nullptr\n  ) const;\n'''
)
insert_after(
    'src/engine/world/World.hpp',
    '  const IWorldLevel& levelFor(const std::string& levelId) const;\n',
    '''  Vec3 sampleVisibleEnvironment(\n    const Ray& ray,\n    float backgroundY,\n    SceneSurfaceHit& hit,\n    std::size_t* sourceLevelIndex = nullptr,\n    Vec3* sourceLocalPoint = nullptr\n  ) const;\n  bool traceVisibleEnvironment(\n    const Ray& ray,\n    SceneSurfaceHit& hit,\n    std::size_t* sourceLevelIndex = nullptr,\n    Vec3* sourceLocalPoint = nullptr\n  ) const;\n  Vec3 levelOffsetInActiveView(std::size_t levelIndex) const;\n  void updateLevelResidency();\n  void updateVisibleBounds();\n'''
)
insert_after(
    'src/engine/world/World.hpp',
    '  std::vector<LevelLight> levelLights_;\n',
    '''  std::vector<Vec3> levelViewOrigins_;\n  std::size_t lowerLevelPreviewDepth_ = 0;\n  WorldBounds visibleBounds_;\n'''
)

# World implementation: layer-aware static sampling. It does not render each
# lower level independently. Per ray it stops at the first level that actually
# has geometry, and Renderer caches the resulting static supersample.
replace_once(
    'src/engine/world/World.cpp',
    '  levelXYBounds_.resize(levels_.size());\n',
    '  levelXYBounds_.resize(levels_.size());\n  levelViewOrigins_.resize(levels_.size());\n'
)
replace_once(
    'src/engine/world/World.cpp',
    '  levelLights_.resize(levels_.size());\n}\n',
    '  levelLights_.resize(levels_.size());\n  updateLevelResidency();\n  updateVisibleBounds();\n}\n'
)
replace_once(
    'src/engine/world/World.cpp',
    'const WorldBounds& World::bounds() const {\n  return activeLevel().bounds();\n}\n',
    'const WorldBounds& World::bounds() const {\n  return visibleBounds_;\n}\n'
)
insert_after(
    'src/engine/world/World.cpp',
    '''const WorldBounds& World::bounds(const std::string& levelId) const {\n  return levelFor(levelId).bounds();\n}\n''',
    r'''

const RoomLayout* World::roomLayout(const std::string& levelId) const {
  const std::size_t index = levelIndex(levelId);
  return index < levels_.size() ? levels_[index]->roomLayout() : nullptr;
}

Vec3 World::levelOffsetInActiveView(std::size_t index) const {
  if (index >= levelViewOrigins_.size() || activeLevelIndex_ >= levelViewOrigins_.size()) {
    return Vec3();
  }
  return levelViewOrigins_[index] - levelViewOrigins_[activeLevelIndex_];
}

void World::setLowerLevelPreviewDepth(std::size_t depth) {
  const std::size_t maximum = levels_.empty() ? 0 : levels_.size() - 1;
  lowerLevelPreviewDepth_ = std::min(depth, maximum);
  updateLevelResidency();
  updateVisibleBounds();
  runtimeRenderCachePrepared_ = false;
}

bool World::setLevelViewOrigin(const std::string& levelId, const Vec3& origin) {
  const std::size_t index = levelIndex(levelId);
  if (index >= levelViewOrigins_.size()) return false;
  levelViewOrigins_[index] = origin;
  updateVisibleBounds();
  runtimeRenderCachePrepared_ = false;
  return true;
}

Vec3 World::levelViewOrigin(const std::string& levelId) const {
  const std::size_t index = levelIndex(levelId);
  return index < levelViewOrigins_.size() ? levelViewOrigins_[index] : Vec3();
}

Vec3 World::sampleVisibleEnvironment(
  const Ray& ray,
  float backgroundY,
  SceneSurfaceHit& hit,
  std::size_t* sourceLevelIndex,
  Vec3* sourceLocalPoint
) const {
  hit = SceneSurfaceHit();
  if (sourceLevelIndex) *sourceLevelIndex = activeLevelIndex_;
  if (sourceLocalPoint) *sourceLocalPoint = Vec3();

  SceneSurfaceHit activeHit;
  const Vec3 activeColour = activeLevel().sampleWithHit(ray, backgroundY, activeHit);
  if (activeHit.found) {
    hit = activeHit;
    if (sourceLocalPoint) *sourceLocalPoint = activeHit.point;
    return activeColour;
  }

  // Upper layers intentionally win before lower layers are consulted. This is
  // both the desired stacked-room compositing rule and an important early-out:
  // pixels covered by the active level never trace either preview level.
  for (
    std::size_t depth = 1;
    depth <= lowerLevelPreviewDepth_ && depth <= activeLevelIndex_;
    ++depth
  ) {
    const std::size_t index = activeLevelIndex_ - depth;
    const Vec3 offset = levelOffsetInActiveView(index);
    const Ray localRay{ray.origin - offset, ray.direction};
    SceneSurfaceHit localHit;
    const Vec3 colour = levels_[index]->sampleWithHit(localRay, backgroundY, localHit);
    if (!localHit.found) continue;

    if (sourceLevelIndex) *sourceLevelIndex = index;
    if (sourceLocalPoint) *sourceLocalPoint = localHit.point;
    localHit.point = localHit.point + offset;
    hit = localHit;
    return colour;
  }

  return activeColour;
}

bool World::traceVisibleEnvironment(
  const Ray& ray,
  SceneSurfaceHit& hit,
  std::size_t* sourceLevelIndex,
  Vec3* sourceLocalPoint
) const {
  hit = SceneSurfaceHit();
  if (sourceLevelIndex) *sourceLevelIndex = activeLevelIndex_;
  if (sourceLocalPoint) *sourceLocalPoint = Vec3();

  SceneSurfaceHit activeHit;
  if (activeLevel().traceEnvironment(ray, activeHit)) {
    hit = activeHit;
    if (sourceLocalPoint) *sourceLocalPoint = activeHit.point;
    return true;
  }

  for (
    std::size_t depth = 1;
    depth <= lowerLevelPreviewDepth_ && depth <= activeLevelIndex_;
    ++depth
  ) {
    const std::size_t index = activeLevelIndex_ - depth;
    const Vec3 offset = levelOffsetInActiveView(index);
    const Ray localRay{ray.origin - offset, ray.direction};
    SceneSurfaceHit localHit;
    if (!levels_[index]->traceEnvironment(localRay, localHit)) continue;

    if (sourceLevelIndex) *sourceLevelIndex = index;
    if (sourceLocalPoint) *sourceLocalPoint = localHit.point;
    localHit.point = localHit.point + offset;
    hit = localHit;
    return true;
  }
  return false;
}

Vec3 World::sampleEnvironment(
  const Ray& ray,
  float backgroundY,
  float& environmentDistance
) const {
  SceneSurfaceHit hit;
  const Vec3 colour = sampleVisibleEnvironment(ray, backgroundY, hit);
  environmentDistance = hit.found
    ? hit.distance
    : std::numeric_limits<float>::max();
  return colour;
}
'''
)
replace_once(
    'src/engine/world/World.cpp',
    '''bool World::traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const {\n  return activeLevel().traceEnvironment(ray, hit);\n}\n''',
    '''bool World::traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const {\n  return traceVisibleEnvironment(ray, hit);\n}\n'''
)
replace_once(
    'src/engine/world/World.cpp',
    '''Vec3 World::sample(const Ray& ray, float backgroundY) const {\n  SceneSurfaceHit environmentHit;\n  const Vec3 environmentColour = activeLevel().sampleWithHit(ray, backgroundY, environmentHit);\n  const float environmentHitDistance = environmentHit.found\n    ? environmentHit.distance\n    : std::numeric_limits<float>::max();\n''',
    '''Vec3 World::sample(const Ray& ray, float backgroundY) const {\n  SceneSurfaceHit environmentHit;\n  const Vec3 environmentColour = sampleVisibleEnvironment(ray, backgroundY, environmentHit);\n  const float environmentHitDistance = environmentHit.found\n    ? environmentHit.distance\n    : std::numeric_limits<float>::max();\n'''
)
replace_once(
    'src/engine/world/World.cpp',
    '''bool World::pickWalkableSurface(const Ray& ray, SceneSurfaceHit& hit) const {\n  if (!traceEnvironment(ray, hit)) return false;\n  return hit.walkable;\n}\n''',
    r'''bool World::pickWalkableSurface(const Ray& ray, SceneSurfaceHit& hit) const {
  // Compatibility API remains active-level only. Callers that want an exposed
  // lower preview as a destination use pickWalkableDestination().
  if (!activeLevel().traceEnvironment(ray, hit)) return false;
  return hit.walkable;
}

bool World::pickWalkableDestination(
  const Ray& ray,
  EntityLocation& destination,
  SceneSurfaceHit* hit
) const {
  SceneSurfaceHit visible;
  std::size_t sourceIndex = activeLevelIndex_;
  Vec3 sourcePoint;
  if (!traceVisibleEnvironment(ray, visible, &sourceIndex, &sourcePoint) || !visible.walkable) {
    return false;
  }

  destination.levelId = levelIds_[sourceIndex];
  destination.position = sourcePoint;
  if (hit) {
    *hit = visible;
    hit->point = sourcePoint;
  }
  return true;
}
'''
)
insert_after(
    'src/engine/world/World.cpp',
    '''std::size_t World::residentLevelCount() const {\n  std::size_t count = 0;\n  for (const std::unique_ptr<IWorldLevel>& level : levels_) {\n    if (level && level->isResident()) ++count;\n  }\n  return count;\n}\n''',
    r'''

void World::updateLevelResidency() {
  for (std::size_t index = 0; index < levels_.size(); ++index) {
    const bool previewResident =
      index <= activeLevelIndex_ &&
      activeLevelIndex_ - index <= lowerLevelPreviewDepth_;
    levels_[index]->setResident(previewResident);
  }
}

void World::updateVisibleBounds() {
  visibleBounds_ = activeLevel().bounds();
  visibleBounds_.focus = activeLevel().bounds().focus;
  for (
    std::size_t depth = 1;
    depth <= lowerLevelPreviewDepth_ && depth <= activeLevelIndex_;
    ++depth
  ) {
    const std::size_t index = activeLevelIndex_ - depth;
    const Vec3 offset = levelOffsetInActiveView(index);
    const WorldBounds& lower = levels_[index]->bounds();
    visibleBounds_.points.reserve(visibleBounds_.points.size() + lower.points.size());
    for (const Vec3& point : lower.points) visibleBounds_.points.push_back(point + offset);
  }
}
'''
)
replace_once(
    'src/engine/world/World.cpp',
    '''bool World::setActiveLevel(std::size_t index) {\n  if (index >= levels_.size() || index == activeLevelIndex_) return false;\n  levels_[activeLevelIndex_]->setResident(false);\n  activeLevelIndex_ = index;\n  levels_[activeLevelIndex_]->setResident(true);\n  runtimeRenderCachePrepared_ = false;\n  return true;\n}\n''',
    '''bool World::setActiveLevel(std::size_t index) {\n  if (index >= levels_.size() || index == activeLevelIndex_) return false;\n  activeLevelIndex_ = index;\n  updateLevelResidency();\n  updateVisibleBounds();\n  runtimeRenderCachePrepared_ = false;\n  return true;\n}\n'''
)

# Demo room geometry and graph configuration.
insert_after(
    'src/demo/DemoWorld.cpp',
    'using engine::Ray;\n',
    'using engine::Room;\nusing engine::RoomConnection;\nusing engine::RoomLayout;\nusing engine::RoomPortal;\nusing engine::RoomSide;\n'
)
insert_after(
    'src/demo/DemoWorld.cpp',
    'constexpr float GROUND_LIMIT = 4.40f;\n',
    '''constexpr float ROOM_SIZE = GROUND_LIMIT * 2.0f;\nconstexpr float ROOM_WALL_HEIGHT = 1.10f;\nconstexpr float ROOM_WALL_THICKNESS = 0.12f;\nconstexpr float ROOM_OPENING_WIDTH = 1.45f;\n'''
)
insert_after(
    'src/demo/DemoWorld.cpp',
    '''struct FloorProxy {\n  float z;\n  Vec3 dark;\n  Vec3 light;\n};\n''',
    r'''

struct RoomWallBox {
  Vec3 centre;
  Vec3 halfExtent;
  Vec3 colour;
};
'''
)
replace_once(
    'src/demo/DemoWorld.cpp',
    '''struct LevelDefinition {\n  std::vector<RenderObject> objects;\n''',
    '''struct LevelDefinition {\n  RoomLayout roomLayout;\n  std::vector<RenderObject> objects;\n'''
)
insert_after(
    'src/demo/DemoWorld.cpp',
    '''NavigationLink navigationLink(const StairConnection& connection) {\n  NavigationLink link;\n  link.fromLevelId = connection.lowerLevelId;\n  link.toLevelId = connection.upperLevelId;\n  link.fromPosition = {connection.centreX, connection.lowY, 0.0f};\n  link.toPosition = {connection.centreX, connection.highY, 0.0f};\n  link.forwardTraversal = staircaseTraversal(ascendingStaircase(connection));\n  link.reverseTraversal = staircaseTraversal(descendingStaircase(connection));\n  link.bidirectional = true;\n  return link;\n}\n''',
    r'''

Room demoRoom(const std::string& id, float x, float y) {
  Room room;
  room.id = id;
  room.centre = {x, y, 0.0f};
  room.width = ROOM_SIZE;
  room.depth = ROOM_SIZE;
  room.floorZ = 0.0f;
  room.wallHeight = ROOM_WALL_HEIGHT;
  room.wallThickness = ROOM_WALL_THICKNESS;
  return room;
}

RoomConnection roomConnection(
  const std::string& id,
  const std::string& a,
  RoomSide aSide,
  const std::string& b,
  RoomSide bSide
) {
  RoomConnection connection;
  connection.id = id;
  connection.a.roomId = a;
  connection.a.side = aSide;
  connection.a.offset = 0.0f;
  connection.a.width = ROOM_OPENING_WIDTH;
  connection.b.roomId = b;
  connection.b.side = bSide;
  connection.b.offset = 0.0f;
  connection.b.width = ROOM_OPENING_WIDTH;
  connection.openPassage = true;
  return connection;
}

RoomLayout lowerRoomLayout() {
  RoomLayout layout;
  layout.rooms.push_back(demoRoom("centre", 0.0f, 0.0f));
  layout.rooms.push_back(demoRoom("north", 0.0f, ROOM_SIZE));
  layout.rooms.push_back(demoRoom("south", 0.0f, -ROOM_SIZE));
  layout.rooms.push_back(demoRoom("west", -ROOM_SIZE, 0.0f));
  layout.rooms.push_back(demoRoom("east", ROOM_SIZE, 0.0f));
  layout.connections.push_back(roomConnection("centre-north", "centre", RoomSide::North, "north", RoomSide::South));
  layout.connections.push_back(roomConnection("centre-south", "centre", RoomSide::South, "south", RoomSide::North));
  layout.connections.push_back(roomConnection("centre-west", "centre", RoomSide::West, "west", RoomSide::East));
  layout.connections.push_back(roomConnection("centre-east", "centre", RoomSide::East, "east", RoomSide::West));
  return layout;
}

RoomLayout middleRoomLayout() {
  RoomLayout layout;
  layout.rooms.push_back(demoRoom("centre", 0.0f, 0.0f));
  layout.rooms.push_back(demoRoom("north", 0.0f, ROOM_SIZE));
  layout.rooms.push_back(demoRoom("south", 0.0f, -ROOM_SIZE));
  layout.rooms.push_back(demoRoom("north-west", -ROOM_SIZE, ROOM_SIZE));
  layout.rooms.push_back(demoRoom("south-east", ROOM_SIZE, -ROOM_SIZE));
  layout.connections.push_back(roomConnection("centre-north", "centre", RoomSide::North, "north", RoomSide::South));
  layout.connections.push_back(roomConnection("centre-south", "centre", RoomSide::South, "south", RoomSide::North));
  layout.connections.push_back(roomConnection("north-west", "north", RoomSide::West, "north-west", RoomSide::East));
  layout.connections.push_back(roomConnection("south-east", "south", RoomSide::East, "south-east", RoomSide::West));
  return layout;
}

RoomLayout upperRoomLayout() {
  RoomLayout layout;
  layout.rooms.push_back(demoRoom("centre", 0.0f, 0.0f));
  layout.rooms.push_back(demoRoom("west", -ROOM_SIZE, 0.0f));
  layout.rooms.push_back(demoRoom("east", ROOM_SIZE, 0.0f));
  layout.rooms.push_back(demoRoom("south", 0.0f, -ROOM_SIZE));
  layout.rooms.push_back(demoRoom("far-south", 0.0f, -ROOM_SIZE * 2.0f));
  layout.connections.push_back(roomConnection("centre-west", "centre", RoomSide::West, "west", RoomSide::East));
  layout.connections.push_back(roomConnection("centre-east", "centre", RoomSide::East, "east", RoomSide::West));
  layout.connections.push_back(roomConnection("centre-south", "centre", RoomSide::South, "south", RoomSide::North));
  layout.connections.push_back(roomConnection("south-far-south", "south", RoomSide::South, "far-south", RoomSide::North));
  return layout;
}
'''
)
replace_once(
    'src/demo/DemoWorld.cpp',
    '''  explicit DemoLevel(LevelDefinition definition)\n      : definition_(std::move(definition)) {\n    buildCollisionObjects();\n''',
    '''  explicit DemoLevel(LevelDefinition definition)\n      : definition_(std::move(definition)) {\n    buildRoomWalls();\n    buildCollisionObjects();\n'''
)
insert_after(
    'src/demo/DemoWorld.cpp',
    '''  const std::vector<WorldObject>& objects() const override {\n    return worldObjects_;\n  }\n''',
    '''\n  const RoomLayout* roomLayout() const override {\n    return &definition_.roomLayout;\n  }\n'''
)
# Static walls are generated from arbitrary room rectangles and connection
# portals. Multiple openings on one wall are supported and merged into gaps.
text = read('src/demo/DemoWorld.cpp')
start = text.index('  void buildCollisionObjects() {')
end = text.index('  void buildStairSteps() {', start)
replacement = r'''  void buildRoomWalls() {
    roomWalls_.clear();
    const RoomSide sides[4] = {
      RoomSide::North,
      RoomSide::South,
      RoomSide::East,
      RoomSide::West
    };

    struct Gap {
      float minimum;
      float maximum;
    };

    for (const Room& room : definition_.roomLayout.rooms) {
      for (RoomSide side : sides) {
        std::vector<Gap> gaps;
        for (const RoomConnection& connection : definition_.roomLayout.connections) {
          if (!connection.openPassage) continue;
          const RoomPortal* portal = nullptr;
          if (connection.a.roomId == room.id && connection.a.side == side) portal = &connection.a;
          if (connection.b.roomId == room.id && connection.b.side == side) portal = &connection.b;
          if (!portal) continue;
          const float halfOpening = std::max(0.0f, portal->width) * 0.5f;
          gaps.push_back({portal->offset - halfOpening, portal->offset + halfOpening});
        }

        const bool horizontal = side == RoomSide::North || side == RoomSide::South;
        const float halfSpan = horizontal ? room.width * 0.5f : room.depth * 0.5f;
        std::sort(gaps.begin(), gaps.end(), [](const Gap& a, const Gap& b) {
          return a.minimum < b.minimum;
        });

        auto emit = [&](float minimum, float maximum) {
          minimum = std::max(minimum, -halfSpan);
          maximum = std::min(maximum, halfSpan);
          if (maximum - minimum <= 0.02f) return;

          RoomWallBox wall;
          wall.colour = definition_.floorDark * 0.68f;
          const float centreAlong = (minimum + maximum) * 0.5f;
          const float halfAlong = (maximum - minimum) * 0.5f;
          const float z = room.floorZ + room.wallHeight * 0.5f;
          if (horizontal) {
            const float y = room.centre.y +
              (side == RoomSide::North ? room.depth * 0.5f : -room.depth * 0.5f);
            wall.centre = {room.centre.x + centreAlong, y, z};
            wall.halfExtent = {halfAlong, room.wallThickness * 0.5f, room.wallHeight * 0.5f};
          } else {
            const float x = room.centre.x +
              (side == RoomSide::East ? room.width * 0.5f : -room.width * 0.5f);
            wall.centre = {x, room.centre.y + centreAlong, z};
            wall.halfExtent = {room.wallThickness * 0.5f, halfAlong, room.wallHeight * 0.5f};
          }
          roomWalls_.push_back(wall);
        };

        float cursor = -halfSpan;
        for (const Gap& gap : gaps) {
          const float gapMinimum = std::max(-halfSpan, gap.minimum);
          const float gapMaximum = std::min(halfSpan, gap.maximum);
          if (gapMaximum <= cursor) continue;
          emit(cursor, gapMinimum);
          cursor = std::max(cursor, gapMaximum);
        }
        emit(cursor, halfSpan);
      }
    }
  }

  void buildCollisionObjects() {
    worldObjects_.clear();
    worldObjects_.reserve(definition_.objects.size() + roomWalls_.size());
    for (const RenderObject& object : definition_.objects) {
      const Vec3 extent = objectExtent(object);
      WorldObject worldObject;
      worldObject.solid = object.solid;
      worldObject.hitBox.minimum = object.position - extent;
      worldObject.hitBox.maximum = object.position + extent;
      worldObjects_.push_back(worldObject);
    }
    for (const RoomWallBox& wall : roomWalls_) {
      WorldObject worldObject;
      worldObject.solid = true;
      worldObject.hitBox.minimum = wall.centre - wall.halfExtent;
      worldObject.hitBox.maximum = wall.centre + wall.halfExtent;
      worldObjects_.push_back(worldObject);
    }
  }

'''
write('src/demo/DemoWorld.cpp', text[:start] + replacement + text[end:])
replace_once(
    'src/demo/DemoWorld.cpp',
    '''  bool overlapsStatic(std::size_t objectIndex, const Object& candidate) const {\n    if (objectIndex >= definition_.objects.size()) return false;\n    const RenderObject& object = definition_.objects[objectIndex];\n    return object.solid && overlapsConvexObject(candidate, object);\n  }\n\n  bool intersectsSolid(const HitBox& hitBox) const override {\n    Object candidate;\n    candidate.hitBox = hitBox;\n    for (std::size_t index = 0; index < definition_.objects.size(); ++index) {\n      if (overlapsStatic(index, candidate)) return true;\n    }\n    return false;\n  }\n''',
    r'''  bool overlapsStatic(std::size_t objectIndex, const Object& candidate) const {
    if (objectIndex < definition_.objects.size()) {
      const RenderObject& object = definition_.objects[objectIndex];
      return object.solid && overlapsConvexObject(candidate, object);
    }

    const std::size_t wallIndex = objectIndex - definition_.objects.size();
    if (wallIndex >= roomWalls_.size()) return false;
    Object wall;
    wall.location = candidate.location;
    wall.location.position = {0.0f, 0.0f, 0.0f};
    wall.hitBox.minimum = roomWalls_[wallIndex].centre - roomWalls_[wallIndex].halfExtent;
    wall.hitBox.maximum = roomWalls_[wallIndex].centre + roomWalls_[wallIndex].halfExtent;
    wall.solid = true;
    return wall.overlaps(candidate);
  }

  bool intersectsSolid(const HitBox& hitBox) const override {
    Object candidate;
    candidate.hitBox = hitBox;
    for (std::size_t index = 0; index < worldObjects_.size(); ++index) {
      if (overlapsStatic(index, candidate)) return true;
    }
    return false;
  }
'''
)
replace_once(
    'src/demo/DemoWorld.cpp',
    '''    bounds_.points.reserve(\n      4 + definition_.objects.size() * 8 + definition_.staircases.size() * 8\n    );\n\n    bounds_.points.push_back({-GROUND_LIMIT, -GROUND_LIMIT, 0.0f});\n    bounds_.points.push_back({GROUND_LIMIT, -GROUND_LIMIT, 0.0f});\n    bounds_.points.push_back({-GROUND_LIMIT, GROUND_LIMIT, 0.0f});\n    bounds_.points.push_back({GROUND_LIMIT, GROUND_LIMIT, 0.0f});\n''',
    r'''    bounds_.points.reserve(
      definition_.roomLayout.rooms.size() * 8 +
      definition_.objects.size() * 8 + definition_.staircases.size() * 8
    );

    for (const Room& room : definition_.roomLayout.rooms) {
      const float halfWidth = room.width * 0.5f;
      const float halfDepth = room.depth * 0.5f;
      for (float x : {room.centre.x - halfWidth, room.centre.x + halfWidth}) {
        for (float y : {room.centre.y - halfDepth, room.centre.y + halfDepth}) {
          bounds_.points.push_back({x, y, room.floorZ});
          bounds_.points.push_back({x, y, room.floorZ + room.wallHeight});
        }
      }
    }
'''
)
# Replace the old one-square ground plane with arbitrary room footprints.
text = read('src/demo/DemoWorld.cpp')
start = text.index('  bool intersectGround(const Ray& ray, float minimum, float maximum, Hit& hit) const {')
end = text.index('  Hit traceClosest(', start)
replacement = r'''  bool intersectGround(const Ray& ray, float minimum, float maximum, Hit& hit) const {
    if (std::fabs(ray.direction.z) < 1e-7f) return false;
    bool found = false;
    float closest = maximum;

    for (const Room& room : definition_.roomLayout.rooms) {
      const float t = (room.floorZ - ray.origin.z) / ray.direction.z;
      if (t < minimum || t > closest) continue;
      const Vec3 point = ray.origin + ray.direction * t;
      if (!room.containsXY(point.x, point.y, EPSILON)) continue;
      if (insideFloorHole(point)) continue;

      found = true;
      closest = t;
      hit.found = true;
      hit.t = t;
      hit.point = point;
      hit.normal = {0.0f, 0.0f, 1.0f};
      hit.colour = floorColour(point);
      hit.kind = SceneSurfaceKind::Ground;
      hit.walkable = true;
    }
    return found;
  }

'''
write('src/demo/DemoWorld.cpp', text[:start] + replacement + text[end:])
replace_once(
    'src/demo/DemoWorld.cpp',
    '''    for (const RenderObject& object : definition_.objects) {\n      Hit hit;\n      if (intersectObject(ray, object, minimum, maximum, hit)) {\n        result = hit;\n        maximum = hit.t;\n      }\n    }\n\n    for (std::size_t index = 0; index < stairSteps_.size(); ++index) {\n''',
    r'''    for (const RenderObject& object : definition_.objects) {
      Hit hit;
      if (intersectObject(ray, object, minimum, maximum, hit)) {
        result = hit;
        maximum = hit.t;
      }
    }

    for (const RoomWallBox& wall : roomWalls_) {
      Hit hit;
      if (intersectAxisAlignedBox(ray, wall.centre, wall.halfExtent, minimum, maximum, hit)) {
        hit.colour = wall.colour;
        hit.kind = SceneSurfaceKind::Object;
        hit.walkable = false;
        result = hit;
        maximum = hit.t;
      }
    }

    for (std::size_t index = 0; index < stairSteps_.size(); ++index) {
'''
)
replace_once(
    'src/demo/DemoWorld.cpp',
    '''    for (const RenderObject& object : definition_.objects) {\n      if (intersectObject(ray, object, minimum, maximum, hit)) return true;\n    }\n    for (std::size_t index = 0; index < stairSteps_.size(); ++index) {\n''',
    r'''    for (const RenderObject& object : definition_.objects) {
      if (intersectObject(ray, object, minimum, maximum, hit)) return true;
    }
    for (const RoomWallBox& wall : roomWalls_) {
      if (intersectAxisAlignedBox(ray, wall.centre, wall.halfExtent, minimum, maximum, hit)) return true;
    }
    for (std::size_t index = 0; index < stairSteps_.size(); ++index) {
'''
)
replace_once(
    'src/demo/DemoWorld.cpp',
    '''  std::vector<WorldObject> worldObjects_;\n  std::vector<std::array<StairStep, STAIR_STEP_COUNT>> stairSteps_;\n''',
    '''  std::vector<WorldObject> worldObjects_;\n  std::vector<RoomWallBox> roomWalls_;\n  std::vector<std::array<StairStep, STAIR_STEP_COUNT>> stairSteps_;\n'''
)
insert_after('src/demo/DemoWorld.cpp', 'LevelDefinition lowerLevel() {\n  LevelDefinition level;\n', '  level.roomLayout = lowerRoomLayout();\n')
insert_after('src/demo/DemoWorld.cpp', 'LevelDefinition middleLevel() {\n  LevelDefinition level;\n', '  level.roomLayout = middleRoomLayout();\n')
insert_after('src/demo/DemoWorld.cpp', 'LevelDefinition upperLevel() {\n  LevelDefinition level;\n', '  level.roomLayout = upperRoomLayout();\n')
# Retire the old fake lower-floor proxies. The World now samples the actual
# configured lower levels through the static preview stack.
replace_once(
    'src/demo/DemoWorld.cpp',
    '''  level.floorProxies.push_back({\n    -STAIR_RISE,\n    LOWER_FLOOR_DARK,\n    LOWER_FLOOR_LIGHT\n  });\n\n''',
    ''
)
replace_once(
    'src/demo/DemoWorld.cpp',
    '''  level.floorProxies.push_back({\n    -STAIR_RISE,\n    MIDDLE_FLOOR_DARK,\n    MIDDLE_FLOOR_LIGHT\n  });\n''',
    ''
)
insert_after(
    'src/demo/DemoWorld.cpp',
    '''  setLevelId(2, "upper");\n''',
    '''  setLevelViewOrigin("lower", {0.0f, 0.0f, 0.0f});\n  setLevelViewOrigin("middle", {0.0f, 0.0f, STAIR_RISE});\n  setLevelViewOrigin("upper", {0.0f, 0.0f, STAIR_RISE * 2.0f});\n  setLowerLevelPreviewDepth(2);\n'''
)

# Pointer destinations now retain the source level of a visible lower preview.
replace_once(
    'src/demo/DemoApplication.cpp',
    '''  engine::SceneSurfaceHit surface;\n  if (!world_.pickWalkableSurface(ray, surface)) return false;\n\n  engine::EntityLocation destination;\n  destination.levelId = world_.activeLevelId();\n  destination.position = surface.point;\n''',
    '''  engine::SceneSurfaceHit surface;\n  engine::EntityLocation destination;\n  if (!world_.pickWalkableDestination(ray, destination, &surface)) return false;\n'''
)
replace_once(
    'src/demo/DemoApplication.cpp',
    '''  engine::SceneSurfaceHit surface;\n  return world_.pickWalkableSurface(ray, surface);\n''',
    '''  engine::EntityLocation destination;\n  return world_.pickWalkableDestination(ray, destination);\n'''
)

# Existing DemoWorld residency assertions now reflect active + configured lower
# preview levels. Generic worlds still default to exactly one resident level.
replace_once('scripts/liminal-lighting-smoke.cpp', '  if (world.residentLevelCount() != 1) return 26;\n  if (!world.isLevelResident("middle")) return 27;\n  if (world.isLevelResident("lower") || world.isLevelResident("upper")) return 28;\n', '  if (world.residentLevelCount() != 2) return 26;\n  if (!world.isLevelResident("middle") || !world.isLevelResident("lower")) return 27;\n  if (world.isLevelResident("upper")) return 28;\n')
replace_once('scripts/liminal-lighting-smoke.cpp', '  // Only that newly requested level may remain render-resident.\n', '  // The active level plus its configured lower previews remain render-resident.\n')
replace_once('scripts/liminal-lighting-smoke.cpp', '  if (world.residentLevelCount() != 1) return 29;\n  if (!world.isLevelResident("upper") || world.isLevelResident("middle")) return 30;\n', '  if (world.residentLevelCount() != 3) return 29;\n  if (!world.isLevelResident("upper") || !world.isLevelResident("middle") || !world.isLevelResident("lower")) return 30;\n')
replace_once('scripts/liminal-lighting-smoke.cpp', '  if (world.residentLevelCount() != 1) return 31;\n  if (!world.isLevelResident("middle") || world.isLevelResident("upper")) return 32;\n', '  if (world.residentLevelCount() != 2) return 31;\n  if (!world.isLevelResident("middle") || !world.isLevelResident("lower") || world.isLevelResident("upper")) return 32;\n')
replace_once('scripts/liminal-lighting-smoke.cpp', '  std::cout << "Liminal-space, single-level residency, and runtime-lighting smoke test passed.\\n";\n', '  std::cout << "Liminal-space, preview residency, and runtime-lighting smoke test passed.\\n";\n')

# Extend geometry regression coverage for generic room metadata, exact demo
# graphs, two-level preview residency, and lower-preview picking.
insert_after(
    'scripts/world-geometry-smoke.cpp',
    '  isoweb::demo::DemoWorld world;\n',
    r'''

  const auto* lowerRooms = world.roomLayout("lower");
  const auto* middleRooms = world.roomLayout("middle");
  const auto* upperRooms = world.roomLayout("upper");
  if (!lowerRooms || !middleRooms || !upperRooms) return 18;
  if (lowerRooms->rooms.size() != 5 || lowerRooms->connections.size() != 4) return 19;
  if (middleRooms->rooms.size() != 5 || middleRooms->connections.size() != 4) return 20;
  if (upperRooms->rooms.size() != 5 || upperRooms->connections.size() != 4) return 21;
  if (!lowerRooms->connected("centre", "north") || !lowerRooms->connected("centre", "west")) return 22;
  if (!middleRooms->connected("north", "north-west") || !middleRooms->connected("south", "south-east")) return 23;
  if (!upperRooms->connected("centre", "south") || !upperRooms->connected("south", "far-south")) return 24;
  if (world.lowerLevelPreviewDepth() != 2) return 25;
  if (world.residentLevelCount() != 2 || !world.isLevelResident("lower") || !world.isLevelResident("middle")) return 26;

  // The west arm of the lower cross is not covered by the middle Z. A visible
  // ray there must hit the real lower room at its stacked height and pick a
  // destination in lower-level local coordinates rather than middle.
  const Ray exposedLowerRay{{-8.80f, 0.0f, 8.0f}, {0.0f, 0.0f, -1.0f}};
  SceneSurfaceHit exposedLower;
  if (!world.traceEnvironment(exposedLowerRay, exposedLower)) return 27;
  if (exposedLower.kind != SceneSurfaceKind::Ground || !near(exposedLower.point.z, -1.40f)) return 28;
  EntityLocation exposedDestination;
  if (!world.pickWalkableDestination(exposedLowerRay, exposedDestination)) return 29;
  if (exposedDestination.levelId != "lower") return 30;
  if (!near(exposedDestination.position.x, -8.80f) || !near(exposedDestination.position.z, 0.0f)) return 31;

  if (!world.levelUp()) return 32;
  if (world.activeLevelId() != "upper" || world.residentLevelCount() != 3) return 33;
  if (!world.isLevelResident("middle") || !world.isLevelResident("lower")) return 34;
  if (!world.levelDown()) return 35;
'''
)
replace_once(
    'scripts/world-geometry-smoke.cpp',
    '  std::cout << "Unified world geometry and physical stair traversal smoke test passed.\\n";\n',
    '  std::cout << "Room graphs, stacked previews, preview picking, and physical stair traversal smoke test passed.\\n";\n'
)

print('room feature transformation applied')
