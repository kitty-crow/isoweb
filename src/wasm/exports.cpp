#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include <emscripten/emscripten.h>

#include "demo/DemoApplication.hpp"

namespace {

isoweb::demo::DemoApplication& application() {
  static isoweb::demo::DemoApplication instance;
  return instance;
}

std::string text(const char* value) {
  return value ? std::string(value) : std::string();
}

isoweb::engine::CharacterFacing facingFromInt(int value) {
  switch (value) {
    case 1: return isoweb::engine::CharacterFacing::Back;
    case 2: return isoweb::engine::CharacterFacing::Left;
    case 3: return isoweb::engine::CharacterFacing::Right;
    default: return isoweb::engine::CharacterFacing::Front;
  }
}

float missingFloat() {
  return std::numeric_limits<float>::quiet_NaN();
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_render() {
  application().render();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_tick(float deltaSeconds) {
  application().tick(deltaSeconds);
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_needs_tick() {
  return application().characterSystem().needsTick() ? 1 : 0;
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

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_pointer_tap(float x, float y, int additive) {
  return application().pointerTap(x, y, additive != 0) ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_drag_select(
  float x0,
  float y0,
  float x1,
  float y1,
  int additive
) {
  return static_cast<int>(application().dragSelect(x0, y0, x1, y1, additive != 0));
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_clear_selection() {
  application().clearSelection();
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_clear_entities() {
  application().clearEntities();
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_character_count() {
  return static_cast<int>(application().world().entities().characters().size());
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_selected_character_count() {
  return static_cast<int>(application().characterSystem().selection().ids().size());
}

extern "C" EMSCRIPTEN_KEEPALIVE float isoweb_character_position_x(const char* id) {
  const auto* character = application().character(text(id));
  return character ? character->location.position.x : missingFloat();
}

extern "C" EMSCRIPTEN_KEEPALIVE float isoweb_character_position_y(const char* id) {
  const auto* character = application().character(text(id));
  return character ? character->location.position.y : missingFloat();
}

extern "C" EMSCRIPTEN_KEEPALIVE float isoweb_character_position_z(const char* id) {
  const auto* character = application().character(text(id));
  return character ? character->location.position.z : missingFloat();
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_character_is_moving(const char* id) {
  const auto* character = application().character(text(id));
  return character && character->moving ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_create_character(
  const char* id,
  const char* worldId,
  const char* timelineId,
  const char* levelId,
  float x,
  float y,
  float z
) {
  isoweb::engine::EntityLocation location;
  location.worldId = text(worldId);
  location.timelineId = text(timelineId);
  location.levelId = text(levelId);
  location.position = {x, y, z};
  isoweb::engine::HitBox hitBox;
  hitBox.minimum = {-0.25f, -0.15f, 0.0f};
  hitBox.maximum = {0.25f, 0.15f, 1.70f};
  return application().createCharacter(
    text(id), location, {0.0f, 1.0f, 0.0f}, hitBox, true, false, true, 1.0f
  ) ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_set_character_location(
  const char* id,
  const char* worldId,
  const char* timelineId,
  const char* levelId,
  float x,
  float y,
  float z
) {
  auto* character = application().character(text(id));
  if (!character) return 0;
  character->location.worldId = text(worldId);
  character->location.timelineId = text(timelineId);
  character->location.levelId = text(levelId);
  character->location.position = {x, y, z};
  return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_set_character_forward(
  const char* id,
  float x,
  float y
) {
  auto* character = application().character(text(id));
  if (!character) return 0;
  character->forward = {x, y, 0.0f};
  return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_set_character_hitbox(
  const char* id,
  float minX,
  float minY,
  float minZ,
  float maxX,
  float maxY,
  float maxZ
) {
  auto* character = application().character(text(id));
  if (!character) return 0;
  character->hitBox.minimum = {minX, minY, minZ};
  character->hitBox.maximum = {maxX, maxY, maxZ};
  return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_set_character_flags(
  const char* id,
  int solid,
  int npc,
  int controllable
) {
  auto* character = application().character(text(id));
  if (!character) return 0;
  character->solid = solid != 0;
  character->npc = npc != 0;
  character->controllable = controllable != 0;
  return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_set_character_speed(const char* id, float multiplier) {
  auto* character = application().character(text(id));
  if (!character) return 0;
  character->movementSpeedMultiplier = multiplier;
  return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_add_character_collision_tag(const char* id, const char* tag) {
  auto* character = application().character(text(id));
  if (!character) return 0;
  character->collisionTags.push_back(text(tag));
  return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_add_character_must_collide_with(const char* id, const char* selector) {
  auto* character = application().character(text(id));
  if (!character) return 0;
  character->mustCollideWith.push_back(text(selector));
  return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_clear_character_collision_filters(const char* id) {
  auto* character = application().character(text(id));
  if (!character) return 0;
  character->collisionTags.clear();
  character->mustCollideWith.clear();
  return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_set_character_sprite(
  const char* id,
  int state,
  const char* action,
  int facing,
  const char* resource,
  int frameCount,
  int columns,
  int rows,
  float nominalFps,
  float worldWidth,
  float worldHeight,
  int loop
) {
  isoweb::engine::SpriteAnimation animation;
  animation.resource = text(resource);
  animation.frameCount = static_cast<std::size_t>(frameCount > 0 ? frameCount : 1);
  animation.columns = static_cast<std::size_t>(columns > 0 ? columns : 1);
  animation.rows = static_cast<std::size_t>(rows > 0 ? rows : 1);
  animation.nominalFramesPerSecond = nominalFps;
  animation.worldWidth = worldWidth;
  animation.worldHeight = worldHeight;
  animation.loop = loop != 0;
  return application().setCharacterSprite(
    text(id), state, text(action), facingFromInt(facing), animation
  ) ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_set_character_action(const char* id, const char* action) {
  return application().setCharacterAction(text(id), text(action)) ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int isoweb_register_sprite_atlas(
  const char* resource,
  int width,
  int height,
  const std::uint8_t* rgba,
  int byteCount
) {
  return application().registerSpriteAtlas(
    text(resource), width, height, rgba, static_cast<std::size_t>(byteCount > 0 ? byteCount : 0)
  ) ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_base_movement_speed(float speed) {
  application().setBaseMovementSpeed(speed);
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_selection_mode(int mode) {
  application().setSelectionMode(
    mode == 1 ? isoweb::engine::SelectionMode::Single : isoweb::engine::SelectionMode::Multiple
  );
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_selection_style(
  float red,
  float green,
  float blue,
  float strength
) {
  isoweb::engine::SelectionStyle style;
  style.tint = {red, green, blue};
  style.strength = strength;
  application().setSelectionStyle(style);
}
