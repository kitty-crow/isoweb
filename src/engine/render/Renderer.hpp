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
  bool refinePreview(std::size_t maxTiles);
  bool previewNeedsRefinement() const;

  int width() const { return frameWidth_; }
  int height() const { return frameHeight_; }
  const std::vector<std::uint8_t>& rgba() const { return rgba_; }

  bool canPan() const { return frameCanPan_; }
  CameraControlState cameraControlState() const { return frameCameraState_; }
  float viewHeight() const { return frameViewHeight_; }
  float wholeZoomScale() const { return frameWholeZoomScale_; }
  std::size_t staticCacheBuildCount() const { return staticCacheBuildCount_; }
  std::size_t staticCacheShiftCount() const { return staticCacheShiftCount_; }
  std::size_t previewCoarseSampleCount() const { return previewCoarseSampleCount_; }
  std::size_t previewRefinedSampleCount() const { return previewRefinedSampleCount_; }
  std::size_t previewDemandedTexelCount() const { return previewDemandedTexelCount_; }
  std::size_t previewPotentialTexelCount() const {
    return static_cast<std::size_t>(previewWidth_) * static_cast<std::size_t>(previewHeight_);
  }

  Ray rayForPixel(float px, float py) const;
  bool groundPointForPixel(float px, float py, float groundZ, Vec3& point) const;
  bool worldPointToPixel(const Vec3& point, float& px, float& py) const;

private:
  struct StaticSample {
    Vec3 colour;
    float environmentDistance = 0.0f;
  };

  struct PreviewSample {
    Vec3 colour;
    bool found = false;
    bool valid = false;
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

  struct PreviewCacheKey {
    int width = 0;
    int height = 0;
    std::size_t level = 0;
    int yawStep = 0;
    int zoomPreset = 0;
    float panX = 0.0f;
    float panY = 0.0f;
    float viewHeight = 0.0f;
    std::uint64_t revision = 0;
  };

  static std::uint8_t toByte(float value);
  void ensureFrame();
  bool staticCacheMatches(const StaticCacheKey& key) const;
  bool staticCacheMatchesExceptPan(const StaticCacheKey& key) const;
  bool previewCacheMatches(const PreviewCacheKey& key) const;
  bool shiftStaticCacheForPan(
    const StaticCacheKey& key,
    const Vec3& forward,
    const Vec3& right,
    const Vec3& up,
    float viewWidth,
    float viewHeight,
    const WorldBounds& bounds
  );

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
  std::vector<PreviewSample> previewSamples_;
  std::vector<std::uint8_t> previewDemand_;
  std::vector<std::uint8_t> previewTileDemand_;
  std::vector<PreviewSample> coarsePreviewSamples_;
  int previewWidth_ = 0;
  int previewHeight_ = 0;
  int previewTilesX_ = 0;
  int previewTilesY_ = 0;
  int coarsePreviewWidth_ = 0;
  int coarsePreviewHeight_ = 0;
  PreviewCacheKey previewCacheKey_;
  bool previewCacheValid_ = false;
  Vec3 previewRayCorner_;
  Vec3 previewRightStep_;
  Vec3 previewDownStep_;
  Vec3 previewForward_;
  std::size_t previewRefineCursor_ = 0;
  unsigned int previewIdleFrames_ = 0;
  std::size_t previewCoarseSampleCount_ = 0;
  std::size_t previewRefinedSampleCount_ = 0;
  std::size_t previewDemandedTexelCount_ = 0;

  std::vector<Vec3> panBackgroundRows_;
  StaticCacheKey staticCacheKey_;
  bool staticCacheValid_ = false;
  std::size_t staticCacheBuildCount_ = 0;
  std::size_t staticCacheShiftCount_ = 0;

  CameraControlState frameCameraState_;
  bool frameCanPan_ = false;
  float frameViewHeight_ = 6.15f;
  float frameWholeZoomScale_ = 1.0f;
};

} // namespace engine
} // namespace isoweb
