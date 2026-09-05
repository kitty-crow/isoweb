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

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_detailed_yaw_mode(int enabled) {
  application().setDetailedYawMode(enabled != 0);
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

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_level_up() {
  application().levelUp();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_level_down() {
  application().levelDown();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_level() {
  application().resetLevel();
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_level_count() {
  return static_cast<int>(application().levelCount());
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_active_level_index() {
  return static_cast<int>(application().activeLevelIndex());
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_default_level_index() {
  return static_cast<int>(application().defaultLevelIndex());
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_state_begin(float baseMovementSpeed) {
  application().resetRuntimeState(baseMovementSpeed);
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_state_add_character(
  float x,
  float y,
  float z,
  float forwardX,
  float forwardY,
  float width,
  float depth,
  float height,
  int levelIndex,
  int solid,
  int npc,
  int controllable,
  float speedMultiplier,
  float selectionRed,
  float selectionGreen,
  float selectionBlue
) {
  application().addCharacter(
    x,
    y,
    z,
    forwardX,
    forwardY,
    width,
    depth,
    height,
    levelIndex,
    solid != 0,
    npc != 0,
    controllable != 0,
    speedMultiplier,
    selectionRed,
    selectionGreen,
    selectionBlue
  );
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_character_count() {
  return static_cast<int>(application().characterCount());
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_pointer_action(float normalisedX, float normalisedY) {
  application().pointerAction(normalisedX, normalisedY);
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_clear_selection() {
  application().clearSelection();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_tick(float deltaSeconds) {
  application().tick(deltaSeconds);
}
