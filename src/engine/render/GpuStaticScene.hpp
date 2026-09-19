#pragma once

#include <vector>

#include "engine/math/Vec3.hpp"

namespace isoweb {
namespace engine {

// Compact, renderer-facing description of a static scene that can be evaluated
// by massively parallel GPU fragment invocations. Simulation, collision and
// authored world data remain CPU-owned; this is only a read-only render view.
struct GpuStaticBox {
  Vec3 centre;
  Vec3 halfExtent;
  Vec3 colour;
  bool floorPattern = false;
};

struct GpuStaticSphere {
  Vec3 centre;
  float radius = 0.0f;
  Vec3 colour;
};

struct GpuGroundRoom {
  Vec3 centre;
  float halfWidth = 0.0f;
  float halfDepth = 0.0f;
};

struct GpuFloorHole {
  float minimumX = 0.0f;
  float maximumX = 0.0f;
  float minimumY = 0.0f;
  float maximumY = 0.0f;
};

struct GpuStaticScene {
  Vec3 lightPosition;
  Vec3 floorDark;
  Vec3 floorLight;

  // Primary rays use the visual set (including camera cutaways). Shadow rays
  // use the physical set, so presentation cutaways never become light leaks.
  std::vector<GpuStaticBox> visualBoxes;
  std::vector<GpuStaticBox> shadowBoxes;
  std::vector<GpuStaticSphere> spheres;
  std::vector<GpuGroundRoom> rooms;
  std::vector<GpuFloorHole> floorHoles;

  void clear() {
    visualBoxes.clear();
    shadowBoxes.clear();
    spheres.clear();
    rooms.clear();
    floorHoles.clear();
  }
};

} // namespace engine
} // namespace isoweb
