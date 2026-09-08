from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"pattern not found in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1))


# IWorld: preview cache revision lets the renderer retain refined tiles while
# invalidating them whenever lower-preview presentation actually changes.
replace_once(
    "src/engine/world/IWorld.hpp",
    "#include <cstddef>\n#include <limits>\n#include <vector>\n",
    "#include <cstddef>\n#include <cstdint>\n#include <limits>\n#include <vector>\n",
)
replace_once(
    "src/engine/world/IWorld.hpp",
    "  virtual float lowerPreviewResolutionScale() const { return 0.25f; }\n  virtual bool sampleLowDetailLowerPreview(const Ray&, Vec3&) const { return false; }\n",
    "  virtual float lowerPreviewResolutionScale() const { return 0.25f; }\n  virtual std::uint64_t lowDetailPreviewRevision() const { return 0; }\n  virtual bool sampleLowDetailLowerPreview(const Ray&, Vec3&) const { return false; }\n",
)

# World: expose and maintain a preview revision. The signature follows lower
# Characters/markers so moving preview entities use the immediate coarse layer;
# once stable, the progressive cache can refine and persist.
replace_once(
    "src/engine/world/World.hpp",
    "#include <cstddef>\n#include <limits>\n",
    "#include <cstddef>\n#include <cstdint>\n#include <limits>\n",
)
replace_once(
    "src/engine/world/World.hpp",
    "  float lowerPreviewResolutionScale() const override { return lowerPreviewResolutionScale_; }\n  bool sampleLowDetailLowerPreview(const Ray& ray, Vec3& colour) const override;\n",
    "  float lowerPreviewResolutionScale() const override { return lowerPreviewResolutionScale_; }\n  std::uint64_t lowDetailPreviewRevision() const override { return lowDetailPreviewRevision_; }\n  bool sampleLowDetailLowerPreview(const Ray& ray, Vec3& colour) const override;\n",
)
replace_once(
    "src/engine/world/World.hpp",
    "  mutable bool runtimeSpritePlaneValid_ = false;\n  mutable bool runtimeRenderCachePrepared_ = false;\n",
    "  mutable bool runtimeSpritePlaneValid_ = false;\n  mutable bool runtimeRenderCachePrepared_ = false;\n  mutable std::uint64_t lowDetailPreviewRevision_ = 1;\n  mutable std::uint64_t lowDetailPreviewSignature_ = 0;\n",
)

replace_once(
    "src/engine/world/World.cpp",
    "#include <algorithm>\n#include <cmath>\n#include <cstdlib>\n",
    "#include <algorithm>\n#include <cmath>\n#include <cstdint>\n#include <cstdlib>\n",
)
replace_once(
    "src/engine/world/World.cpp",
    "float distanceSquared(const Vec3& a, const Vec3& b) {\n  const Vec3 delta = a - b;\n  return dot(delta, delta);\n}\n",
    "float distanceSquared(const Vec3& a, const Vec3& b) {\n  const Vec3 delta = a - b;\n  return dot(delta, delta);\n}\n\nvoid hashPreviewValue(std::uint64_t& hash, std::uint64_t value) {\n  hash ^= value;\n  hash *= 1099511628211ULL;\n}\n\nvoid hashPreviewFloat(std::uint64_t& hash, float value) {\n  const long long quantised = static_cast<long long>(std::llround(value * 4096.0f));\n  hashPreviewValue(hash, static_cast<std::uint64_t>(quantised));\n}\n",
)
replace_once(
    "src/engine/world/World.cpp",
    "  levelLookup_[id] = index;\n  runtimeRenderCachePrepared_ = false;\n  return true;\n",
    "  levelLookup_[id] = index;\n  ++lowDetailPreviewRevision_;\n  runtimeRenderCachePrepared_ = false;\n  return true;\n",
)
replace_once(
    "src/engine/world/World.cpp",
    "void World::setLowerLevelPreviewDepth(std::size_t depth) {\n  const std::size_t maximum = levels_.empty() ? 0 : levels_.size() - 1;\n  lowerLevelPreviewDepth_ = std::min(depth, maximum);\n  updateLevelResidency();\n",
    "void World::setLowerLevelPreviewDepth(std::size_t depth) {\n  const std::size_t maximum = levels_.empty() ? 0 : levels_.size() - 1;\n  const std::size_t nextDepth = std::min(depth, maximum);\n  if (nextDepth != lowerLevelPreviewDepth_) ++lowDetailPreviewRevision_;\n  lowerLevelPreviewDepth_ = nextDepth;\n  updateLevelResidency();\n",
)
replace_once(
    "src/engine/world/World.cpp",
    "void World::setLowerPreviewResolutionScale(float scale) {\n  lowerPreviewResolutionScale_ = std::max(0.0625f, std::min(0.5f, scale));\n}\n",
    "void World::setLowerPreviewResolutionScale(float scale) {\n  const float nextScale = std::max(0.0625f, std::min(0.5f, scale));\n  if (std::fabs(nextScale - lowerPreviewResolutionScale_) > 1e-6f) ++lowDetailPreviewRevision_;\n  lowerPreviewResolutionScale_ = nextScale;\n}\n",
)
replace_once(
    "src/engine/world/World.cpp",
    "  if (index >= levelViewOrigins_.size()) return false;\n  levelViewOrigins_[index] = origin;\n  updateVisibleBounds();\n",
    "  if (index >= levelViewOrigins_.size()) return false;\n  const Vec3 previous = levelViewOrigins_[index];\n  levelViewOrigins_[index] = origin;\n  if (distanceSquared(previous, origin) > 1e-12f) ++lowDetailPreviewRevision_;\n  updateVisibleBounds();\n",
)
replace_once(
    "src/engine/world/World.cpp",
    "    runtimeRenderEntries_.push_back(std::move(entry));\n  }\n\n  runtimeSampleScratch_.clear();\n",
    "    runtimeRenderEntries_.push_back(std::move(entry));\n  }\n\n  // Track only data that can change the cheap lower-preview pixels. Moving\n  // Characters and flashing destinations deliberately advance this revision,\n  // keeping them on the immediate coarse path. When they stop changing, the\n  // progressive tile cache becomes stable and may refine asynchronously.\n  std::uint64_t previewSignature = 1469598103934665603ULL;\n  for (const LowDetailPreviewCharacter& preview : lowDetailPreviewCharacters_) {\n    hashPreviewValue(previewSignature, preview.levelIndex);\n    hashPreviewFloat(previewSignature, preview.position.x);\n    hashPreviewFloat(previewSignature, preview.position.y);\n    hashPreviewFloat(previewSignature, preview.position.z);\n    hashPreviewFloat(previewSignature, preview.forward.x);\n    hashPreviewFloat(previewSignature, preview.forward.y);\n    hashPreviewValue(previewSignature, preview.selected ? 1 : 0);\n  }\n  for (const LowDetailPreviewMarker& marker : lowDetailPreviewMarkers_) {\n    hashPreviewValue(previewSignature, marker.levelIndex);\n    hashPreviewFloat(previewSignature, marker.position.x);\n    hashPreviewFloat(previewSignature, marker.position.y);\n    hashPreviewFloat(previewSignature, marker.elapsedSeconds);\n  }\n  if (characterSystem_) {\n    const SelectionStyle style = characterSystem_->selectionStyle();\n    hashPreviewFloat(previewSignature, style.tint.x);\n    hashPreviewFloat(previewSignature, style.tint.y);\n    hashPreviewFloat(previewSignature, style.tint.z);\n    hashPreviewFloat(previewSignature, style.strength);\n  }\n  if (previewSignature != lowDetailPreviewSignature_) {\n    lowDetailPreviewSignature_ = previewSignature;\n    ++lowDetailPreviewRevision_;\n  }\n\n  runtimeSampleScratch_.clear();\n",
)

# Renderer header: progressive normal tiles + tiny immediate coarse fallback.
Path("src/engine/render/Renderer.hpp").write_text(r'''#pragma once

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
''')

# Renderer implementation.
replace_once(
    "src/engine/render/Renderer.cpp",
    "constexpr float RAY_ORIGIN_MARGIN = 4.0f;\n",
    "constexpr float RAY_ORIGIN_MARGIN = 4.0f;\nconstexpr int PREVIEW_TILE_SIZE = 16;\nconstexpr float COARSE_PREVIEW_SCALE = 0.125f;\nconstexpr int MAX_COARSE_PREVIEW_WIDTH = 160;\nconstexpr int MAX_COARSE_PREVIEW_HEIGHT = 90;\nconstexpr unsigned int PREVIEW_IDLE_DELAY_FRAMES = 6;\n",
)
replace_once(
    "src/engine/render/Renderer.cpp",
    "bool Renderer::staticCacheMatchesExceptPan(const StaticCacheKey& key) const {\n  return staticCacheValid_ &&\n    staticCacheKey_.width == key.width &&\n    staticCacheKey_.height == key.height &&\n    staticCacheKey_.level == key.level &&\n    staticCacheKey_.yawStep == key.yawStep &&\n    staticCacheKey_.zoomPreset == key.zoomPreset &&\n    std::fabs(staticCacheKey_.viewHeight - key.viewHeight) <= 1e-6f;\n}\n",
    "bool Renderer::staticCacheMatchesExceptPan(const StaticCacheKey& key) const {\n  return staticCacheValid_ &&\n    staticCacheKey_.width == key.width &&\n    staticCacheKey_.height == key.height &&\n    staticCacheKey_.level == key.level &&\n    staticCacheKey_.yawStep == key.yawStep &&\n    staticCacheKey_.zoomPreset == key.zoomPreset &&\n    std::fabs(staticCacheKey_.viewHeight - key.viewHeight) <= 1e-6f;\n}\n\nbool Renderer::previewCacheMatches(const PreviewCacheKey& key) const {\n  return previewCacheValid_ &&\n    previewCacheKey_.width == key.width &&\n    previewCacheKey_.height == key.height &&\n    previewCacheKey_.level == key.level &&\n    previewCacheKey_.yawStep == key.yawStep &&\n    previewCacheKey_.zoomPreset == key.zoomPreset &&\n    previewCacheKey_.panX == key.panX &&\n    previewCacheKey_.panY == key.panY &&\n    previewCacheKey_.viewHeight == key.viewHeight &&\n    previewCacheKey_.revision == key.revision;\n}\n",
)

old_preview_block = '''  // Decorative lower floors are rendered once per frame into a deliberately
  // small buffer. This is one analytic sample per preview texel, no supersampling.
  previewWidth_ = 0;
  previewHeight_ = 0;
  if (world_.supportsLowDetailLowerPreview()) {
    const float previewScale = std::max(0.0625f, std::min(0.5f, world_.lowerPreviewResolutionScale()));
    previewWidth_ = std::max(1, std::min(320, static_cast<int>(std::ceil(frameWidth_ * previewScale))));
    previewHeight_ = std::max(1, std::min(180, static_cast<int>(std::ceil(frameHeight_ * previewScale))));
    const std::size_t required = static_cast<std::size_t>(previewWidth_) * previewHeight_;
    if (previewSamples_.size() != required) previewSamples_.resize(required);

    const float previewStepX = width / static_cast<float>(previewWidth_);
    const float previewStepY = height / static_cast<float>(previewHeight_);
    const float previewOriginDistance = rayOriginDistance(bounds, forward);
    const Vec3 previewCorner =
      focus - forward * previewOriginDistance - right * (width * 0.5f) + up * (height * 0.5f);
    const Vec3 previewRightStep = right * previewStepX;
    const Vec3 previewDownStep = up * (-previewStepY);
    Vec3 previewRow = previewCorner + previewRightStep * 0.5f + previewDownStep * 0.5f;
    for (int py = 0; py < previewHeight_; ++py) {
      Vec3 previewOrigin = previewRow;
      for (int px = 0; px < previewWidth_; ++px) {
        PreviewSample& sample = previewSamples_[static_cast<std::size_t>(py) * previewWidth_ + px];
        sample.found = world_.sampleLowDetailLowerPreview({previewOrigin, forward}, sample.colour);
        previewOrigin = previewOrigin + previewRightStep;
      }
      previewRow = previewRow + previewDownStep;
    }
  }
'''
new_preview_block = '''  // Lower previews are progressive and demand-driven. The normal 0.25x cache
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
    const float previewScale = std::max(0.0625f, std::min(0.5f, world_.lowerPreviewResolutionScale()));
    previewWidth_ = std::max(1, std::min(320, static_cast<int>(std::ceil(frameWidth_ * previewScale))));
    previewHeight_ = std::max(1, std::min(180, static_cast<int>(std::ceil(frameHeight_ * previewScale))));
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

    coarsePreviewWidth_ = std::max(
      1,
      std::min(MAX_COARSE_PREVIEW_WIDTH, static_cast<int>(std::ceil(frameWidth_ * COARSE_PREVIEW_SCALE)))
    );
    coarsePreviewHeight_ = std::max(
      1,
      std::min(MAX_COARSE_PREVIEW_HEIGHT, static_cast<int>(std::ceil(frameHeight_ * COARSE_PREVIEW_SCALE)))
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
'''
replace_once("src/engine/render/Renderer.cpp", old_preview_block, new_preview_block)

old_loop_setup = '''    const int previewY = previewHeight_ > 0
      ? std::min(previewHeight_ - 1, y * previewHeight_ / frameHeight_)
      : 0;
    std::uint8_t* frameRow = reinterpret_cast<std::uint8_t*>(
'''
new_loop_setup = '''    const int previewY = previewHeight_ > 0
      ? std::min(previewHeight_ - 1, y * previewHeight_ / frameHeight_)
      : 0;
    const int coarsePreviewY = coarsePreviewHeight_ > 0
      ? std::min(coarsePreviewHeight_ - 1, y * coarsePreviewHeight_ / frameHeight_)
      : 0;
    std::uint8_t* frameRow = reinterpret_cast<std::uint8_t*>(
'''
replace_once("src/engine/render/Renderer.cpp", old_loop_setup, new_loop_setup)

old_pixel_setup = '''    for (int x = 0; x < frameWidth_; ++x, ++pixelIndex) {
      const PreviewSample* previewForPixel = nullptr;
      if (previewWidth_ > 0 && previewHeight_ > 0) {
        const int previewX = std::min(previewWidth_ - 1, x * previewWidth_ / frameWidth_);
        previewForPixel = &previewSamples_[
          static_cast<std::size_t>(previewY) * previewWidth_ + previewX
        ];
      }

      Vec3 colour;
'''
new_pixel_setup = '''    for (int x = 0; x < frameWidth_; ++x, ++pixelIndex) {
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
'''
replace_once("src/engine/render/Renderer.cpp", old_pixel_setup, new_pixel_setup)

old_composite = '''        if (
          environmentDistance >= NO_HIT_DISTANCE &&
          previewForPixel &&
          previewForPixel->found
        ) {
          environmentColour = previewForPixel->colour;
        }
'''
new_composite = '''        if (environmentDistance >= NO_HIT_DISTANCE && previewForPixel) {
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
'''
replace_once("src/engine/render/Renderer.cpp", old_composite, new_composite)

# Progressive refinement methods are inserted before render().
replace_once(
    "src/engine/render/Renderer.cpp",
    "void Renderer::render() {\n",
    '''bool Renderer::previewNeedsRefinement() const {
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
''',
)

# DemoApplication: background refinement redraw is presentation-only and reuses
# the exact active-level static cache.
replace_once(
    "src/demo/DemoApplication.hpp",
    "  void render();\n  void tick(float deltaSeconds);\n",
    "  void render();\n  bool refinePreview(std::size_t maxTiles);\n  bool previewNeedsRefinement() const { return renderer_.previewNeedsRefinement(); }\n  void tick(float deltaSeconds);\n",
)
replace_once(
    "src/demo/DemoApplication.hpp",
    "  std::size_t staticCacheShiftCount() const { return renderer_.staticCacheShiftCount(); }\n",
    "  std::size_t staticCacheShiftCount() const { return renderer_.staticCacheShiftCount(); }\n  std::size_t previewCoarseSampleCount() const { return renderer_.previewCoarseSampleCount(); }\n  std::size_t previewRefinedSampleCount() const { return renderer_.previewRefinedSampleCount(); }\n  std::size_t previewDemandedTexelCount() const { return renderer_.previewDemandedTexelCount(); }\n  std::size_t previewPotentialTexelCount() const { return renderer_.previewPotentialTexelCount(); }\n",
)
replace_once(
    "src/demo/DemoApplication.cpp",
    "void DemoApplication::render() {\n  redraw();\n}\n\nvoid DemoApplication::tick(float deltaSeconds) {\n",
    "void DemoApplication::render() {\n  redraw();\n}\n\nbool DemoApplication::refinePreview(std::size_t maxTiles) {\n  if (!renderer_.refinePreview(maxTiles)) return false;\n  redraw(false);\n  return true;\n}\n\nvoid DemoApplication::tick(float deltaSeconds) {\n",
)

# WASM exports and browser loop: cooperative asynchronous refinement. This is
# real scheduling across animation frames, not a misleading Promise around CPU
# work on the same call stack.
replace_once(
    "src/wasm/exports.cpp",
    "extern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_needs_tick() {\n  return application().needsTick() ? 1 : 0;\n}\n",
    "extern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_needs_tick() {\n  return application().needsTick() ? 1 : 0;\n}\n\nextern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_preview_needs_refinement() {\n  return application().previewNeedsRefinement() ? 1 : 0;\n}\n\nextern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_refine_preview(int maxTiles) {\n  const std::size_t budget = static_cast<std::size_t>(std::max(0, maxTiles));\n  return application().refinePreview(budget) ? 1 : 0;\n}\n",
)
replace_once(
    "src/wasm/exports.cpp",
    "extern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_static_cache_build_count() {\n  return static_cast<int>(application().staticCacheBuildCount());\n}\n",
    "extern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_static_cache_build_count() {\n  return static_cast<int>(application().staticCacheBuildCount());\n}\n\nextern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_preview_coarse_sample_count() {\n  return static_cast<int>(application().previewCoarseSampleCount());\n}\n\nextern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_preview_refined_sample_count() {\n  return static_cast<int>(application().previewRefinedSampleCount());\n}\n\nextern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_preview_demanded_texel_count() {\n  return static_cast<int>(application().previewDemandedTexelCount());\n}\n\nextern \"C\" EMSCRIPTEN_KEEPALIVE int isoweb_preview_potential_texel_count() {\n  return static_cast<int>(application().previewPotentialTexelCount());\n}\n",
)
replace_once(
    "src/wasm/exports.cpp",
    "#include <cstdint>\n#include <limits>\n",
    "#include <algorithm>\n#include <cstdint>\n#include <limits>\n",
)
replace_once(
    "web/src/runtime.ts",
    "  _isoweb_needs_tick(): number;\n",
    "  _isoweb_needs_tick(): number;\n  _isoweb_preview_needs_refinement(): number;\n  _isoweb_refine_preview(maxTiles: number): number;\n",
)
replace_once(
    "web/src/runtime.ts",
    "  _isoweb_static_cache_build_count(): number;\n",
    "  _isoweb_static_cache_build_count(): number;\n  _isoweb_preview_coarse_sample_count(): number;\n  _isoweb_preview_refined_sample_count(): number;\n  _isoweb_preview_demanded_texel_count(): number;\n  _isoweb_preview_potential_texel_count(): number;\n",
)
replace_once(
    "web/src/App.ts",
    "      if (this.module._isoweb_needs_tick()) this.module._isoweb_tick(deltaSeconds);\n      requestAnimationFrame(animate);\n",
    "      if (this.module._isoweb_needs_tick()) {\n        this.module._isoweb_tick(deltaSeconds);\n      } else if (this.module._isoweb_preview_needs_refinement()) {\n        // One small tile per idle animation frame. Camera input renders its\n        // coarse fallback immediately; refinement waits several frames after\n        // the last camera change, so panning never blocks on preview loading.\n        this.module._isoweb_refine_preview(1);\n      }\n      requestAnimationFrame(animate);\n",
)

# Export bridge.
p = Path("scripts/build-wasm.sh")
text = p.read_text()
text = text.replace(
    '"_isoweb_render","_isoweb_tick","_isoweb_needs_tick",',
    '"_isoweb_render","_isoweb_tick","_isoweb_needs_tick","_isoweb_preview_needs_refinement","_isoweb_refine_preview",',
    1,
)
text = text.replace(
    '"_isoweb_default_level_index","_isoweb_static_cache_build_count",',
    '"_isoweb_default_level_index","_isoweb_static_cache_build_count","_isoweb_preview_coarse_sample_count","_isoweb_preview_refined_sample_count","_isoweb_preview_demanded_texel_count","_isoweb_preview_potential_texel_count",',
    1,
)
p.write_text(text)

# Browser regression specifically checks the progressive contract: immediate
# work is coarse, refinement is deferred, and only visible/exposed texels are
# demanded. It also verifies panning does not synchronously run normal preview
# refinement.
Path("scripts/progressive-preview-smoke.ts").write_text(r'''import { createRequire } from 'node:module';
import { extname, normalize } from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require('../vendor/pages/node_modules/playwright');

const mimeTypes: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json',
  '.webp': 'image/webp',
  '.wasm': 'application/wasm'
};

const server = Bun.serve({
  port: 0,
  async fetch(request) {
    const url = new URL(request.url);
    let pathname = decodeURIComponent(url.pathname);
    if (pathname === '/') pathname = '/index.html';
    if (pathname === '/favicon.ico') return new Response(null, { status: 204 });
    const relative = normalize(pathname).replace(/^[/\\]+/, '');
    if (relative.startsWith('..')) return new Response('Forbidden', { status: 403 });
    const file = Bun.file(`site/${relative}`);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });
    return new Response(file, { headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' } });
  }
});

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 720, height: 720 } });

try {
  await page.goto(`http://127.0.0.1:${server.port}/`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  const initial = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_level_up(); // middle -> upper, exposing both lower previews
    return {
      potential: module._isoweb_preview_potential_texel_count(),
      demanded: module._isoweb_preview_demanded_texel_count(),
      coarse: module._isoweb_preview_coarse_sample_count(),
      refined: module._isoweb_preview_refined_sample_count(),
      needs: module._isoweb_preview_needs_refinement()
    };
  });

  if (!(initial.potential > 0)) throw new Error(`No lower-preview buffer on upper level: ${JSON.stringify(initial)}`);
  if (!(initial.demanded > 0 && initial.demanded < initial.potential)) {
    throw new Error(`Preview demand was not visibility-culled: ${JSON.stringify(initial)}`);
  }
  if (!(initial.coarse > 0 && initial.coarse < initial.potential)) {
    throw new Error(`Immediate preview did not stay coarse/lazy: ${JSON.stringify(initial)}`);
  }
  if (initial.needs !== 1) throw new Error(`Visible preview had no deferred refinement work: ${JSON.stringify(initial)}`);

  // The first calls are intentionally suppressed by the idle delay. This is
  // what prevents panning/zooming from turning newly exposed preview tiles into
  // a synchronous frame-time spike.
  const idleGate = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const before = module._isoweb_preview_refined_sample_count();
    const results: number[] = [];
    for (let i = 0; i < 6; ++i) results.push(module._isoweb_refine_preview(1));
    const afterGate = module._isoweb_preview_refined_sample_count();
    const firstRefine = module._isoweb_refine_preview(1);
    const afterRefine = module._isoweb_preview_refined_sample_count();
    return { before, results, afterGate, firstRefine, afterRefine };
  });
  if (idleGate.results.some(value => value !== 0) || idleGate.afterGate !== idleGate.before) {
    throw new Error(`Preview refined during the camera idle guard: ${JSON.stringify(idleGate)}`);
  }
  if (!(idleGate.firstRefine === 1 && idleGate.afterRefine > idleGate.afterGate)) {
    throw new Error(`Preview did not refine incrementally after becoming idle: ${JSON.stringify(idleGate)}`);
  }

  const panState = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const refinedBefore = module._isoweb_preview_refined_sample_count();
    module._isoweb_pan(28, 0);
    const refinedAfterPan = module._isoweb_preview_refined_sample_count();
    const coarseAfterPan = module._isoweb_preview_coarse_sample_count();
    const firstBackgroundAttempt = module._isoweb_refine_preview(1);
    const refinedAfterAttempt = module._isoweb_preview_refined_sample_count();
    return {
      refinedBefore,
      refinedAfterPan,
      coarseAfterPan,
      firstBackgroundAttempt,
      refinedAfterAttempt,
      demanded: module._isoweb_preview_demanded_texel_count(),
      potential: module._isoweb_preview_potential_texel_count()
    };
  });

  if (panState.refinedAfterPan !== panState.refinedBefore) {
    throw new Error(`Panning synchronously refined preview tiles: ${JSON.stringify(panState)}`);
  }
  if (!(panState.coarseAfterPan > 0)) {
    throw new Error(`Panning exposed no immediate coarse fallback: ${JSON.stringify(panState)}`);
  }
  if (panState.firstBackgroundAttempt !== 0 || panState.refinedAfterAttempt !== panState.refinedAfterPan) {
    throw new Error(`Panning did not re-arm the asynchronous idle guard: ${JSON.stringify(panState)}`);
  }
  if (!(panState.demanded > 0 && panState.demanded < panState.potential)) {
    throw new Error(`Panned preview stopped culling permanently hidden/off-view texels: ${JSON.stringify(panState)}`);
  }

  console.log('Progressive lower-preview smoke test passed.');
} finally {
  await browser.close();
  server.stop(true);
}
''')

# package + CI
p = Path("package.json")
text = p.read_text()
text = text.replace(
    '    "test:runtime": "bun scripts/runtime-smoke.ts",\n',
    '    "test:runtime": "bun scripts/runtime-smoke.ts",\n    "test:progressive-preview": "bun scripts/progressive-preview-smoke.ts",\n',
    1,
)
p.write_text(text)

replace_once(
    ".github/workflows/pages.yml",
    "      - name: Smoke-test WASM browser boot\n        run: bun run test:runtime\n\n      - name: Exercise centre joysticks in Chromium\n",
    "      - name: Smoke-test WASM browser boot\n        run: bun run test:runtime\n\n      - name: Test progressive lower-preview loading\n        run: bun run test:progressive-preview\n\n      - name: Exercise centre joysticks in Chromium\n",
)

print("progressive preview patch applied")
