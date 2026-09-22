#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/world/Room.hpp"
#include "engine/world/World.hpp"

namespace isoweb {
namespace engine {

enum class RuntimePrimitiveKind {
  Cube,
  Sphere,
  Cone,
  Pyramid,
  Dodecahedron,
  Icosahedron
};

struct RuntimeGroundRegion {
  Vec3 centre;
  float width = 0.0f;
  float depth = 0.0f;
  bool walkable = true;
};

struct RuntimePrimitive {
  RuntimePrimitiveKind kind = RuntimePrimitiveKind::Cube;
  Vec3 position;
  float size = 0.5f;
  float height = 1.0f;
  Vec3 colour = {0.6f, 0.6f, 0.6f};
  bool solid = true;
};

struct RuntimeFloorHole {
  float minimumX = 0.0f;
  float maximumX = 0.0f;
  float minimumY = 0.0f;
  float maximumY = 0.0f;
};

struct RuntimeStaircase {
  float startX = 0.0f;
  float startY = 0.0f;
  float endX = 0.0f;
  float endY = 0.0f;
  float startZ = 0.0f;
  float endZ = 0.0f;
  float width = 0.8f;
};

struct RuntimeFloorProxy {
  float z = 0.0f;
  Vec3 dark;
  Vec3 light;
};

struct RuntimeLevelDefinition {
  RoomLayout roomLayout;
  std::vector<RuntimeGroundRegion> ground;
  std::vector<RuntimePrimitive> objects;
  std::vector<RuntimeFloorHole> floorHoles;
  std::vector<RuntimeStaircase> staircases;
  std::vector<RuntimeFloorProxy> floorProxies;
  Vec3 lightPosition;
  Vec3 floorDark = {0.5f, 0.5f, 0.5f};
  Vec3 floorLight = {0.6f, 0.6f, 0.6f};
  Vec3 wallColour = {0.34f, 0.34f, 0.34f};
  Vec3 boundsFocus = {0.0f, 0.15f, 0.55f};
};

std::unique_ptr<IWorldLevel> makeRuntimeWorldLevel(RuntimeLevelDefinition definition);

} // namespace engine
} // namespace isoweb
