#include "engine/camera/Camera.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace isoweb {
namespace engine {
namespace {

constexpr float PI = 3.14159265358979323846f;

struct CameraBasisTable {
  std::array<Vec3, 8> forward;
  std::array<Vec3, 8> groundRight;
  std::array<Vec3, 8> groundDown;
};

const CameraBasisTable& cameraBasisTable() {
  // Yaw is deliberately quantised to eight states. Build the exact basis with
  // the original rotate/normalise maths once, rather than repeating trig and
  // square roots whenever rendering, picking or computing controls asks for it.
  static const CameraBasisTable table = [] {
    CameraBasisTable result;
    const Vec3 base = normalise({1.0f, 1.0f, -1.0f});
    for (int step = 0; step < 8; ++step) {
      const Vec3 forward = rotateZ(base, -step * PI * 0.25f);
      result.forward[step] = forward;
      result.groundRight[step] = normalise(cross(forward, {0.0f, 0.0f, 1.0f}));
      result.groundDown[step] = normalise({forward.x, forward.y, 0.0f});
    }
    return result;
  }();
  return table;
}

float clampPan(float value, float minimum, float maximum) {
  return std::max(minimum, std::min(maximum, value));
}

Vec3 clampPanToBounds(
  const Vec3& candidate,
  const Vec3& forward,
  const Vec3& right,
  const Vec3& down,
  float viewHeight,
  float aspect,
  const WorldBounds& bounds,
  float fallbackLimit
) {
  if (bounds.points.empty() || viewHeight <= 1e-7f || aspect <= 1e-7f) {
    return {
      clampPan(candidate.x, -fallbackLimit, fallbackLimit),
      clampPan(candidate.y, -fallbackLimit, fallbackLimit),
      0.0f
    };
  }

  const Vec3 up = normalise(cross(right, forward));
  const float verticalProjection = dot(down, up);
  if (std::fabs(verticalProjection) <= 1e-7f) return candidate;

  float minimumRight = 1e9f;
  float maximumRight = -1e9f;
  float minimumUp = 1e9f;
  float maximumUp = -1e9f;
  for (const Vec3& point : bounds.points) {
    const Vec3 relative = point - bounds.focus;
    const float projectedRight = dot(relative, right);
    const float projectedUp = dot(relative, up);
    minimumRight = std::min(minimumRight, projectedRight);
    maximumRight = std::max(maximumRight, projectedRight);
    minimumUp = std::min(minimumUp, projectedUp);
    maximumUp = std::max(maximumUp, projectedUp);
  }

  const float halfWidth = viewHeight * aspect * 0.5f;
  const float halfHeight = viewHeight * 0.5f;
  const auto centreRange = [](float minimum, float maximum, float halfViewport) {
    const float span = maximum - minimum;
    if (span > halfViewport * 2.0f) {
      return std::array<float, 2>{{minimum + halfViewport, maximum - halfViewport}};
    }
    return std::array<float, 2>{{maximum - halfViewport, minimum + halfViewport}};
  };

  const std::array<float, 2> rightRange = centreRange(minimumRight, maximumRight, halfWidth);
  const std::array<float, 2> upRange = centreRange(minimumUp, maximumUp, halfHeight);
  const float candidateRight = dot(candidate, right);
  const float candidateUp = dot(candidate, up);
  const float clampedRight = clampPan(candidateRight, rightRange[0], rightRange[1]);
  const float clampedUp = clampPan(candidateUp, upRange[0], upRange[1]);

  return right * clampedRight + down * (clampedUp / verticalProjection);
}

} // namespace

Camera::Camera(const CameraConfig& config) : config_(config) {}

Vec3 Camera::forward() const {
  return cameraBasisTable().forward[yawStep_ & 7];
}

Vec3 Camera::groundRight() const {
  return cameraBasisTable().groundRight[yawStep_ & 7];
}

Vec3 Camera::groundDown() const {
  return cameraBasisTable().groundDown[yawStep_ & 7];
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
  const Vec3 r = groundRight();
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
  if (wholeZoom()) return false;
  const float aspect = static_cast<float>(frameWidth) / frameHeight;
  const float currentHeight = baseViewHeight(aspect) / zoomScale();
  return currentHeight + 0.0001f < wholeViewHeight(aspect, bounds);
}

CameraControlState Camera::controlState(
  int frameWidth,
  int frameHeight,
  const WorldBounds& bounds
) const {
  const float aspect = static_cast<float>(frameWidth) / frameHeight;
  const float wholeHeight = wholeViewHeight(aspect, bounds);
  const float baseHeight = baseViewHeight(aspect);
  const float currentHeight = wholeZoom() ? wholeHeight : baseHeight / zoomScale();
  return controlState(
    frameWidth,
    frameHeight,
    bounds,
    currentHeight + 0.0001f < wholeHeight,
    baseHeight / wholeHeight
  );
}

CameraControlState Camera::controlState(
  int frameWidth,
  int frameHeight,
  const WorldBounds& bounds,
  bool panEnabled,
  float wholeZoomScaleValue
) const {
  CameraControlState state;
  const int position = sequencePosition(wholeZoomScaleValue);

  state.canZoomIn = position + 1 < sequenceLength();
  state.canZoomOut = position > 0;
  state.canResetZoom = zoomPreset_ != 3;
  state.canResetYaw = yawStep_ != 0;

  if (panEnabled) {
    const Vec3 rightAxis = groundRight();
    const Vec3 downAxis = groundDown();
    const float aspect = static_cast<float>(frameWidth) / frameHeight;
    const float currentHeight = viewHeight(frameWidth, frameHeight, bounds);
    const auto wouldMove = [&](float right, float down) {
      const Vec3 delta = rightAxis * right + downAxis * down;
      const Vec3 next = clampPanToBounds(
        {panX_ + delta.x, panY_ + delta.y, 0.0f},
        forward(),
        rightAxis,
        downAxis,
        currentHeight,
        aspect,
        bounds,
        config_.panLimit
      );
      return std::fabs(next.x - panX_) > 0.0001f ||
        std::fabs(next.y - panY_) > 0.0001f;
    };

    state.canPanUp = wouldMove(0.0f, 1.0f);
    state.canPanDown = wouldMove(0.0f, -1.0f);
    state.canPanLeft = wouldMove(-1.0f, 0.0f);
    state.canPanRight = wouldMove(1.0f, 0.0f);
    state.canResetPan = std::fabs(panX_) > 0.0001f || std::fabs(panY_) > 0.0001f;
  }

  return state;
}

int Camera::presetAt(int position, float wholeZoomScaleValue) const {
  if (!detailedMode_) return position + 2;
  if (wholeZoomScaleValue > 0.25f) {
    static const int order[6] = {1, 0, 2, 3, 4, 5};
    return order[position];
  }
  return position;
}

int Camera::sequenceLength() const {
  return detailedMode_ ? 6 : 3;
}

int Camera::sequencePosition(float wholeZoomScaleValue) const {
  if (!detailedMode_) {
    if (zoomPreset_ >= 2 && zoomPreset_ <= 4) return zoomPreset_ - 2;
    return 1;
  }

  if (wholeZoomScaleValue <= 0.25f) {
    return zoomPreset_ >= 0 && zoomPreset_ <= 5 ? zoomPreset_ : 3;
  }

  static const int order[6] = {1, 0, 2, 3, 4, 5};
  for (int position = 0; position < 6; ++position) {
    if (order[position] == zoomPreset_) return position;
  }
  return 3;
}

void Camera::resetPanPixelRemainder() {
  panPixelRemainderRight_ = 0.0f;
  panPixelRemainderVertical_ = 0.0f;
}

void Camera::rotateClockwise() {
  yawStep_ = (yawStep_ + (detailedYawMode_ ? 1 : 2)) & 7;
  resetPanPixelRemainder();
}

void Camera::rotateCounterClockwise() {
  yawStep_ = (yawStep_ + (detailedYawMode_ ? 7 : 6)) & 7;
  resetPanPixelRemainder();
}

void Camera::resetYaw() {
  yawStep_ = 0;
  resetPanPixelRemainder();
}

void Camera::setDetailedYawMode(bool enabled) {
  detailedYawMode_ = enabled;
  if (!detailedYawMode_ && (yawStep_ & 1)) {
    yawStep_ = (yawStep_ + 1) & 6;
  }
  resetPanPixelRemainder();
}

void Camera::stepZoom(int delta, int frameWidth, int frameHeight, const WorldBounds& bounds) {
  const float wholeScale = wholeZoomScale(frameWidth, frameHeight, bounds);
  const int position = std::max(
    0,
    std::min(sequenceLength() - 1, sequencePosition(wholeScale) + delta)
  );
  zoomPreset_ = presetAt(position, wholeScale);
  resetPanPixelRemainder();
}

void Camera::resetZoom() {
  zoomPreset_ = 3;
  resetPanPixelRemainder();
}

void Camera::setDetailedMode(bool enabled) {
  detailedMode_ = enabled;
  if (!detailedMode_) {
    if (zoomPreset_ < 2) zoomPreset_ = 2;
    if (zoomPreset_ > 4) zoomPreset_ = 4;
  }
  resetPanPixelRemainder();
}

void Camera::pan(float right, float down, int frameWidth, int frameHeight, const WorldBounds& bounds) {
  if (!canPan(frameWidth, frameHeight, bounds) || frameHeight <= 0) {
    resetPanPixelRemainder();
    return;
  }

  // Orthographic panning is ultimately a screen translation. Accumulate the
  // requested motion in renderer-pixel space and only commit whole pixels.
  // This is visually continuous at animation-frame rates and, crucially,
  // makes consecutive pan frames exact translations of the static ray cache.
  const float height = viewHeight(frameWidth, frameHeight, bounds);
  if (height <= 1e-7f) return;
  const float pixelsPerWorld = static_cast<float>(frameHeight) / height;
  const Vec3 rightAxis = groundRight();
  const Vec3 downAxis = groundDown();
  const Vec3 upAxis = normalise(cross(rightAxis, forward()));
  const float verticalProjection = dot(downAxis, upAxis);
  if (std::fabs(verticalProjection) <= 1e-7f) return;

  const float pendingRight = panPixelRemainderRight_ + right * pixelsPerWorld;
  const float pendingVertical =
    panPixelRemainderVertical_ + down * verticalProjection * pixelsPerWorld;
  const float pixelRight = std::trunc(pendingRight);
  const float pixelVertical = std::trunc(pendingVertical);
  panPixelRemainderRight_ = pendingRight - pixelRight;
  panPixelRemainderVertical_ = pendingVertical - pixelVertical;

  if (pixelRight == 0.0f && pixelVertical == 0.0f) return;

  const float quantisedRight = pixelRight / pixelsPerWorld;
  const float quantisedDown = pixelVertical / (verticalProjection * pixelsPerWorld);
  const Vec3 delta = rightAxis * quantisedRight + downAxis * quantisedDown;
  const Vec3 candidate{panX_ + delta.x, panY_ + delta.y, 0.0f};
  const float aspect = static_cast<float>(frameWidth) / frameHeight;
  const Vec3 clamped = clampPanToBounds(
    candidate,
    forward(),
    rightAxis,
    downAxis,
    height,
    aspect,
    bounds,
    config_.panLimit
  );

  if (
    std::fabs(clamped.x - candidate.x) > 1e-6f ||
    std::fabs(clamped.y - candidate.y) > 1e-6f
  ) resetPanPixelRemainder();
  panX_ = clamped.x;
  panY_ = clamped.y;
}

void Camera::resetPan() {
  panX_ = 0.0f;
  panY_ = 0.0f;
  resetPanPixelRemainder();
}

} // namespace engine
} // namespace isoweb
