#include "demo/DemoApplication.hpp"

namespace isoweb {
namespace demo {

DemoApplication::DemoApplication()
    : camera_(engine::CameraConfig(3.25f, 6.15f, 5.50f)),
      renderer_(world_, camera_, controls_) {}

void DemoApplication::redraw() {
  renderer_.render();
  presenter_.present(
    renderer_.rgba(),
    renderer_.width(),
    renderer_.height(),
    camera_,
    renderer_.canPan(),
    renderer_.viewHeight(),
    renderer_.wholeZoomScale()
  );
}

void DemoApplication::render() {
  redraw();
}

void DemoApplication::resize(int width, int height) {
  renderer_.resize(width, height);
  redraw();
}

void DemoApplication::rotateClockwise() {
  camera_.rotateClockwise();
  redraw();
}

void DemoApplication::rotateCounterClockwise() {
  camera_.rotateCounterClockwise();
  redraw();
}

void DemoApplication::resetYaw() {
  camera_.resetYaw();
  redraw();
}

void DemoApplication::zoomIn() {
  camera_.stepZoom(1, renderer_.width(), renderer_.height(), world_.bounds());
  redraw();
}

void DemoApplication::zoomOut() {
  camera_.stepZoom(-1, renderer_.width(), renderer_.height(), world_.bounds());
  redraw();
}

void DemoApplication::resetZoom() {
  camera_.resetZoom();
  redraw();
}

void DemoApplication::setDetailedMode(bool enabled) {
  camera_.setDetailedMode(enabled);
  redraw();
}

void DemoApplication::pan(float right, float down) {
  camera_.pan(right, down, renderer_.width(), renderer_.height(), world_.bounds());
  redraw();
}

void DemoApplication::resetCamera() {
  camera_.resetPan();
  redraw();
}

} // namespace demo
} // namespace isoweb
