#include <emscripten/emscripten.h>

#include "demo/DemoApplication.hpp"

namespace {

isoweb::demo::DemoApplication& application() {
  static isoweb::demo::DemoApplication instance;
  return instance;
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_render() {
  application().render();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_resize(int width, int height) {
  application().resize(width, height);
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_clockwise() {
  application().rotateClockwise();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_counterclockwise() {
  application().rotateCounterClockwise();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_yaw() {
  application().resetYaw();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_in() {
  application().zoomIn();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_out() {
  application().zoomOut();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_zoom() {
  application().resetZoom();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_detailed_mode(int enabled) {
  application().setDetailedMode(enabled != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_pan(float right, float down) {
  application().pan(right, down);
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_camera() {
  application().resetCamera();
}
