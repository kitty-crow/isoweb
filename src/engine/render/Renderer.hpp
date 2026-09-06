#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "DFPSR/api/imageAPI.h"
#include "engine/camera/Camera.hpp"
#include "engine/ui/ControlSprites.hpp"
#include "engine/world/IWorld.hpp"

namespace isoweb {
namespace engine {

class Renderer {
public:
  Renderer(const IWorld& world, Camera& camera, ControlSprites& controls);

  void resize(int width, int height);
  void render();

  int width() const { return frameWidth_; }
  int height() const { return frameHeight_; }
  const std::vector<std::uint8_t>& rgba() const { return rgba_; }

  bool canPan() const;
  CameraControlState cameraControlState() const;
  float viewHeight() const;
  float wholeZoomScale() const;

  Ray rayForPixel(float px, float py) const;
  bool groundPointForPixel(float px, float py, float groundZ, Vec3& point) const;
  bool worldPointToPixel(const Vec3& point, float& px, float& py) const;

private:
  struct StaticSample {
    Vec3 colour;
    float environmentDistance = 0.0f;
  };

  struct StaticCacheKey {
    int width = 0;
    int height = 0;
    std::size_t level = 0;
    int yawStep = 0;
    int zoomPreset = 0;
    float panX = 0.0f;
    float panY = 0.0f;
    float viewHeight = 0.0f;
  };

  static std::uint8_t toByte(float value);
  void ensureFrame();
  bool staticCacheMatches(const StaticCacheKey& key) const;

  const IWorld& world_;
  Camera& camera_;
  ControlSprites& controls_;
  int frameWidth_ = 512;
  int frameHeight_ = 288;
  int allocatedFrameWidth_ = 0;
  int allocatedFrameHeight_ = 0;
  dsr::OrderedImageRgbaU8 frame_;
  std::vector<std::uint8_t> rgba_;

  std::vector<StaticSample> staticSamples_;
  StaticCacheKey staticCacheKey_;
  bool staticCacheValid_ = false;
};

} // namespace engine
} // namespace isoweb
