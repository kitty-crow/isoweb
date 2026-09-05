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

  void rotateClockwise();
  void rotateCounterClockwise();
  void resetYaw();

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

private:
  static constexpr float PI = 3.14159265358979323846f;

  float yawRadians() const;
  float zoomScale() const;
  bool wholeZoom() const;
  int detailedPresetAt(int position, int frameWidth, int frameHeight, const WorldBounds& bounds) const;
  int presetAt(int position, int frameWidth, int frameHeight, const WorldBounds& bounds) const;
  int sequenceLength() const;
  int sequencePosition(int frameWidth, int frameHeight, const WorldBounds& bounds) const;

  CameraConfig config_;
  int yawStep_ = 0;
  int zoomPreset_ = 3;
  float panX_ = 0.0f;
  float panY_ = 0.0f;
  bool detailedMode_ = false;
};

} // namespace engine
} // namespace isoweb
