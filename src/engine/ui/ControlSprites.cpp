#include "engine/ui/ControlSprites.hpp"

#include <algorithm>
#include <cmath>

#include "DFPSR/api/drawAPI.h"

namespace isoweb {
namespace engine {
namespace {

constexpr int ROTATE_ARROW_WIDTH = 56;
constexpr int ROTATE_ARROW_HEIGHT = 44;
constexpr int RESET_DISK_SIZE = 38;
constexpr int ROTATE_LEFT_X = 18;
constexpr int ROTATE_ROW_GAP = 8;
constexpr int ZOOM_CONTROL_SIZE = 32;
constexpr int TOP_LEFT = 18;
constexpr int TOP_RIGHT = 18;
constexpr int TOP_CONTROL_TOP = 18;
constexpr int TOP_CONTROL_GAP = 6;
constexpr int CONTROL_BOTTOM = 18;
constexpr int PAN_ARROW_SIZE = 38;
constexpr int PAN_X_STEP = 48;
constexpr int PAN_Y_STEP = 36;
constexpr int PAN_PAD_RIGHT = 18;
constexpr int PAN_PAD_BOTTOM = 16;
constexpr float STICK_TRAVEL = 11.0f;
constexpr float PI = 3.14159265358979323846f;

float clampUnit(float value) {
  return std::max(-1.0f, std::min(1.0f, value));
}

int stickPixels(float value) {
  return static_cast<int>(std::round(clampUnit(value) * STICK_TRAVEL));
}

bool stickActive(const ControlStickOffset& value) {
  return std::fabs(value.x) > 0.01f || std::fabs(value.y) > 0.01f;
}

float segmentDistance(float px, float py, float ax, float ay, float bx, float by) {
  const float x = bx - ax;
  const float y = by - ay;
  const float squaredLength = x * x + y * y;
  const float t = squaredLength > 0.0f
    ? std::max(0.0f, std::min(1.0f, ((px - ax) * x + (py - ay) * y) / squaredLength))
    : 0.0f;
  const float dx = px - (ax + x * t);
  const float dy = py - (ay + y * t);
  return std::sqrt(dx * dx + dy * dy);
}

float curvedArrowDistance(float px, float py) {
  constexpr int SEGMENTS = 28;
  const float centreX = 27.5f;
  const float centreY = 25.0f;
  const float radiusX = 19.0f;
  const float radiusY = 12.0f;
  const float start = PI * 0.89f;
  const float end = PI * 0.10f;

  float minimum = 1000.0f;
  float previousX = centreX + radiusX * std::cos(start);
  float previousY = centreY - radiusY * std::sin(start);

  for (int index = 1; index <= SEGMENTS; ++index) {
    const float angle = start + (end - start) * static_cast<float>(index) / SEGMENTS;
    const float x = centreX + radiusX * std::cos(angle);
    const float y = centreY - radiusY * std::sin(angle);
    minimum = std::min(minimum, segmentDistance(px, py, previousX, previousY, x, y));
    previousX = x;
    previousY = y;
  }

  const float tangentX = std::sin(end) * radiusX;
  const float tangentY = std::cos(end) * radiusY;
  const float inverseTangentLength = 1.0f / std::sqrt(tangentX * tangentX + tangentY * tangentY);
  const float dx = tangentX * inverseTangentLength;
  const float dy = tangentY * inverseTangentLength;
  const float normalX = -dy;
  const float normalY = dx;
  const float baseX = previousX - dx * 9.0f;
  const float baseY = previousY - dy * 9.0f;

  minimum = std::min(
    minimum,
    segmentDistance(px, py, previousX, previousY, baseX + normalX * 5.5f, baseY + normalY * 5.5f)
  );
  return std::min(
    minimum,
    segmentDistance(px, py, previousX, previousY, baseX - normalX * 5.5f, baseY - normalY * 5.5f)
  );
}

float arrowDistance(float px, float py, float dx, float dy) {
  const float centre = PAN_ARROW_SIZE * 0.5f;
  const float normalX = -dy;
  const float normalY = dx;
  const float tailX = centre - dx * 10.0f;
  const float tailY = centre - dy * 10.0f;
  const float baseX = centre + dx * 5.0f;
  const float baseY = centre + dy * 5.0f;
  const float tipX = centre + dx * 13.0f;
  const float tipY = centre + dy * 13.0f;

  float minimum = segmentDistance(px, py, tailX, tailY, baseX, baseY);
  minimum = std::min(
    minimum,
    segmentDistance(px, py, tipX, tipY, baseX + normalX * 6.0f, baseY + normalY * 6.0f)
  );
  return std::min(
    minimum,
    segmentDistance(px, py, tipX, tipY, baseX - normalX * 6.0f, baseY - normalY * 6.0f)
  );
}

float zoomGlyphDistance(float px, float py, bool plus) {
  const float centre = ZOOM_CONTROL_SIZE * 0.5f;
  const float arm = 8.5f;
  const float horizontal = segmentDistance(px, py, centre - arm, centre, centre + arm, centre);
  return plus
    ? std::min(horizontal, segmentDistance(px, py, centre, centre - arm, centre, centre + arm))
    : horizontal;
}

float levelGlyphDistance(float px, float py) {
  const float left = 13.0f;
  const float right = 25.0f;
  const float top = 13.5f;
  const float bottom = 24.5f;
  float distance = segmentDistance(px, py, left, top, right, top);
  distance = std::min(distance, segmentDistance(px, py, right, top, left, bottom));
  return std::min(distance, segmentDistance(px, py, left, bottom, right, bottom));
}

} // namespace

void ControlSprites::setStickOffset(ControlStick stick, float x, float y) {
  const std::size_t index = static_cast<std::size_t>(stick);
  if (index >= stickOffsets_.size()) return;
  stickOffsets_[index].x = clampUnit(x);
  stickOffsets_[index].y = clampUnit(y);
}

const ControlStickOffset& ControlSprites::stickOffset(ControlStick stick) const {
  const std::size_t index = static_cast<std::size_t>(stick);
  return stickOffsets_[index < stickOffsets_.size() ? index : 0];
}

void ControlSprites::spritePixel(
  dsr::OrderedImageRgbaU8& sprite,
  int x,
  int y,
  float distance,
  float strength
) {
  const float outer = std::max(0.0f, std::min(1.0f, 3.6f - distance));
  if (outer <= 0.0f) return;

  const float core = std::max(0.0f, std::min(1.0f, 2.25f - distance));
  const int alpha = static_cast<int>(outer * 235.0f * strength + 0.5f);
  const int shade = static_cast<int>(64.0f + core * 186.0f * strength + 0.5f);
  dsr::image_writePixel(sprite, x, y, dsr::ColorRgbaI32(shade, shade, shade, alpha));
}

void ControlSprites::buildRotate(dsr::OrderedImageRgbaU8& sprite, bool mirror, bool disabled) {
  sprite = dsr::image_create_RgbaU8(ROTATE_ARROW_WIDTH, ROTATE_ARROW_HEIGHT, true);
  dsr::image_fill(sprite, {0, 0, 0, 0});
  const float strength = disabled ? 0.28f : 1.0f;
  for (int y = 0; y < ROTATE_ARROW_HEIGHT; ++y) {
    for (int x = 0; x < ROTATE_ARROW_WIDTH; ++x) {
      const float sampleX = mirror ? ROTATE_ARROW_WIDTH - 1 - x + 0.5f : x + 0.5f;
      spritePixel(sprite, x, y, curvedArrowDistance(sampleX, y + 0.5f), strength);
    }
  }
}

void ControlSprites::buildPan(dsr::OrderedImageRgbaU8& sprite, float dx, float dy, bool disabled) {
  sprite = dsr::image_create_RgbaU8(PAN_ARROW_SIZE, PAN_ARROW_SIZE, true);
  dsr::image_fill(sprite, {0, 0, 0, 0});
  for (int y = 0; y < PAN_ARROW_SIZE; ++y) {
    for (int x = 0; x < PAN_ARROW_SIZE; ++x) {
      spritePixel(sprite, x, y, arrowDistance(x + 0.5f, y + 0.5f, dx, dy), disabled ? 0.28f : 1.0f);
    }
  }
}

void ControlSprites::buildReset(dsr::OrderedImageRgbaU8& sprite, bool disabled) {
  sprite = dsr::image_create_RgbaU8(RESET_DISK_SIZE, RESET_DISK_SIZE, true);
  dsr::image_fill(sprite, {0, 0, 0, 0});
  const float centre = RESET_DISK_SIZE * 0.5f;
  const float strength = disabled ? 0.28f : 1.0f;

  for (int y = 0; y < RESET_DISK_SIZE; ++y) {
    for (int x = 0; x < RESET_DISK_SIZE; ++x) {
      const float dx = x + 0.5f - centre;
      const float dy = y + 0.5f - centre;
      const float radiusSquared = dx * dx + dy * dy;
      if (radiusSquared <= 156.25f) {
        const float radius = std::sqrt(radiusSquared);
        const float edge = std::max(0.0f, std::min(1.0f, 13.5f - radius));
        const int shade = static_cast<int>((radiusSquared < 81.0f ? 198.0f : 232.0f) * strength);
        const int alpha = static_cast<int>(edge * 225.0f * strength + 0.5f);
        dsr::image_writePixel(sprite, x, y, {shade, shade, shade, alpha});
      }
    }
  }
}

void ControlSprites::buildLevelReset(dsr::OrderedImageRgbaU8& sprite, bool disabled) {
  buildReset(sprite, disabled);
  const float strength = disabled ? 0.28f : 1.0f;
  for (int y = 0; y < RESET_DISK_SIZE; ++y) {
    for (int x = 0; x < RESET_DISK_SIZE; ++x) {
      spritePixel(sprite, x, y, levelGlyphDistance(x + 0.5f, y + 0.5f), strength);
    }
  }
}

void ControlSprites::buildZoom(dsr::OrderedImageRgbaU8& sprite, bool plus, bool disabled) {
  sprite = dsr::image_create_RgbaU8(ZOOM_CONTROL_SIZE, ZOOM_CONTROL_SIZE, true);
  dsr::image_fill(sprite, {0, 0, 0, 0});
  const float strength = disabled ? 0.28f : 1.0f;
  for (int y = 0; y < ZOOM_CONTROL_SIZE; ++y) {
    for (int x = 0; x < ZOOM_CONTROL_SIZE; ++x) {
      spritePixel(sprite, x, y, zoomGlyphDistance(x + 0.5f, y + 0.5f, plus), strength);
    }
  }
}

void ControlSprites::ensureSprites() {
  if (!dsr::image_exists(clockwiseSprite_)) {
    buildRotate(clockwiseSprite_, false);
    buildRotate(counterClockwiseSprite_, true);
  }
  if (!dsr::image_exists(upSprite_)) {
    buildPan(upSprite_, 0.0f, -1.0f);
    buildPan(downSprite_, 0.0f, 1.0f);
    buildPan(leftSprite_, -1.0f, 0.0f);
    buildPan(rightSprite_, 1.0f, 0.0f);
    buildPan(upDisabled_, 0.0f, -1.0f, true);
    buildPan(downDisabled_, 0.0f, 1.0f, true);
    buildPan(leftDisabled_, -1.0f, 0.0f, true);
    buildPan(rightDisabled_, 1.0f, 0.0f, true);
  }
  if (!dsr::image_exists(resetSprite_)) {
    buildReset(resetSprite_);
    buildReset(resetDisabled_, true);
  }
  if (!dsr::image_exists(levelResetSprite_)) {
    buildLevelReset(levelResetSprite_);
    buildLevelReset(levelResetDisabled_, true);
  }
  if (!dsr::image_exists(plusSprite_)) {
    buildZoom(plusSprite_, true);
    buildZoom(minusSprite_, false);
    buildZoom(plusDisabled_, true, true);
    buildZoom(minusDisabled_, false, true);
  }
}

void ControlSprites::draw(
  dsr::OrderedImageRgbaU8& frame,
  int frameWidth,
  int frameHeight,
  const CameraControlState& cameraState,
  const LevelControlState& levelState
) {
  ensureSprites();

  // Zoom stays top-left and reads horizontally as: -  •  +.
  const int zoomOutX = TOP_LEFT;
  const int zoomResetX = zoomOutX + ZOOM_CONTROL_SIZE + TOP_CONTROL_GAP;
  const int zoomInX = zoomResetX + RESET_DISK_SIZE + TOP_CONTROL_GAP;
  const int zoomResetTop = TOP_CONTROL_TOP;
  const int zoomControlTop = zoomResetTop + (RESET_DISK_SIZE - ZOOM_CONTROL_SIZE) / 2;
  const ControlStickOffset& zoomStick = stickOffset(ControlStick::Zoom);
  dsr::OrderedImageRgbaU8& zoomIn = cameraState.canZoomIn ? plusSprite_ : plusDisabled_;
  dsr::OrderedImageRgbaU8& zoomReset = (cameraState.canResetZoom || stickActive(zoomStick))
    ? resetSprite_
    : resetDisabled_;
  dsr::OrderedImageRgbaU8& zoomOut = cameraState.canZoomOut ? minusSprite_ : minusDisabled_;
  dsr::draw_alphaFilter(frame, zoomOut, zoomOutX, zoomControlTop);
  dsr::draw_alphaFilter(
    frame,
    zoomReset,
    zoomResetX + stickPixels(zoomStick.x),
    zoomResetTop
  );
  dsr::draw_alphaFilter(frame, zoomIn, zoomInX, zoomControlTop);

  // Z-level stays top-right and remains vertical.
  const int levelX = frameWidth - TOP_RIGHT - PAN_ARROW_SIZE;
  const int levelUpTop = TOP_CONTROL_TOP;
  const int levelResetTop = levelUpTop + PAN_ARROW_SIZE + TOP_CONTROL_GAP;
  const int levelDownTop = levelResetTop + RESET_DISK_SIZE + TOP_CONTROL_GAP;
  const ControlStickOffset& levelStick = stickOffset(ControlStick::Level);
  dsr::OrderedImageRgbaU8& levelUp = levelState.canMoveUp ? upSprite_ : upDisabled_;
  dsr::OrderedImageRgbaU8& levelDown = levelState.canMoveDown ? downSprite_ : downDisabled_;
  dsr::OrderedImageRgbaU8& levelReset = (!levelState.atDefault || stickActive(levelStick))
    ? levelResetSprite_
    : levelResetDisabled_;
  dsr::draw_alphaFilter(frame, levelUp, levelX, levelUpTop);
  dsr::draw_alphaFilter(
    frame,
    levelReset,
    levelX,
    levelResetTop + stickPixels(levelStick.y)
  );
  dsr::draw_alphaFilter(frame, levelDown, levelX, levelDownTop);

  const int yawTop = frameHeight - CONTROL_BOTTOM - ROTATE_ARROW_HEIGHT;
  const int counterClockwiseX = ROTATE_LEFT_X;
  const int resetYawX = counterClockwiseX + ROTATE_ARROW_WIDTH + ROTATE_ROW_GAP;
  const int clockwiseX = resetYawX + RESET_DISK_SIZE + ROTATE_ROW_GAP;
  const int resetYawTop = yawTop + (ROTATE_ARROW_HEIGHT - RESET_DISK_SIZE) / 2;
  const ControlStickOffset& yawStick = stickOffset(ControlStick::Yaw);
  dsr::OrderedImageRgbaU8& yawReset = (cameraState.canResetYaw || stickActive(yawStick))
    ? resetSprite_
    : resetDisabled_;
  dsr::draw_alphaFilter(frame, counterClockwiseSprite_, counterClockwiseX, yawTop);
  dsr::draw_alphaFilter(
    frame,
    yawReset,
    resetYawX + stickPixels(yawStick.x),
    resetYawTop + stickPixels(yawStick.y)
  );
  dsr::draw_alphaFilter(frame, clockwiseSprite_, clockwiseX, yawTop);

  const int centreX = frameWidth - PAN_PAD_RIGHT - PAN_ARROW_SIZE - PAN_X_STEP;
  const int centreY = frameHeight - PAN_PAD_BOTTOM - PAN_ARROW_SIZE - PAN_Y_STEP;
  const ControlStickOffset& panStick = stickOffset(ControlStick::Pan);
  dsr::OrderedImageRgbaU8& left = cameraState.canPanLeft ? leftSprite_ : leftDisabled_;
  dsr::OrderedImageRgbaU8& right = cameraState.canPanRight ? rightSprite_ : rightDisabled_;
  dsr::OrderedImageRgbaU8& up = cameraState.canPanUp ? upSprite_ : upDisabled_;
  dsr::OrderedImageRgbaU8& down = cameraState.canPanDown ? downSprite_ : downDisabled_;
  dsr::OrderedImageRgbaU8& centre = (cameraState.canResetPan || stickActive(panStick))
    ? resetSprite_
    : resetDisabled_;

  dsr::draw_alphaFilter(frame, left, centreX - PAN_X_STEP, centreY);
  dsr::draw_alphaFilter(
    frame,
    centre,
    centreX + stickPixels(panStick.x),
    centreY + stickPixels(panStick.y)
  );
  dsr::draw_alphaFilter(frame, right, centreX + PAN_X_STEP, centreY);
  dsr::draw_alphaFilter(frame, up, centreX, centreY - PAN_Y_STEP);
  dsr::draw_alphaFilter(frame, down, centreX, centreY + PAN_Y_STEP);
}

} // namespace engine
} // namespace isoweb
