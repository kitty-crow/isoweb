from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"expected source block not found in {path}: {old[:160]!r}")
    p.write_text(text.replace(old, new, 1))

# Public world contract for conservative dynamic damage bounds.
replace_once(
    'src/engine/world/IWorld.hpp',
    '''struct WorldBounds {\n  Vec3 focus;\n  std::vector<Vec3> points;\n};\n''',
    '''struct WorldBounds {\n  Vec3 focus;\n  std::vector<Vec3> points;\n};\n\nstruct RuntimeDamageBound {\n  Vec3 centre;\n  float radius = 0.0f;\n};\n'''
)
replace_once(
    'src/engine/world/IWorld.hpp',
    '''  // Parallel frame compositing is opt-in. Worlds returning true promise that''',
    '''  // Conservative world-space bounds covering every pixel that runtime\n  // compositing may alter. Renderers may use these only to skip pixels whose\n  // previous scene value is already exact. Empty means no dynamic damage.\n  virtual void collectRuntimeDamageBounds(std::vector<RuntimeDamageBound>& output) const {\n    output.clear();\n  }\n\n  // Parallel frame compositing is opt-in. Worlds returning true promise that'''
)

replace_once(
    'src/engine/world/World.hpp',
    '''  bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const override;''',
    '''  void collectRuntimeDamageBounds(std::vector<RuntimeDamageBound>& output) const override;\n\n  bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const override;'''
)

# Export entries and feedback footprints as conservative spheres. Sphere
# projection deliberately overdraws corners/depth, which is safe for damage.
replace_once(
    'src/engine/world/World.cpp',
    '''bool World::setLevelLight(\n  const std::string& levelId,''',
    '''void World::collectRuntimeDamageBounds(std::vector<RuntimeDamageBound>& output) const {\n  output.clear();\n  output.reserve(\n    runtimeRenderEntries_.size() +\n    destinationFeedbackMarkers_.size() +\n    lowDetailPreviewMarkers_.size()\n  );\n\n  for (const RuntimeRenderEntry& entry : runtimeRenderEntries_) {\n    RuntimeDamageBound bound;\n    bound.centre = entry.broadphaseCentre;\n    bound.radius = std::sqrt(std::max(0.0f, entry.broadphaseRadiusSquared));\n    output.push_back(bound);\n  }\n\n  const auto markerRadius = [](float minimumX, float maximumX, float minimumY, float maximumY) {\n    const float halfX = std::max(std::fabs(minimumX), std::fabs(maximumX));\n    const float halfY = std::max(std::fabs(minimumY), std::fabs(maximumY));\n    return std::sqrt(halfX * halfX + halfY * halfY) + 0.08f;\n  };\n\n  for (const DestinationFeedbackMarker& marker : destinationFeedbackMarkers_) {\n    RuntimeDamageBound bound;\n    bound.centre = {marker.position.x, marker.position.y, marker.floorZ};\n    bound.radius = markerRadius(\n      marker.minimumX, marker.maximumX, marker.minimumY, marker.maximumY\n    );\n    output.push_back(bound);\n  }\n\n  for (const LowDetailPreviewMarker& marker : lowDetailPreviewMarkers_) {\n    RuntimeDamageBound bound;\n    const Vec3 offset = levelOffsetInActiveView(marker.levelIndex);\n    bound.centre = marker.position + offset;\n    bound.radius = markerRadius(\n      marker.minimumX, marker.maximumX, marker.minimumY, marker.maximumY\n    ) + 0.08f;\n    output.push_back(bound);\n  }\n}\n\nbool World::setLevelLight(\n  const std::string& levelId,'''
)

# Renderer state for previous/current damage and preview visual revisions.
replace_once(
    'src/engine/render/Renderer.hpp',
    '''  struct PreviewSample {\n    Vec3 colour;\n    bool found = false;\n    bool valid = false;\n  };\n''',
    '''  struct PreviewSample {\n    Vec3 colour;\n    bool found = false;\n    bool valid = false;\n  };\n\n  struct DamageRect {\n    int minimumX = 0;\n    int minimumY = 0;\n    int maximumX = 0;\n    int maximumY = 0;\n  };\n'''
)
replace_once(
    'src/engine/render/Renderer.hpp',
    '''  void invalidateWorldCache() {\n    staticCacheValid_ = false;\n    previewCacheValid_ = false;\n    previewIdleFrames_ = 0;\n  }''',
    '''  void invalidateWorldCache() {\n    staticCacheValid_ = false;\n    previewCacheValid_ = false;\n    previewIdleFrames_ = 0;\n    damageHistoryValid_ = false;\n  }'''
)
replace_once(
    'src/engine/render/Renderer.hpp',
    '''  std::size_t previewDemandedTexelCount_ = 0;\n\n  std::vector<Vec3> panBackgroundRows_;''',
    '''  std::size_t previewDemandedTexelCount_ = 0;\n  std::size_t previewVisualRevision_ = 0;\n  std::size_t lastRenderedPreviewRevision_ = 0;\n\n  std::vector<RuntimeDamageBound> runtimeDamageBounds_;\n  std::vector<DamageRect> previousDamageRects_;\n  std::vector<DamageRect> currentDamageRects_;\n  std::vector<int> dirtyMinimumX_;\n  std::vector<int> dirtyMaximumX_;\n  bool damageHistoryValid_ = false;\n\n  std::vector<Vec3> panBackgroundRows_;'''
)

# Resizes invalidate retained scene pixels.
replace_once(
    'src/engine/render/Renderer.cpp',
    '''void Renderer::resize(int width, int height) {\n  frameWidth_ = std::max(1, width);\n  frameHeight_ = std::max(1, height);''',
    '''void Renderer::resize(int width, int height) {\n  frameWidth_ = std::max(1, width);\n  frameHeight_ = std::max(1, height);\n  damageHistoryValid_ = false;'''
)

# Any progressive preview visual change invalidates skipped background pixels.
replace_once(
    'src/engine/render/Renderer.cpp',
    '''  return changed;\n}\n\nvoid Renderer::render() {''',
    '''  if (changed) ++previewVisualRevision_;\n  return changed;\n}\n\nvoid Renderer::render() {'''
)

# Remember whether static/camera state differed before a possible exact pan
# cache shift makes the key match.
replace_once(
    'src/engine/render/Renderer.cpp',
    '''  if (useStaticCache) {\n    const std::size_t sampleCount = pixelCount * 4;''',
    '''  const bool staticFrameChanged =\n    !useStaticCache || !staticCacheMatches(nextStaticKey);\n\n  if (useStaticCache) {\n    const std::size_t sampleCount = pixelCount * 4;'''
)

# Promote preview cache reset information into a render-wide flag.
replace_once(
    'src/engine/render/Renderer.cpp',
    '''  previewCoarseSampleCount_ = 0;\n  previewDemandedTexelCount_ = 0;\n  if (world_.supportsLowDetailLowerPreview()) {''',
    '''  previewCoarseSampleCount_ = 0;\n  previewDemandedTexelCount_ = 0;\n  bool previewFrameChanged = false;\n  if (world_.supportsLowDetailLowerPreview()) {'''
)
replace_once(
    'src/engine/render/Renderer.cpp',
    '''    const bool resetPreviewCache =\n      !previewCacheMatches(nextPreviewKey) || previewSamples_.size() != required;\n    if (resetPreviewCache) {''',
    '''    const bool resetPreviewCache =\n      !previewCacheMatches(nextPreviewKey) || previewSamples_.size() != required;\n    previewFrameChanged = resetPreviewCache;\n    if (resetPreviewCache) {'''
)
replace_once(
    'src/engine/render/Renderer.cpp',
    '''  } else {\n    previewTilesX_ = 0;\n    previewTilesY_ = 0;\n    previewCacheValid_ = false;''',
    '''  } else {\n    previewFrameChanged = previewCacheValid_;\n    previewTilesX_ = 0;\n    previewTilesY_ = 0;\n    previewCacheValid_ = false;'''
)

# Inject damage projection and row spans immediately before the row compositor.
anchor = '''  // Runtime compositing, gamma conversion and framebuffer writes are independent\n  // per row once preview mutation is out of the equation. Thread the hot path'''
damage = r'''  // Project exact runtime damage conservatively into the orthographic frame.
  // When static/cache/preview state is unchanged, every pixel outside current
  // and previous dynamic bounds is already the exact desired pixel from the
  // preceding frame. Reusing it avoids the full-screen four-sample compositor.
  world_.collectRuntimeDamageBounds(runtimeDamageBounds_);
  currentDamageRects_.clear();
  currentDamageRects_.reserve(runtimeDamageBounds_.size());
  const float pixelsPerWorldX = static_cast<float>(frameWidth_) / width;
  const float pixelsPerWorldY = static_cast<float>(frameHeight_) / height;

  for (const RuntimeDamageBound& bound : runtimeDamageBounds_) {
    if (bound.radius < 0.0f) continue;
    const Vec3 delta = bound.centre - focus;
    const float centreX =
      (0.5f + dot(delta, right) / width) * static_cast<float>(frameWidth_);
    const float centreY =
      (0.5f - dot(delta, up) / height) * static_cast<float>(frameHeight_);
    const float radiusX = bound.radius * pixelsPerWorldX + 3.0f;
    const float radiusY = bound.radius * pixelsPerWorldY + 3.0f;
    DamageRect rect;
    rect.minimumX = std::max(0, static_cast<int>(std::floor(centreX - radiusX)));
    rect.maximumX = std::min(frameWidth_, static_cast<int>(std::ceil(centreX + radiusX)) + 1);
    rect.minimumY = std::max(0, static_cast<int>(std::floor(centreY - radiusY)));
    rect.maximumY = std::min(frameHeight_, static_cast<int>(std::ceil(centreY + radiusY)) + 1);
    if (rect.minimumX < rect.maximumX && rect.minimumY < rect.maximumY) {
      currentDamageRects_.push_back(rect);
    }
  }

  const bool fullSceneRender =
    !useStaticCache ||
    staticFrameChanged ||
    previewFrameChanged ||
    previewVisualRevision_ != lastRenderedPreviewRevision_ ||
    !damageHistoryValid_;

  dirtyMinimumX_.assign(static_cast<std::size_t>(frameHeight_), frameWidth_);
  dirtyMaximumX_.assign(static_cast<std::size_t>(frameHeight_), 0);
  const auto markDamage = [&](const DamageRect& rect) {
    for (int y = rect.minimumY; y < rect.maximumY; ++y) {
      dirtyMinimumX_[static_cast<std::size_t>(y)] = std::min(
        dirtyMinimumX_[static_cast<std::size_t>(y)],
        rect.minimumX
      );
      dirtyMaximumX_[static_cast<std::size_t>(y)] = std::max(
        dirtyMaximumX_[static_cast<std::size_t>(y)],
        rect.maximumX
      );
    }
  };

  if (fullSceneRender) {
    std::fill(dirtyMinimumX_.begin(), dirtyMinimumX_.end(), 0);
    std::fill(dirtyMaximumX_.begin(), dirtyMaximumX_.end(), frameWidth_);
  } else {
    for (const DamageRect& rect : previousDamageRects_) markDamage(rect);
    for (const DamageRect& rect : currentDamageRects_) markDamage(rect);
  }

  // Runtime compositing, gamma conversion and framebuffer writes are independent
  // per row once preview mutation is out of the equation. Thread the hot path'''
replace_once('src/engine/render/Renderer.cpp', anchor, damage)

# Row compositor now starts/ends at the dirty span for each row. Replaying x
# additions preserves the exact old floating-point origin sequence.
replace_once(
    'src/engine/render/Renderer.cpp',
    '''    std::size_t pixelIndex =\n      static_cast<std::size_t>(yBegin) * static_cast<std::size_t>(frameWidth_);\n    for (int y = yBegin; y < yEnd; ++y) {\n      Vec3 pixelOrigin = rowOrigin;''',
    '''    for (int y = yBegin; y < yEnd; ++y) {\n      const int xBegin = dirtyMinimumX_[static_cast<std::size_t>(y)];\n      const int xEnd = dirtyMaximumX_[static_cast<std::size_t>(y)];\n      if (xBegin >= xEnd) {\n        rowOrigin = rowOrigin + downStep;\n        continue;\n      }\n\n      Vec3 pixelOrigin = rowOrigin;\n      for (int skippedX = 0; skippedX < xBegin; ++skippedX) {\n        pixelOrigin = pixelOrigin + rightStep;\n      }\n      std::size_t pixelIndex =\n        static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth_) +\n        static_cast<std::size_t>(xBegin);'''
)
replace_once(
    'src/engine/render/Renderer.cpp',
    '''      for (int x = 0; x < frameWidth_; ++x, ++pixelIndex) {''',
    '''      for (int x = xBegin; x < xEnd; ++x, ++pixelIndex) {'''
)

# Persist damage history after scene rendering, before controls are drawn.
replace_once(
    'src/engine/render/Renderer.cpp',
    '''  if (rebuildStaticCache) {\n    staticCacheKey_ = nextStaticKey;\n    staticCacheValid_ = true;\n    ++staticCacheBuildCount_;\n  }\n\n  LevelControlState levelState;''',
    '''  if (rebuildStaticCache) {\n    staticCacheKey_ = nextStaticKey;\n    staticCacheValid_ = true;\n    ++staticCacheBuildCount_;\n  }\n\n  previousDamageRects_ = currentDamageRects_;\n  damageHistoryValid_ = true;\n  lastRenderedPreviewRevision_ = previewVisualRevision_;\n\n  LevelControlState levelState;'''
)

print('Applied exact retained-frame damage-region rendering.')
