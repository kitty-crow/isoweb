#include "engine/render/Renderer.hpp"

#include <algorithm>
#include <cmath>

namespace isoweb {
namespace engine {

Renderer::Renderer(const IWorld& world, Camera& camera, ControlSprites& controls)
    : world_(world), camera_(camera), controls_(controls) {}

void Renderer::resize(int width, int height) {
  frameWidth_ = std::max(160, std::min(1600, width));
  frameHeight_ = std::max(160, std::min(1600, height));
}

bool Renderer::canPan() const {
  return camera_.canPan(frameWidth_, frameHeight_, world_.bounds());
}

float Renderer::viewHeight() const {
  return camera_.viewHeight(frameWidth_, frameHeight_, world_.bounds());
}

float Renderer::wholeZoomScale() const {
  return camera_.wholeZoomScale(frameWidth_, frameHeight_, world_.bounds());
}

std::uint8_t Renderer::toByte(float value) {
  value = std::pow(std::max(0.0f, std::min(1.0f, value)), 1.0f / 2.2f);
  return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
}

void Renderer::ensureFrame() {
  if (!dsr::image_exists(frame_) || allocatedFrameWidth_ != frameWidth_ || allocatedFrameHeight_ != frameHeight_) {
    frame_ = dsr::image_create_RgbaU8(frameWidth_, frameHeight_, false);
    allocatedFrameWidth_ = frameWidth_;
    allocatedFrameHeight_ = frameHeight_;
  }

  const std::size_t required = static_cast<std::size_t>(frameWidth_) * frameHeight_ * 4;
  if (rgba_.size() != required) rgba_.resize(required);
}

Ray Renderer::makeRay(float px, float py) const {
  const Vec3 forward = camera_.forward();
  const Vec3 right = normalise(cross(forward, {0.0f, 0.0f, 1.0f}));
  const Vec3 up = normalise(cross(right, forward));
  const float aspect = static_cast<float>(frameWidth_) / frameHeight_;
  const float height = viewHeight();
  const float width = height * aspect;
  const float screenX = (px / frameWidth_ - 0.5f) * width;
  const float screenY = (0.5f - py / frameHeight_) * height;

  const WorldBounds& bounds = world_.bounds();
  const Vec3 focus = canPan()
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;

  return {focus - forward * 9.0f + right * screenX + up * screenY, forward};
}

void Renderer::render() {
  ensureFrame();
  const float offsets[2] = {0.25f, 0.75f};

  for (int y = 0; y < frameHeight_; ++y) {
    for (int x = 0; x < frameWidth_; ++x) {
      Vec3 colour;
      for (int sampleY = 0; sampleY < 2; ++sampleY) {
        for (int sampleX = 0; sampleX < 2; ++sampleX) {
          const float px = x + offsets[sampleX];
          const float py = y + offsets[sampleY];
          colour = colour + world_.sample(makeRay(px, py), py / frameHeight_);
        }
      }
      colour = colour * 0.25f;
      dsr::image_writePixel(
        frame_,
        x,
        y,
        {toByte(colour.x), toByte(colour.y), toByte(colour.z), 255}
      );
    }
  }

  LevelControlState levelState;
  levelState.canMoveUp = world_.activeLevelIndex() + 1 < world_.levelCount();
  levelState.canMoveDown = world_.activeLevelIndex() > 0;
  levelState.atDefault = world_.activeLevelIndex() == world_.defaultLevelIndex();
  controls_.draw(frame_, frameWidth_, frameHeight_, canPan(), levelState);

  for (int y = 0; y < frameHeight_; ++y) {
    for (int x = 0; x < frameWidth_; ++x) {
      const auto colour = dsr::image_readPixel_border(frame_, x, y);
      const std::size_t index = static_cast<std::size_t>((y * frameWidth_ + x) * 4);
      rgba_[index] = colour.red;
      rgba_[index + 1] = colour.green;
      rgba_[index + 2] = colour.blue;
      rgba_[index + 3] = 255;
    }
  }
}

} // namespace engine
} // namespace isoweb
