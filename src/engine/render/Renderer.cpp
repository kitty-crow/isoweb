#include "engine/render/Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace isoweb {
namespace engine {
namespace {

constexpr std::size_t MAX_STATIC_CACHE_PIXELS = 600000;

const std::array<float, 255>& gammaThresholds() {
  // The old conversion was round(pow(linear, 1/2.2) * 255). The boundary at
  // which output N becomes selected is therefore ((N - 0.5) / 255)^2.2.
  // Computing these 255 boundaries once removes three pow() calls per pixel
  // while preserving the same 8-bit transfer curve and sample quality.
  static const std::array<float, 255> thresholds = [] {
    std::array<float, 255> values{};
    for (std::size_t output = 1; output <= 255; ++output) {
      const float encodedBoundary =
        (static_cast<float>(output) - 0.5f) * (1.0f / 255.0f);
      values[output - 1] = std::pow(encodedBoundary, 2.2f);
    }
    return values;
  }();
  return thresholds;
}

} // namespace

Renderer::Renderer(const IWorld& world, Camera& camera, ControlSprites& controls)
    : world_(world), camera_(camera), controls_(controls) {}

void Renderer::resize(int width, int height) {
  frameWidth_ = std::max(160, std::min(1600, width));
  frameHeight_ = std::max(160, std::min(1600, height));
}

std::uint8_t Renderer::toByte(float value) {
  if (value <= 0.0f) return 0;
  if (value >= 1.0f) return 255;
  const auto& thresholds = gammaThresholds();
  return static_cast<std::uint8_t>(
    std::upper_bound(thresholds.begin(), thresholds.end(), value) - thresholds.begin()
  );
}

bool Renderer::staticCacheMatches(const StaticCacheKey& key) const {
  return staticCacheValid_ &&
    staticCacheKey_.width == key.width &&
    staticCacheKey_.height == key.height &&
    staticCacheKey_.level == key.level &&
    staticCacheKey_.yawStep == key.yawStep &&
    staticCacheKey_.zoomPreset == key.zoomPreset &&
    staticCacheKey_.panX == key.panX &&
    staticCacheKey_.panY == key.panY &&
    staticCacheKey_.viewHeight == key.viewHeight;
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

Ray Renderer::rayForPixel(float px, float py) const {
  const Vec3 forward = camera_.forward();
  const Vec3 right = normalise(cross(forward, {0.0f, 0.0f, 1.0f}));
  const Vec3 up = normalise(cross(right, forward));
  const float aspect = static_cast<float>(frameWidth_) / frameHeight_;
  const float height = frameViewHeight_;
  const float width = height * aspect;
  const float screenX = (px / frameWidth_ - 0.5f) * width;
  const float screenY = (0.5f - py / frameHeight_) * height;

  const WorldBounds& bounds = world_.bounds();
  const Vec3 focus = frameCanPan_
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;

  return {focus - forward * 9.0f + right * screenX + up * screenY, forward};
}

bool Renderer::groundPointForPixel(float px, float py, float groundZ, Vec3& point) const {
  const Ray ray = rayForPixel(px, py);
  if (std::fabs(ray.direction.z) < 1e-7f) return false;
  const float t = (groundZ - ray.origin.z) / ray.direction.z;
  if (t <= 0.0f) return false;
  point = ray.origin + ray.direction * t;
  return true;
}

bool Renderer::worldPointToPixel(const Vec3& point, float& px, float& py) const {
  const Vec3 forward = camera_.forward();
  const Vec3 right = normalise(cross(forward, {0.0f, 0.0f, 1.0f}));
  const Vec3 up = normalise(cross(right, forward));
  const float aspect = static_cast<float>(frameWidth_) / frameHeight_;
  const float height = frameViewHeight_;
  const float width = height * aspect;
  if (width <= 0.0f || height <= 0.0f) return false;

  const WorldBounds& bounds = world_.bounds();
  const Vec3 focus = frameCanPan_
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;
  const Vec3 delta = point - focus;
  const float screenX = dot(delta, right);
  const float screenY = dot(delta, up);

  px = (screenX / width + 0.5f) * frameWidth_;
  py = (0.5f - screenY / height) * frameHeight_;
  return px >= 0.0f && px <= frameWidth_ && py >= 0.0f && py <= frameHeight_;
}

void Renderer::render() {
  ensureFrame();

  const WorldBounds& bounds = world_.bounds();
  const Vec3 forward = camera_.forward();
  const Vec3 right = normalise(cross(forward, {0.0f, 0.0f, 1.0f}));
  const Vec3 up = normalise(cross(right, forward));
  const float aspect = static_cast<float>(frameWidth_) / frameHeight_;
  const float height = camera_.viewHeight(frameWidth_, frameHeight_, bounds);
  const float width = height * aspect;
  const bool panEnabled = camera_.canPan(frameWidth_, frameHeight_, bounds);
  const Vec3 focus = panEnabled
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;

  // These values are needed again immediately by the browser presenter. Keep
  // the exact metrics from this render rather than rescanning bounds and
  // rebuilding camera state after the expensive frame has already completed.
  frameViewHeight_ = height;
  frameCanPan_ = panEnabled;
  frameWholeZoomScale_ = camera_.wholeZoomScale(frameWidth_, frameHeight_, bounds);

  world_.prepareRenderFrame(forward);

  const std::size_t pixelCount =
    static_cast<std::size_t>(frameWidth_) * static_cast<std::size_t>(frameHeight_);
  const bool useStaticCache =
    world_.supportsStaticSampleCache() && pixelCount <= MAX_STATIC_CACHE_PIXELS;
  StaticCacheKey nextStaticKey;
  nextStaticKey.width = frameWidth_;
  nextStaticKey.height = frameHeight_;
  nextStaticKey.level = world_.activeLevelIndex();
  nextStaticKey.yawStep = camera_.yawStep();
  nextStaticKey.zoomPreset = camera_.zoomPreset();
  nextStaticKey.panX = camera_.panX();
  nextStaticKey.panY = camera_.panY();
  nextStaticKey.viewHeight = height;

  const bool rebuildStaticCache = useStaticCache && !staticCacheMatches(nextStaticKey);
  if (useStaticCache) {
    const std::size_t sampleCount = pixelCount * 4;
    if (staticSamples_.size() != sampleCount) staticSamples_.resize(sampleCount);
  } else {
    staticCacheValid_ = false;
  }

  const float inverseFrameWidth = 1.0f / static_cast<float>(frameWidth_);
  const float inverseFrameHeight = 1.0f / static_cast<float>(frameHeight_);
  const Vec3 rightStep = right * (width * inverseFrameWidth);
  const Vec3 downStep = up * (-height * inverseFrameHeight);
  const Vec3 cornerOrigin = focus - forward * 9.0f - right * (width * 0.5f) + up * (height * 0.5f);

  const Vec3 sampleOffsets[4] = {
    rightStep * 0.25f + downStep * 0.25f,
    rightStep * 0.75f + downStep * 0.25f,
    rightStep * 0.25f + downStep * 0.75f,
    rightStep * 0.75f + downStep * 0.75f
  };

  Vec3 rowOrigin = cornerOrigin;
  std::size_t pixelIndex = 0;
  for (int y = 0; y < frameHeight_; ++y) {
    Vec3 pixelOrigin = rowOrigin;
    const float backgroundY[2] = {
      (static_cast<float>(y) + 0.25f) * inverseFrameHeight,
      (static_cast<float>(y) + 0.75f) * inverseFrameHeight
    };

    for (int x = 0; x < frameWidth_; ++x, ++pixelIndex) {
      Vec3 colour;
      for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
        const Ray ray{pixelOrigin + sampleOffsets[sampleIndex], forward};
        const float sampleBackgroundY = backgroundY[sampleIndex >> 1];

        if (useStaticCache) {
          StaticSample& staticSample = staticSamples_[pixelIndex * 4 + sampleIndex];
          if (rebuildStaticCache) {
            staticSample.colour = world_.sampleEnvironment(
              ray,
              sampleBackgroundY,
              staticSample.environmentDistance
            );
          }
          colour = colour + world_.compositeRuntime(
            ray,
            staticSample.colour,
            staticSample.environmentDistance
          );
        } else {
          colour = colour + world_.sample(ray, sampleBackgroundY);
        }
      }
      colour = colour * 0.25f;

      dsr::image_writePixel(
        frame_,
        x,
        y,
        {toByte(colour.x), toByte(colour.y), toByte(colour.z), 255}
      );
      pixelOrigin = pixelOrigin + rightStep;
    }
    rowOrigin = rowOrigin + downStep;
  }

  if (rebuildStaticCache) {
    staticCacheKey_ = nextStaticKey;
    staticCacheValid_ = true;
  }

  frameCameraState_ = camera_.controlState(frameWidth_, frameHeight_, bounds);
  LevelControlState levelState;
  levelState.canMoveUp = world_.activeLevelIndex() + 1 < world_.levelCount();
  levelState.canMoveDown = world_.activeLevelIndex() > 0;
  levelState.atDefault = world_.activeLevelIndex() == world_.defaultLevelIndex();
  controls_.draw(frame_, frameWidth_, frameHeight_, frameCameraState_, levelState);

  // OrderedImageRgbaU8 guarantees RGBA byte order on every platform. Copy
  // whole visible rows from DFPSR's padded image buffer instead of performing
  // width*height safe pixel reads, unpacking, and four channel assignments.
  const std::size_t rowBytes = static_cast<std::size_t>(frameWidth_) * 4;
  for (int y = 0; y < frameHeight_; ++y) {
    const std::uint8_t* source = reinterpret_cast<const std::uint8_t*>(
      &dsr::image_accessPixel(frame_, 0, y)
    );
    std::memcpy(rgba_.data() + static_cast<std::size_t>(y) * rowBytes, source, rowBytes);
  }
}

} // namespace engine
} // namespace isoweb
