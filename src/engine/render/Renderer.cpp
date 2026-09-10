#include "engine/render/Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace isoweb {
namespace engine {
namespace {

constexpr std::size_t MAX_STATIC_CACHE_PIXELS = 600000;
constexpr float PAN_SHIFT_EPSILON = 0.025f;
constexpr float NO_HIT_DISTANCE = 1.0e30f;
constexpr float BASE_RAY_ORIGIN_DISTANCE = 9.0f;
constexpr float RAY_ORIGIN_MARGIN = 4.0f;
constexpr int PREVIEW_TILE_SIZE = 16;
constexpr float COARSE_PREVIEW_SCALE = 0.25f;
constexpr std::size_t MAX_PREVIEW_PIXELS = 1600000;
constexpr std::size_t MAX_COARSE_PREVIEW_PIXELS = 90000;
constexpr unsigned int PREVIEW_IDLE_DELAY_FRAMES = 6;

float rayOriginDistance(const WorldBounds& bounds, const Vec3& forward) {
  float distance = BASE_RAY_ORIGIN_DISTANCE;
  for (const Vec3& point : bounds.points) {
    // A ray begins at focus - forward * distance. Ensure every visible-bound
    // point is comfortably in front of that plane. The margin also covers
    // normal Character height beyond a floor-only edge of the static bounds.
    const float projection = dot(point - bounds.focus, forward);
    distance = std::max(distance, -projection + RAY_ORIGIN_MARGIN);
  }
  return distance;
}

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

bool Renderer::staticCacheMatchesExceptPan(const StaticCacheKey& key) const {
  return staticCacheValid_ &&
    staticCacheKey_.width == key.width &&
    staticCacheKey_.height == key.height &&
    staticCacheKey_.level == key.level &&
    staticCacheKey_.yawStep == key.yawStep &&
    staticCacheKey_.zoomPreset == key.zoomPreset &&
    std::fabs(staticCacheKey_.viewHeight - key.viewHeight) <= 1e-6f;
}

bool Renderer::previewCacheMatches(const PreviewCacheKey& key) const {
  return previewCacheValid_ &&
    previewCacheKey_.width == key.width &&
    previewCacheKey_.height == key.height &&
    previewCacheKey_.level == key.level &&
    previewCacheKey_.yawStep == key.yawStep &&
    previewCacheKey_.zoomPreset == key.zoomPreset &&
    previewCacheKey_.panX == key.panX &&
    previewCacheKey_.panY == key.panY &&
    previewCacheKey_.viewHeight == key.viewHeight &&
    previewCacheKey_.revision == key.revision;
}

bool Renderer::shiftStaticCacheForPan(
  const StaticCacheKey& key,
  const Vec3& forward,
  const Vec3& right,
  const Vec3& up,
  float viewWidth,
  float viewHeight,
  const WorldBounds& bounds
) {
  if (!staticCacheMatchesExceptPan(key) || viewWidth <= 0.0f || viewHeight <= 0.0f) return false;
  if (staticCacheKey_.panX == key.panX && staticCacheKey_.panY == key.panY) return true;

  const Vec3 panDelta(
    key.panX - staticCacheKey_.panX,
    key.panY - staticCacheKey_.panY,
    0.0f
  );
  const float sourceShiftXFloat = dot(panDelta, right) * frameWidth_ / viewWidth;
  const float sourceShiftYFloat = -dot(panDelta, up) * frameHeight_ / viewHeight;
  const int sourceShiftX = static_cast<int>(std::lround(sourceShiftXFloat));
  const int sourceShiftY = static_cast<int>(std::lround(sourceShiftYFloat));

  // Camera::pan accumulates motion in renderer-pixel space, so normal
  // interactive pan deltas land exactly on this grid. Refuse cache shifting if
  // another caller changes pan by an arbitrary sub-pixel amount: correctness
  // wins and the normal full static rebuild is used instead.
  if (
    std::fabs(sourceShiftXFloat - sourceShiftX) > PAN_SHIFT_EPSILON ||
    std::fabs(sourceShiftYFloat - sourceShiftY) > PAN_SHIFT_EPSILON
  ) {
    return false;
  }
  if (sourceShiftX == 0 && sourceShiftY == 0) return false;
  if (std::abs(sourceShiftX) >= frameWidth_ || std::abs(sourceShiftY) >= frameHeight_) return false;

  const int destinationX0 = std::max(0, -sourceShiftX);
  const int destinationX1 = std::min(frameWidth_, frameWidth_ - sourceShiftX);
  const int destinationY0 = std::max(0, -sourceShiftY);
  const int destinationY1 = std::min(frameHeight_, frameHeight_ - sourceShiftY);
  if (destinationX0 >= destinationX1 || destinationY0 >= destinationY1) return false;

  // The environment API receives backgroundY explicitly. For a vertical cache
  // shift, refresh the no-hit gradient once per sub-sample row using a ray that
  // points away from the bounded scene. This avoids retracing every background
  // pixel while keeping the screen-space background anchored exactly.
  if (sourceShiftY != 0) {
    const std::size_t requiredRows = static_cast<std::size_t>(frameHeight_) * 2;
    if (panBackgroundRows_.size() != requiredRows) panBackgroundRows_.resize(requiredRows);
    const Ray missRay{bounds.focus + Vec3(0.0f, 0.0f, 1000000.0f), Vec3(0.0f, 0.0f, 1.0f)};
    const float inverseHeight = 1.0f / static_cast<float>(frameHeight_);
    for (int y = 0; y < frameHeight_; ++y) {
      for (int rowSample = 0; rowSample < 2; ++rowSample) {
        const float offset = rowSample == 0 ? 0.25f : 0.75f;
        float distance = 0.0f;
        const Vec3 colour = world_.sampleEnvironment(
          missRay,
          (static_cast<float>(y) + offset) * inverseHeight,
          distance
        );
        if (distance < NO_HIT_DISTANCE) return false;
        panBackgroundRows_[static_cast<std::size_t>(y) * 2 + rowSample] = colour;
      }
    }
  }

  // Move the overlapping pixel blocks in-place. Each pixel owns four exact
  // supersamples. Row order prevents a vertical move from overwriting a source
  // row that has not been consumed yet; memmove handles horizontal overlap.
  const std::size_t samplesPerPixel = 4;
  const std::size_t rowSampleCount =
    static_cast<std::size_t>(destinationX1 - destinationX0) * samplesPerPixel;
  auto moveRow = [&](int destinationY) {
    const int sourceY = destinationY + sourceShiftY;
    const std::size_t destinationIndex =
      (static_cast<std::size_t>(destinationY) * frameWidth_ + destinationX0) * samplesPerPixel;
    const std::size_t sourceIndex =
      (static_cast<std::size_t>(sourceY) * frameWidth_ + destinationX0 + sourceShiftX) * samplesPerPixel;
    std::memmove(
      staticSamples_.data() + destinationIndex,
      staticSamples_.data() + sourceIndex,
      rowSampleCount * sizeof(StaticSample)
    );
  };

  if (sourceShiftY >= 0) {
    for (int y = destinationY0; y < destinationY1; ++y) moveRow(y);
  } else {
    for (int y = destinationY1 - 1; y >= destinationY0; --y) moveRow(y);
  }

  auto invalidatePixel = [&](int x, int y) {
    StaticSample* samples = staticSamples_.data() +
      (static_cast<std::size_t>(y) * frameWidth_ + x) * samplesPerPixel;
    for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
      samples[sampleIndex].environmentDistance = -1.0f;
    }
  };

  for (int y = 0; y < frameHeight_; ++y) {
    if (y < destinationY0 || y >= destinationY1) {
      for (int x = 0; x < frameWidth_; ++x) invalidatePixel(x, y);
      continue;
    }
    for (int x = 0; x < destinationX0; ++x) invalidatePixel(x, y);
    for (int x = destinationX1; x < frameWidth_; ++x) invalidatePixel(x, y);
  }

  const float forwardShift = dot(panDelta, forward);
  for (int y = destinationY0; y < destinationY1; ++y) {
    for (int x = destinationX0; x < destinationX1; ++x) {
      StaticSample* samples = staticSamples_.data() +
        (static_cast<std::size_t>(y) * frameWidth_ + x) * samplesPerPixel;
      for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
        StaticSample& sample = samples[sampleIndex];
        if (sample.environmentDistance < 0.0f) continue;
        if (sample.environmentDistance < NO_HIT_DISTANCE) {
          sample.environmentDistance = std::max(0.001f, sample.environmentDistance - forwardShift);
        } else if (sourceShiftY != 0) {
          sample.colour = panBackgroundRows_[
            static_cast<std::size_t>(y) * 2 + (sampleIndex >> 1)
          ];
        }
      }
    }
  }

  staticCacheKey_ = key;
  staticCacheValid_ = true;
  ++staticCacheShiftCount_;
  return true;
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
  const Vec3 right = camera_.groundRight();
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

  return {
    focus - forward * rayOriginDistance(bounds, forward) + right * screenX + up * screenY,
    forward
  };
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
  const Vec3 right = camera_.groundRight();
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

bool Renderer::previewNeedsRefinement() const {
  if (!previewCacheValid_ || previewWidth_ <= 0 || previewHeight_ <= 0) return false;
  for (std::size_t tile = 0; tile < previewTileDemand_.size(); ++tile) {
    if (!previewTileDemand_[tile]) continue;
    const int tileX = static_cast<int>(tile % static_cast<std::size_t>(previewTilesX_));
    const int tileY = static_cast<int>(tile / static_cast<std::size_t>(previewTilesX_));
    const int x0 = tileX * PREVIEW_TILE_SIZE;
    const int y0 = tileY * PREVIEW_TILE_SIZE;
    const int x1 = std::min(previewWidth_, x0 + PREVIEW_TILE_SIZE);
    const int y1 = std::min(previewHeight_, y0 + PREVIEW_TILE_SIZE);
    for (int y = y0; y < y1; ++y) {
      for (int x = x0; x < x1; ++x) {
        const std::size_t index = static_cast<std::size_t>(y) * previewWidth_ + x;
        if (previewDemand_[index] && !previewSamples_[index].valid) return true;
      }
    }
  }
  return false;
}

bool Renderer::refinePreview(std::size_t maxTiles) {
  if (!previewNeedsRefinement() || maxTiles == 0) return false;
  if (previewIdleFrames_ > 0) {
    --previewIdleFrames_;
    return false;
  }

  const std::size_t tileCount = previewTileDemand_.size();
  if (tileCount == 0) return false;
  bool changed = false;
  std::size_t refinedTiles = 0;
  std::size_t inspected = 0;

  while (refinedTiles < maxTiles && inspected < tileCount) {
    const std::size_t tile = previewRefineCursor_ % tileCount;
    previewRefineCursor_ = (previewRefineCursor_ + 1) % tileCount;
    ++inspected;
    if (!previewTileDemand_[tile]) continue;

    const int tileX = static_cast<int>(tile % static_cast<std::size_t>(previewTilesX_));
    const int tileY = static_cast<int>(tile / static_cast<std::size_t>(previewTilesX_));
    const int x0 = tileX * PREVIEW_TILE_SIZE;
    const int y0 = tileY * PREVIEW_TILE_SIZE;
    const int x1 = std::min(previewWidth_, x0 + PREVIEW_TILE_SIZE);
    const int y1 = std::min(previewHeight_, y0 + PREVIEW_TILE_SIZE);
    bool tileChanged = false;

    for (int y = y0; y < y1; ++y) {
      Vec3 origin =
        previewRayCorner_ +
        previewRightStep_ * (static_cast<float>(x0) + 0.5f) +
        previewDownStep_ * (static_cast<float>(y) + 0.5f);
      for (int x = x0; x < x1; ++x) {
        const std::size_t index = static_cast<std::size_t>(y) * previewWidth_ + x;
        if (previewDemand_[index] && !previewSamples_[index].valid) {
          PreviewSample& sample = previewSamples_[index];
          sample.found = world_.sampleLowDetailLowerPreview(
            {origin, previewForward_},
            sample.colour
          );
          sample.valid = true;
          ++previewRefinedSampleCount_;
          tileChanged = true;
        }
        origin = origin + previewRightStep_;
      }
    }

    if (tileChanged) {
      changed = true;
      ++refinedTiles;
    }
  }

  return changed;
}

void Renderer::render() {
  ensureFrame();

  const WorldBounds& bounds = world_.bounds();
  const Vec3 forward = camera_.forward();
  const Vec3 right = camera_.groundRight();
  const Vec3 up = normalise(cross(right, forward));
  const float aspect = static_cast<float>(frameWidth_) / frameHeight_;

  // Project the static bounds once. The old path independently asked for
  // viewHeight, canPan, wholeZoomScale and then controlState, each of which
  // could repeat this bounds scan.
  const float wholeHeight = camera_.wholeViewHeight(aspect, bounds);
  const float baseHeight = camera_.baseViewHeight(aspect);
  const float height = camera_.zoomPreset() == 0
    ? wholeHeight
    : camera_.viewHeight(frameWidth_, frameHeight_, bounds);
  const float width = height * aspect;
  const bool panEnabled = height + 0.0001f < wholeHeight;
  const Vec3 focus = panEnabled
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;

  frameViewHeight_ = height;
  frameCanPan_ = panEnabled;
  frameWholeZoomScale_ = baseHeight / wholeHeight;
  frameCameraState_ = camera_.controlState(
    frameWidth_,
    frameHeight_,
    bounds,
    panEnabled,
    frameWholeZoomScale_
  );

  world_.prepareRenderFrame(forward);

  // Lower previews are progressive and demand-driven. The normal high-detail cache
  // is not synchronously rebuilt when camera state changes. Instead the frame
  // records only texels that can actually contribute through active-level
  // holes, displays a tiny immediate coarse fallback, and refines demanded
  // 16x16 tiles later through refinePreview().
  previewWidth_ = 0;
  previewHeight_ = 0;
  coarsePreviewWidth_ = 0;
  coarsePreviewHeight_ = 0;
  previewCoarseSampleCount_ = 0;
  previewDemandedTexelCount_ = 0;
  if (world_.supportsLowDetailLowerPreview()) {
    float previewScale = std::max(0.0625f, std::min(1.0f, world_.lowerPreviewResolutionScale()));
    const double requestedPreviewPixels =
      static_cast<double>(frameWidth_) * static_cast<double>(frameHeight_) *
      static_cast<double>(previewScale) * static_cast<double>(previewScale);
    if (requestedPreviewPixels > static_cast<double>(MAX_PREVIEW_PIXELS)) {
      previewScale *= static_cast<float>(std::sqrt(
        static_cast<double>(MAX_PREVIEW_PIXELS) / requestedPreviewPixels
      ));
    }
    previewWidth_ = std::max(1, static_cast<int>(std::ceil(frameWidth_ * previewScale)));
    previewHeight_ = std::max(1, static_cast<int>(std::ceil(frameHeight_ * previewScale)));
    previewTilesX_ = (previewWidth_ + PREVIEW_TILE_SIZE - 1) / PREVIEW_TILE_SIZE;
    previewTilesY_ = (previewHeight_ + PREVIEW_TILE_SIZE - 1) / PREVIEW_TILE_SIZE;

    PreviewCacheKey nextPreviewKey;
    nextPreviewKey.width = frameWidth_;
    nextPreviewKey.height = frameHeight_;
    nextPreviewKey.level = world_.activeLevelIndex();
    nextPreviewKey.yawStep = camera_.yawStep();
    nextPreviewKey.zoomPreset = camera_.zoomPreset();
    nextPreviewKey.panX = camera_.panX();
    nextPreviewKey.panY = camera_.panY();
    nextPreviewKey.viewHeight = height;
    nextPreviewKey.revision = world_.lowDetailPreviewRevision();

    const std::size_t required = static_cast<std::size_t>(previewWidth_) * previewHeight_;
    const std::size_t tileCount = static_cast<std::size_t>(previewTilesX_) * previewTilesY_;
    if (!previewCacheMatches(nextPreviewKey) || previewSamples_.size() != required) {
      previewSamples_.assign(required, PreviewSample());
      previewDemand_.assign(required, 0);
      previewTileDemand_.assign(tileCount, 0);
      previewCacheKey_ = nextPreviewKey;
      previewCacheValid_ = true;
      previewRefineCursor_ = 0;
      previewIdleFrames_ = PREVIEW_IDLE_DELAY_FRAMES;
    } else {
      std::fill(previewDemand_.begin(), previewDemand_.end(), 0);
      std::fill(previewTileDemand_.begin(), previewTileDemand_.end(), 0);
    }

    const float previewStepX = width / static_cast<float>(previewWidth_);
    const float previewStepY = height / static_cast<float>(previewHeight_);
    const float previewOriginDistance = rayOriginDistance(bounds, forward);
    previewRayCorner_ =
      focus - forward * previewOriginDistance - right * (width * 0.5f) + up * (height * 0.5f);
    previewRightStep_ = right * previewStepX;
    previewDownStep_ = up * (-previewStepY);
    previewForward_ = forward;

    float coarsePreviewScale = COARSE_PREVIEW_SCALE;
    const double requestedCoarsePixels =
      static_cast<double>(frameWidth_) * static_cast<double>(frameHeight_) *
      static_cast<double>(coarsePreviewScale) * static_cast<double>(coarsePreviewScale);
    if (requestedCoarsePixels > static_cast<double>(MAX_COARSE_PREVIEW_PIXELS)) {
      coarsePreviewScale *= static_cast<float>(std::sqrt(
        static_cast<double>(MAX_COARSE_PREVIEW_PIXELS) / requestedCoarsePixels
      ));
    }
    coarsePreviewWidth_ = std::max(
      1,
      static_cast<int>(std::ceil(frameWidth_ * coarsePreviewScale))
    );
    coarsePreviewHeight_ = std::max(
      1,
      static_cast<int>(std::ceil(frameHeight_ * coarsePreviewScale))
    );
    coarsePreviewSamples_.assign(
      static_cast<std::size_t>(coarsePreviewWidth_) * coarsePreviewHeight_,
      PreviewSample()
    );
  } else {
    previewTilesX_ = 0;
    previewTilesY_ = 0;
    previewCacheValid_ = false;
    previewSamples_.clear();
    previewDemand_.clear();
    previewTileDemand_.clear();
    coarsePreviewSamples_.clear();
  }

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

  if (useStaticCache) {
    const std::size_t sampleCount = pixelCount * 4;
    if (staticSamples_.size() != sampleCount) {
      staticSamples_.resize(sampleCount);
      staticCacheValid_ = false;
    }
    if (!staticCacheMatches(nextStaticKey) && staticCacheMatchesExceptPan(nextStaticKey)) {
      shiftStaticCacheForPan(nextStaticKey, forward, right, up, width, height, bounds);
    }
  } else {
    staticCacheValid_ = false;
  }

  const bool rebuildStaticCache = useStaticCache && !staticCacheMatches(nextStaticKey);
  const float inverseFrameWidth = 1.0f / static_cast<float>(frameWidth_);
  const float inverseFrameHeight = 1.0f / static_cast<float>(frameHeight_);
  const Vec3 rightStep = right * (width * inverseFrameWidth);
  const Vec3 downStep = up * (-height * inverseFrameHeight);
  const float originDistance = rayOriginDistance(bounds, forward);
  const Vec3 cornerOrigin =
    focus - forward * originDistance - right * (width * 0.5f) + up * (height * 0.5f);

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
    const int previewY = previewHeight_ > 0
      ? std::min(previewHeight_ - 1, y * previewHeight_ / frameHeight_)
      : 0;
    const int coarsePreviewY = coarsePreviewHeight_ > 0
      ? std::min(coarsePreviewHeight_ - 1, y * coarsePreviewHeight_ / frameHeight_)
      : 0;
    std::uint8_t* frameRow = reinterpret_cast<std::uint8_t*>(
      &dsr::image_accessPixel(frame_, 0, y)
    );

    for (int x = 0; x < frameWidth_; ++x, ++pixelIndex) {
      PreviewSample* previewForPixel = nullptr;
      std::size_t previewIndex = 0;
      int previewX = 0;
      if (previewWidth_ > 0 && previewHeight_ > 0) {
        previewX = std::min(previewWidth_ - 1, x * previewWidth_ / frameWidth_);
        previewIndex = static_cast<std::size_t>(previewY) * previewWidth_ + previewX;
        previewForPixel = &previewSamples_[previewIndex];
      }

      PreviewSample* coarseForPixel = nullptr;
      if (coarsePreviewWidth_ > 0 && coarsePreviewHeight_ > 0) {
        const int coarsePreviewX = std::min(
          coarsePreviewWidth_ - 1,
          x * coarsePreviewWidth_ / frameWidth_
        );
        coarseForPixel = &coarsePreviewSamples_[
          static_cast<std::size_t>(coarsePreviewY) * coarsePreviewWidth_ + coarsePreviewX
        ];
      }

      Vec3 colour;
      for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
        const Ray ray{pixelOrigin + sampleOffsets[sampleIndex], forward};
        const float sampleBackgroundY = backgroundY[sampleIndex >> 1];

        Vec3 environmentColour;
        float environmentDistance = std::numeric_limits<float>::max();
        if (useStaticCache) {
          StaticSample& staticSample = staticSamples_[pixelIndex * 4 + sampleIndex];
          if (rebuildStaticCache || staticSample.environmentDistance < 0.0f) {
            staticSample.colour = world_.sampleEnvironment(
              ray,
              sampleBackgroundY,
              staticSample.environmentDistance
            );
          }
          environmentColour = staticSample.colour;
          environmentDistance = staticSample.environmentDistance;
        } else {
          environmentColour = world_.sampleEnvironment(
            ray,
            sampleBackgroundY,
            environmentDistance
          );
        }

        if (environmentDistance >= NO_HIT_DISTANCE && previewForPixel) {
          const PreviewSample* displayPreview = previewForPixel->valid ? previewForPixel : nullptr;
          if (!displayPreview && coarseForPixel) {
            if (!coarseForPixel->valid) {
              const int coarseIndex = static_cast<int>(coarseForPixel - coarsePreviewSamples_.data());
              const int coarseX = coarseIndex % coarsePreviewWidth_;
              const int coarseY = coarseIndex / coarsePreviewWidth_;
              const float coarseStepX = width / static_cast<float>(coarsePreviewWidth_);
              const float coarseStepY = height / static_cast<float>(coarsePreviewHeight_);
              const Vec3 coarseOrigin =
                focus - forward * originDistance - right * (width * 0.5f) + up * (height * 0.5f) +
                right * (coarseStepX * (static_cast<float>(coarseX) + 0.5f)) +
                up * (-coarseStepY * (static_cast<float>(coarseY) + 0.5f));
              coarseForPixel->found = world_.sampleLowDetailLowerPreview(
                {coarseOrigin, forward},
                coarseForPixel->colour
              );
              coarseForPixel->valid = true;
              ++previewCoarseSampleCount_;
            }
            displayPreview = coarseForPixel->found ? coarseForPixel : nullptr;
          }

          // Only genuinely exposed lower-preview texels enter the background
          // refinement queue. Active-level coverage never gets preview work;
          // sampleLowDetailLowerPreview itself stops at the first lower level,
          // so deeper floors covered by a nearer preview level are never sampled.
          if (displayPreview && displayPreview->found) {
            environmentColour = displayPreview->colour;
            if (!previewDemand_[previewIndex]) {
              previewDemand_[previewIndex] = 1;
              ++previewDemandedTexelCount_;
            }
            const std::size_t tileX = static_cast<std::size_t>(previewX / PREVIEW_TILE_SIZE);
            const std::size_t tileY = static_cast<std::size_t>(previewY / PREVIEW_TILE_SIZE);
            previewTileDemand_[tileY * static_cast<std::size_t>(previewTilesX_) + tileX] = 1;
          }
        }

        colour = colour + world_.compositeRuntime(
          ray,
          environmentColour,
          environmentDistance
        );
      }
      colour = colour * 0.25f;

      const std::size_t offset = static_cast<std::size_t>(x) * 4;
      frameRow[offset] = toByte(colour.x);
      frameRow[offset + 1] = toByte(colour.y);
      frameRow[offset + 2] = toByte(colour.z);
      frameRow[offset + 3] = 255;
      pixelOrigin = pixelOrigin + rightStep;
    }
    rowOrigin = rowOrigin + downStep;
  }

  if (rebuildStaticCache) {
    staticCacheKey_ = nextStaticKey;
    staticCacheValid_ = true;
    ++staticCacheBuildCount_;
  }

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
