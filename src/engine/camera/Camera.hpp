#pragma once

#include "engine/math/Vec3.hpp"
#include "engine/world/IWorld.hpp"

namespace isoweb {
namespace engine {

struct CameraConfig {
  float panLimit;
  float baseViewHeight;
  float minimumViewWidth;

  CameraConfig(float panLimitValue, float baseViewHeightValue, float minimumViewWidthValue)
      : panLimit(panLimitValue),
        baseViewHeight(baseViewHeightValue),
        minimumViewWidth(minimumViewWidthValue) {}
};

struct CameraControlState {
  bool canZoomIn = false;
  bool canZoomOut = false;
  bool canResetZoom = false;
  bool canResetYaw = false;
  bool canPanUp = false;
  bool canPanDown = false;
  bool canPanLeft = false;
  bool canPanRight = false;
  bool canResetPan = false;
};

class Camera {
public:
  explicit Camera(const CameraConfig& config);

  Vec3 forward() const;
  Vec3 groundRight() const;
  Vec3 groundDown() const;

  float wholeViewHeight(float aspect, const WorldBounds& bounds) const;
  float baseViewHeight(float aspect) const;
  float wholeZoomScale(int frameWidth, int frameHeight, const WorldBounds& bounds) const;
  float viewHeight(int frameWidth, int frameHeight, const WorldBounds& bounds) const;
  bool canPan(int frameWidth, int frameHeight, const WorldBounds& bounds) const;
  CameraControlState controlState(int frameWidth, int frameHeight, const WorldBounds& bounds) const;
  CameraControlState controlState(
    int frameWidth,
    int frameHeight,
    const WorldBounds& bounds,
    bool panEnabled,
    float wholeZoomScaleValue
  ) const;

  void rotateClockwise();
  void rotateCounterClockwise();
  void resetYaw();
  void setDetailedYawMode(bool enabled);

  void stepZoom(int delta, int frameWidth, int frameHeight, const WorldBounds& bounds);
  void resetZoom();
  void setDetailedMode(bool enabled);

  void pan(float right, float down, int frameWidth, int frameHeight, const WorldBounds& bounds);
  void resetPan();

  int yawStep() const { return yawStep_; }
  int zoomPreset() const { return zoomPreset_; }
  float panX() const { return panX_; }
  float panY() const { return panY_; }
  bool detailedMode() const { return detailedMode_; }
  bool detailedYawMode() const { return detailedYawMode_; }

private:
  float zoomScale() const;
  bool wholeZoom() const;
  int presetAt(int position, float wholeZoomScaleValue) const;
  int sequenceLength() const;
  int sequencePosition(float wholeZoomScaleValue) const;
  void resetPanPixelRemainder();

  CameraConfig config_;
  int yawStep_ = 0;
  int zoomPreset_ = 3;
  float panX_ = 0.0f;
  float panY_ = 0.0f;
  float panPixelRemainderRight_ = 0.0f;
  float panPixelRemainderVertical_ = 0.0f;
  bool detailedMode_ = false;
  bool detailedYawMode_ = false;
};

} // namespace engine
} // namespace isoweb
