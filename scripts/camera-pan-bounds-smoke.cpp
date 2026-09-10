#include <cmath>
#include <cstdlib>
#include <iostream>

#include "engine/camera/Camera.hpp"

using namespace isoweb::engine;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "[camera-pan-bounds] " << message << '\n';
    std::exit(1);
  }
}

void addRoomBounds(WorldBounds& bounds, float centreX, float centreY) {
  constexpr float halfRoom = 4.40f;
  for (float x : {centreX - halfRoom, centreX + halfRoom}) {
    for (float y : {centreY - halfRoom, centreY + halfRoom}) {
      bounds.points.push_back({x, y, 0.0f});
      bounds.points.push_back({x, y, 1.80f});
    }
  }
}

WorldBounds makeMiddleFloorBounds() {
  constexpr float roomSize = 8.80f;
  WorldBounds bounds;
  bounds.focus = {0.0f, 0.15f, 0.55f};
  addRoomBounds(bounds, 0.0f, 0.0f);
  addRoomBounds(bounds, 0.0f, roomSize);
  addRoomBounds(bounds, 0.0f, -roomSize);
  addRoomBounds(bounds, -roomSize, roomSize);
  addRoomBounds(bounds, roomSize, -roomSize);
  return bounds;
}

void planarUpExtents(
  const WorldBounds& bounds,
  const Vec3& up,
  float& minimum,
  float& maximum
) {
  minimum = 1e9f;
  maximum = -1e9f;
  for (const Vec3& point : bounds.points) {
    const Vec3 relative(point.x - bounds.focus.x, point.y - bounds.focus.y, 0.0f);
    const float projected = dot(relative, up);
    minimum = std::min(minimum, projected);
    maximum = std::max(maximum, projected);
  }
}

} // namespace

int main() {
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
  const WorldBounds bounds = makeMiddleFloorBounds();

  // Reproduce the phone shape where the previous half-viewport inset was most
  // severe. At 1x it stopped around the centre room despite the Z-shaped floor.
  const int width = 390;
  const int height = 844;
  require(camera.canPan(width, height, bounds), "middle floor did not enable phone panning");

  const Vec3 right = camera.groundRight();
  const Vec3 up = normalise(cross(right, camera.forward()));
  float minimumUp = 0.0f;
  float maximumUp = 0.0f;
  planarUpExtents(bounds, up, minimumUp, maximumUp);

  // For the default camera basis, positive Camera::pan down has a positive
  // projection onto renderer-up. Drive it until it reaches the true planar
  // edge instead of stopping half a viewport before it.
  for (int i = 0; i < 240; ++i) camera.pan(0.0f, 1.0f, width, height, bounds);
  const Vec3 positivePan(camera.panX(), camera.panY(), 0.0f);
  const float positiveProjected = dot(positivePan, up);
  require(
    std::fabs(positiveProjected - maximumUp) < 0.05f,
    "portrait joystick pan stopped before the positive projected floor edge"
  );
  require(
    std::sqrt(camera.panX() * camera.panX() + camera.panY() * camera.panY()) > 8.0f,
    "portrait joystick pan still stops around the centre room"
  );
  auto state = camera.controlState(width, height, bounds);
  require(!state.canPanUp, "positive pan control stayed enabled after reaching the real floor edge");
  require(state.canPanDown, "opposite pan control disabled before returning across the floor");

  camera.resetPan();
  for (int i = 0; i < 240; ++i) camera.pan(0.0f, -1.0f, width, height, bounds);
  const Vec3 negativePan(camera.panX(), camera.panY(), 0.0f);
  const float negativeProjected = dot(negativePan, up);
  require(
    std::fabs(negativeProjected - minimumUp) < 0.05f,
    "portrait joystick pan stopped before the negative projected floor edge"
  );

  camera.resetPan();
  for (int i = 0; i < 240; ++i) camera.pan(1.0f, 0.0f, width, height, bounds);
  const Vec3 rightPan(camera.panX(), camera.panY(), 0.0f);
  float maximumRight = -1e9f;
  for (const Vec3& point : bounds.points) {
    const Vec3 relative(point.x - bounds.focus.x, point.y - bounds.focus.y, 0.0f);
    maximumRight = std::max(maximumRight, dot(relative, right));
  }
  require(
    std::fabs(dot(rightPan, right) - maximumRight) < 0.05f,
    "portrait joystick pan stopped before the real right floor edge"
  );

  std::cout << "Camera pan reaches the full active-floor footprint on a portrait viewport.\n";
  return 0;
}
