#include "DemoWorldFixture.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/world/RuntimeWorld.hpp"

namespace isoweb {
namespace test {
namespace {

using engine::NavigationLink;
using engine::Room;
using engine::RoomConnection;
using engine::RoomLayout;
using engine::RoomSide;
using engine::RuntimeFloorHole;
using engine::RuntimeGroundRegion;
using engine::RuntimeLevelDefinition;
using engine::RuntimePrimitive;
using engine::RuntimePrimitiveKind;
using engine::RuntimeStaircase;
using engine::Vec3;

constexpr float GROUND_LIMIT = 4.40f;
constexpr float ROOM_SIZE = GROUND_LIMIT * 2.0f;
constexpr float DEMO_CHARACTER_HEIGHT = 1.65f;
constexpr float DEMO_MIN_STOREY_RATIO = 1.25f;
constexpr float DEMO_STOREY_HEIGHT = DEMO_CHARACTER_HEIGHT * DEMO_MIN_STOREY_RATIO;
constexpr float ROOM_WALL_HEIGHT = 1.80f;
constexpr float ROOM_WALL_THICKNESS = 0.12f;
constexpr float ROOM_OPENING_WIDTH = 1.45f;
constexpr int STAIR_STEP_COUNT = 10;
constexpr float STAIR_WIDTH = 0.78f;
constexpr float STAIR_LOW_Y = -3.30f;
constexpr float STAIR_HIGH_Y = -1.20f;
constexpr float LOWER_MIDDLE_STAIR_X = 2.15f;
constexpr float MIDDLE_UPPER_STAIR_X = 3.20f;
constexpr float STAIR_HOLE_INSET = 0.015f;
constexpr float STAIR_LANDING_MARGIN = 0.25f;

const Vec3 LOWER_FLOOR_DARK(0.34f, 0.34f, 0.36f);
const Vec3 LOWER_FLOOR_LIGHT(0.40f, 0.40f, 0.42f);
const Vec3 MIDDLE_FLOOR_DARK(0.567f, 0.6048f, 0.630f);
const Vec3 MIDDLE_FLOOR_LIGHT(0.621f, 0.6624f, 0.690f);
const Vec3 UPPER_FLOOR_DARK(0.74f, 0.74f, 0.76f);
const Vec3 UPPER_FLOOR_LIGHT(0.82f, 0.82f, 0.84f);

struct StairConnection {
  float centreX;
  float lowY;
  float highY;
  float rise;
  float width;
  const char* lowerLevelId;
  const char* upperLevelId;
};

const StairConnection LOWER_MIDDLE_STAIR = {
  LOWER_MIDDLE_STAIR_X, STAIR_LOW_Y, STAIR_HIGH_Y, DEMO_STOREY_HEIGHT,
  STAIR_WIDTH, "lower", "middle"
};
const StairConnection MIDDLE_UPPER_STAIR = {
  MIDDLE_UPPER_STAIR_X, STAIR_LOW_Y, STAIR_HIGH_Y, DEMO_STOREY_HEIGHT,
  STAIR_WIDTH, "middle", "upper"
};

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
  connection.a.width = ROOM_OPENING_WIDTH;
  connection.b.roomId = b;
  connection.b.side = bSide;
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

RuntimeStaircase ascendingStaircase(const StairConnection& connection) {
  return {
    connection.centreX, connection.lowY, connection.highY,
    0.0f, connection.rise, connection.width
  };
}

RuntimeStaircase descendingStaircase(const StairConnection& connection) {
  return {
    connection.centreX, connection.highY, connection.lowY,
    0.0f, -connection.rise, connection.width
  };
}

RuntimeFloorHole stairHole(const StairConnection& connection) {
  return {
    connection.centreX - connection.width * 0.5f + STAIR_HOLE_INSET,
    connection.centreX + connection.width * 0.5f - STAIR_HOLE_INSET,
    connection.lowY + STAIR_HOLE_INSET,
    connection.highY - STAIR_HOLE_INSET
  };
}

std::vector<Vec3> staircaseTraversal(const RuntimeStaircase& staircase) {
  std::vector<Vec3> result;
  result.reserve(STAIR_STEP_COUNT);
  const float lowerZ = std::min(staircase.startZ, staircase.endZ);
  const bool ascending = staircase.endZ > staircase.startZ;
  const float inverseStepCount = 1.0f / static_cast<float>(STAIR_STEP_COUNT);
  const float yStep = (staircase.endY - staircase.startY) * inverseStepCount;
  for (int index = 0; index < STAIR_STEP_COUNT; ++index) {
    const float y0 = staircase.startY + yStep * index;
    const float y1 = y0 + yStep;
    const float fraction = (ascending ? index + 1 : index) * inverseStepCount;
    const float topZ = staircase.startZ + (staircase.endZ - staircase.startZ) * fraction;
    result.push_back({
      staircase.centreX,
      (y0 + y1) * 0.5f,
      std::max(topZ, lowerZ + 0.025f)
    });
  }
  return result;
}

NavigationLink navigationLink(const StairConnection& connection) {
  NavigationLink link;
  link.id = std::string(connection.lowerLevelId) + "-" + connection.upperLevelId;
  link.type = "stairs";
  link.fromLevelId = connection.lowerLevelId;
  link.toLevelId = connection.upperLevelId;
  link.fromPosition = {connection.centreX, connection.lowY - STAIR_LANDING_MARGIN, 0.0f};
  link.toPosition = {connection.centreX, connection.highY + STAIR_LANDING_MARGIN, 0.0f};
  link.forwardTraversal = staircaseTraversal(ascendingStaircase(connection));
  link.reverseTraversal = staircaseTraversal(descendingStaircase(connection));
  link.bidirectional = true;
  return link;
}

void addRoomGround(RuntimeLevelDefinition& level) {
  for (const Room& room : level.roomLayout.rooms) {
    RuntimeGroundRegion ground;
    ground.centre = {room.centre.x, room.centre.y, room.floorZ};
    ground.width = room.width;
    ground.depth = room.depth;
    ground.walkable = true;
    level.ground.push_back(ground);
  }
}

RuntimeLevelDefinition lowerLevel() {
  RuntimeLevelDefinition level;
  level.roomLayout = lowerRoomLayout();
  level.lightPosition = {4.20f, -3.20f, 5.60f};
  level.floorDark = LOWER_FLOOR_DARK;
  level.floorLight = LOWER_FLOOR_LIGHT;
  level.wallColour = LOWER_FLOOR_DARK * 0.68f;
  level.roomLayout.previewFloorDark = LOWER_FLOOR_DARK;
  level.roomLayout.previewFloorLight = LOWER_FLOOR_LIGHT;
  level.roomLayout.previewWall = level.wallColour;
  addRoomGround(level);
  level.objects.push_back({RuntimePrimitiveKind::Cone, {-1.30f, -0.80f, 0.82f}, 0.86f, 1.64f, {0.62f, 0.25f, 0.82f}, true});
  level.objects.push_back({RuntimePrimitiveKind::Pyramid, {1.20f, 0.85f, 0.83f}, 0.92f, 1.66f, {0.96f, 0.78f, 0.16f}, true});
  level.staircases.push_back(ascendingStaircase(LOWER_MIDDLE_STAIR));
  return level;
}

RuntimeLevelDefinition middleLevel() {
  RuntimeLevelDefinition level;
  level.roomLayout = middleRoomLayout();
  level.lightPosition = {-3.60f, -4.20f, 6.50f};
  level.floorDark = MIDDLE_FLOOR_DARK;
  level.floorLight = MIDDLE_FLOOR_LIGHT;
  level.wallColour = MIDDLE_FLOOR_DARK * 0.68f;
  level.roomLayout.previewFloorDark = MIDDLE_FLOOR_DARK;
  level.roomLayout.previewFloorLight = MIDDLE_FLOOR_LIGHT;
  level.roomLayout.previewWall = level.wallColour;
  addRoomGround(level);
  level.objects.push_back({RuntimePrimitiveKind::Cube, {-1.05f, 0.65f, 0.775f}, 0.80f, 1.55f, {0.18f, 0.48f, 0.88f}, true});
  level.objects.push_back({RuntimePrimitiveKind::Sphere, {1.05f, -0.25f, 0.90f}, 0.90f, 1.80f, {0.95f, 0.43f, 0.12f}, true});
  level.floorHoles.push_back(stairHole(LOWER_MIDDLE_STAIR));
  level.staircases.push_back(descendingStaircase(LOWER_MIDDLE_STAIR));
  level.staircases.push_back(ascendingStaircase(MIDDLE_UPPER_STAIR));
  return level;
}

RuntimeLevelDefinition upperLevel() {
  RuntimeLevelDefinition level;
  level.roomLayout = upperRoomLayout();
  level.lightPosition = {3.80f, 4.40f, 7.20f};
  level.floorDark = UPPER_FLOOR_DARK;
  level.floorLight = UPPER_FLOOR_LIGHT;
  level.wallColour = UPPER_FLOOR_DARK * 0.68f;
  level.roomLayout.previewFloorDark = UPPER_FLOOR_DARK;
  level.roomLayout.previewFloorLight = UPPER_FLOOR_LIGHT;
  level.roomLayout.previewWall = level.wallColour;
  addRoomGround(level);
  level.objects.push_back({RuntimePrimitiveKind::Dodecahedron, {-1.35f, 0.95f, 1.00f}, 0.72f, 0.0f, {0.18f, 0.50f, 0.94f}, true});
  level.objects.push_back({RuntimePrimitiveKind::Icosahedron, {1.30f, -0.95f, 1.10f}, 0.78f, 0.0f, {0.90f, 0.16f, 0.14f}, true});
  level.floorHoles.push_back(stairHole(MIDDLE_UPPER_STAIR));
  level.staircases.push_back(descendingStaircase(MIDDLE_UPPER_STAIR));
  return level;
}

} // namespace

std::vector<std::unique_ptr<engine::IWorldLevel>> DemoWorldFixture::makeLevels() {
  std::vector<std::unique_ptr<engine::IWorldLevel>> levels;
  levels.reserve(3);
  levels.push_back(engine::makeRuntimeWorldLevel(lowerLevel()));
  levels.push_back(engine::makeRuntimeWorldLevel(middleLevel()));
  levels.push_back(engine::makeRuntimeWorldLevel(upperLevel()));
  return levels;
}

DemoWorldFixture::DemoWorldFixture()
    : engine::World(makeLevels(), 1) {
  setLevelId(0, "lower");
  setLevelId(1, "middle");
  setLevelId(2, "upper");
  setLevelViewOrigin("lower", {0.0f, 0.0f, 0.0f});
  setLevelViewOrigin("middle", {0.0f, 0.0f, DEMO_STOREY_HEIGHT});
  setLevelViewOrigin("upper", {0.0f, 0.0f, DEMO_STOREY_HEIGHT * 2.0f});
  setLowerLevelPreviewDepth(2);
  setLowerPreviewResolutionScale(1.0f);
  setNavigationLinks({
    navigationLink(LOWER_MIDDLE_STAIR),
    navigationLink(MIDDLE_UPPER_STAIR)
  });
}

} // namespace test
} // namespace isoweb
