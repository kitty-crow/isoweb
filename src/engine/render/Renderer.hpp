#pragma once

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

private:
  static std::uint8_t toByte(float value);
  void ensureFrame();

  const IWorld& world_;
  Camera& camera_;
  ControlSprites& controls_;
  int frameWidth_ = 512;
  int frameHeight_ = 288;
  int allocatedFrameWidth_ = 0;
  int allocatedFrameHeight_ = 0;
  dsr::OrderedImageRgbaU8 frame_;
  std::vector<std::uint8_t> rgba_;
};

} // namespace engine
} // namespace isoweb
