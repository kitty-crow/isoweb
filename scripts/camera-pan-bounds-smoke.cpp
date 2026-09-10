#include <cmath>
#include <cstdlib>
#include <iostream>

#include "engine/camera/Camera.hpp"

using isoweb::engine::Camera;
using isoweb::engine::CameraConfig;
using isoweb::engine::Vec3;
using isoweb::engine::WorldBounds;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "[camera-pan-bounds] " << message << '\n';
    std::exit(1);
  }
}

WorldBounds makeBounds() {
  WorldBounds bounds;
  bounds.focus = {0.0f, 0.0f, 0.0f};
  for (float x : {-13.2f, 13.2f}) {
    for (float y : {-13.2f, 13.2f}) {
      for (float z : {0.0f, 1.8f}) bounds.points.push_back({x, y, z});
    }
  }
  return bounds;
}

} // namespace

int main() {
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
  const WorldBounds bounds = makeBounds();
  const int width = 720;
  const int height = 720;

  require(camera.canPan(width, height, bounds), "large level did not enable panning");

  // The historical fixed 3.25-world-unit limit only reached the centre room.
  // A large authored level must allow the camera to travel well beyond that.
  for (int i = 0; i < 40; ++i) camera.pan(1.0f, 0.0f, width, height, bounds);
  const float positiveDistance = std::sqrt(
    camera.panX() * camera.panX() + camera.panY() * camera.panY()
  );
  require(positiveDistance > 6.0f, "pan still stopped near the legacy centre-room limit");

  // Repeated input must still clamp at the real projected level edge instead
  // of allowing unbounded empty-space panning.
  const float clampedX = camera.panX();
  const float clampedY = camera.panY();
  for (int i = 0; i < 40; ++i) camera.pan(1.0f, 0.0f, width, height, bounds);
  require(
    std::fabs(camera.panX() - clampedX) < 0.05f &&
    std::fabs(camera.panY() - clampedY) < 0.05f,
    "camera did not clamp at the authored level boundary"
  );

  const auto state = camera.controlState(width, height, bounds);
  require(!state.canPanRight, "pan-right control stayed enabled at the real level edge");
  require(state.canPanLeft, "pan-left control disabled before returning across the level");

  std::cout << "Camera pan bounds track authored level geometry.\n";
  return 0;
}
