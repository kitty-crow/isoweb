#include "engine/camera/Camera.hpp"

#include <algorithm>
#include <cmath>

namespace isoweb {
namespace engine {

Camera::Camera(const CameraConfig& config) : config_(config) {}

float Camera::yawRadians() const {
  return -yawStep_ * PI * 0.25f;
}

Vec3 Camera::forward() const {
  return rotateZ(normalise({1.0f, 1.0f, -1.0f}), yawRadians());
}

Vec3 Camera::groundRight() const {
  return normalise(cross(forward(), {0.0f, 0.0f, 1.0f}));
}

Vec3 Camera::groundDown() const {
  const Vec3 value = forward();
  return normalise({value.x, value.y, 0.0f});
}

bool Camera::wholeZoom() const {
  return zoomPreset_ == 0;
}

float Camera::zoomScale() const {
  switch (zoomPreset_) {
    case 5: return 4.0f;
    case 4: return 2.0f;
    case 3: return 1.0f;
    case 2: return 0.5f;
    case 1: return 0.25f;
    default: return 0.25f;
  }
}

float Camera::wholeViewHeight(float aspect, const WorldBounds& bounds) const {
  const Vec3 f = forward();
  const Vec3 r = normalise(cross(f, {0.0f, 0.0f, 1.0f}));
  const Vec3 u = normalise(cross(r, f));

  float minR = 1e9f;
  float maxR = -1e9f;
  float minU = 1e9f;
  float maxU = -1e9f;

  for (const Vec3& point : bounds.points) {
    const Vec3 relative = point - bounds.focus;
    const float projectedR = dot(relative, r);
    const float projectedU = dot(relative, u);
    minR = std::min(minR, projectedR);
    maxR = std::max(maxR, projectedR);
    minU = std::min(minU, projectedU);
    maxU = std::max(maxU, projectedU);
  }

  const float halfWidth = std::max(std::fabs(minR), std::fabs(maxR));
  const float halfHeight = std::max(std::fabs(minU), std::fabs(maxU));
  return std::max(halfHeight * 2.04f, (halfWidth * 2.04f) / aspect);
}

float Camera::baseViewHeight(float aspect) const {
  return std::max(config_.baseViewHeight, config_.minimumViewWidth / aspect);
}

float Camera::wholeZoomScale(int frameWidth, int frameHeight, const WorldBounds& bounds) const {
  const float aspect = static_cast<float>(frameWidth) / frameHeight;
  return baseViewHeight(aspect) / wholeViewHeight(aspect, bounds);
}

float Camera::viewHeight(int frameWidth, int frameHeight, const WorldBounds& bounds) const {
  const float aspect = static_cast<float>(frameWidth) / frameHeight;
  return wholeZoom() ? wholeViewHeight(aspect, bounds) : baseViewHeight(aspect) / zoomScale();
}

bool Camera::canPan(int frameWidth, int frameHeight, const WorldBounds& bounds) const {
  const float aspect = static_cast<float>(frameWidth) / frameHeight;
  return viewHeight(frameWidth, frameHeight, bounds) + 0.0001f < wholeViewHeight(aspect, bounds);
}

bool Camera::wouldPan(
  float right,
  float down,
  int frameWidth,
  int frameHeight,
  const WorldBounds& bounds
) const {
  if (!canPan(frameWidth, frameHeight, bounds)) return false;

  const Vec3 delta = groundRight() * right + groundDown() * down;
  const float nextX = std::max(-config_.panLimit, std::min(config_.panLimit, panX_ + delta.x));
  const float nextY = std::max(-config_.panLimit, std::min(config_.panLimit, panY_ + delta.y));
  return std::fabs(nextX - panX_) > 0.0001f || std::fabs(nextY - panY_) > 0.0001f;
}

CameraControlState Camera::controlState(
  int frameWidth,
  int frameHeight,
  const WorldBounds& bounds
) const {
  CameraControlState state;
  const int position = sequencePosition(frameWidth, frameHeight, bounds);

  state.canZoomIn = position + 1 < sequenceLength();
  state.canZoomOut = position > 0;
  state.canResetZoom = zoomPreset_ != 3;
  state.canResetYaw = yawStep_ != 0;

  state.canPanUp = wouldPan(0.0f, 1.0f, frameWidth, frameHeight, bounds);
  state.canPanDown = wouldPan(0.0f, -1.0f, frameWidth, frameHeight, bounds);
  state.canPanLeft = wouldPan(-1.0f, 0.0f, frameWidth, frameHeight, bounds);
  state.canPanRight = wouldPan(1.0f, 0.0f, frameWidth, frameHeight, bounds);
  state.canResetPan = canPan(frameWidth, frameHeight, bounds) &&
    (std::fabs(panX_) > 0.0001f || std::fabs(panY_) > 0.0001f);

  return state;
}

int Camera::detailedPresetAt(int position, int frameWidth, int frameHeight, const WorldBounds& bounds) const {
  if (wholeZoomScale(frameWidth, frameHeight, bounds) > 0.25f) {
    static const int order[6] = {1, 0, 2, 3, 4, 5};
    return order[position];
  }
  return position;
}

int Camera::presetAt(int position, int frameWidth, int frameHeight, const WorldBounds& bounds) const {
  return detailedMode_ ? detailedPresetAt(position, frameWidth, frameHeight, bounds) : position + 2;
}

int Camera::sequenceLength() const {
  return detailedMode_ ? 6 : 3;
}

int Camera::sequencePosition(int frameWidth, int frameHeight, const WorldBounds& bounds) const {
  for (int position = 0; position < sequenceLength(); ++position) {
    if (presetAt(position, frameWidth, frameHeight, bounds) == zoomPreset_) {
      return position;
    }
  }
  return detailedMode_ ? 3 : 1;
}

void Camera::rotateClockwise() {
  yawStep_ = (yawStep_ + 1) & 7;
}

void Camera::rotateCounterClockwise() {
  yawStep_ = (yawStep_ + 7) & 7;
}

void Camera::resetYaw() {
  yawStep_ = 0;
}

void Camera::stepZoom(int delta, int frameWidth, int frameHeight, const WorldBounds& bounds) {
  const int position = std::max(
    0,
    std::min(sequenceLength() - 1, sequencePosition(frameWidth, frameHeight, bounds) + delta)
  );
  zoomPreset_ = presetAt(position, frameWidth, frameHeight, bounds);
}

void Camera::resetZoom() {
  zoomPreset_ = 3;
}

void Camera::setDetailedMode(bool enabled) {
  detailedMode_ = enabled;
  if (!detailedMode_) {
    if (zoomPreset_ < 2) zoomPreset_ = 2;
    if (zoomPreset_ > 4) zoomPreset_ = 4;
  }
}

void Camera::pan(float right, float down, int frameWidth, int frameHeight, const WorldBounds& bounds) {
  if (!canPan(frameWidth, frameHeight, bounds)) return;

  const Vec3 delta = groundRight() * right + groundDown() * down;
  panX_ = std::max(-config_.panLimit, std::min(config_.panLimit, panX_ + delta.x));
  panY_ = std::max(-config_.panLimit, std::min(config_.panLimit, panY_ + delta.y));
}

void Camera::resetPan() {
  panX_ = 0.0f;
  panY_ = 0.0f;
}

} // namespace engine
} // namespace isoweb
