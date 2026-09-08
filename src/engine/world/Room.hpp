#pragma once

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
