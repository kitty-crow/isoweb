from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"expected source block not found in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1))

# Give render workers an explicit scratch slot. The default implementation
# preserves compatibility for worlds that do not need per-worker state.
replace_once(
    "src/engine/world/IWorld.hpp",
    '''  virtual Vec3 compositeRuntime(\n    const Ray&,\n    const Vec3& environmentColour,\n    float\n  ) const {\n    return environmentColour;\n  }\n\n  // Parallel frame compositing is opt-in.''',
    '''  virtual Vec3 compositeRuntime(\n    const Ray&,\n    const Vec3& environmentColour,\n    float\n  ) const {\n    return environmentColour;\n  }\n\n  // Threaded renderers may assign a stable worker slot so a world can keep\n  // reusable per-worker scratch without TLS lookups or per-ray construction.\n  // Worlds that do not need scratch simply delegate to the normal compositor.\n  virtual Vec3 compositeRuntimeForWorker(\n    const Ray& ray,\n    const Vec3& environmentColour,\n    float environmentDistance,\n    std::size_t\n  ) const {\n    return compositeRuntime(ray, environmentColour, environmentDistance);\n  }\n\n  // Parallel frame compositing is opt-in.'''
)

replace_once(
    "src/engine/world/World.hpp",
    '''#include <cstddef>\n#include <cstdint>''',
    '''#include <array>\n#include <cstddef>\n#include <cstdint>'''
)

replace_once(
    "src/engine/world/World.hpp",
    '''  Vec3 compositeRuntime(\n    const Ray& ray,\n    const Vec3& environmentColour,\n    float environmentDistance\n  ) const override {\n    // Renderer always prepares the frame first. Destination acknowledgements\n    // live in this dynamic layer too, so an off-view Character may still leave\n    // its destination footprint visible on the currently rendered endpoint.\n    if (\n      runtimeRenderCachePrepared_ &&\n      runtimeRenderEntries_.empty() &&\n      destinationFeedbackMarkers_.empty() &&\n      lowDetailPreviewMarkers_.empty()\n    ) {\n      return environmentColour;\n    }\n    bool found = false;\n    const Vec3 runtime = sampleRuntimeEntities(\n      ray,\n      environmentColour,\n      environmentDistance,\n      found\n    );\n    return found ? runtime : environmentColour;\n  }''',
    '''  Vec3 compositeRuntime(\n    const Ray& ray,\n    const Vec3& environmentColour,\n    float environmentDistance\n  ) const override {\n    return compositeRuntimeForWorker(ray, environmentColour, environmentDistance, 0);\n  }\n\n  Vec3 compositeRuntimeForWorker(\n    const Ray& ray,\n    const Vec3& environmentColour,\n    float environmentDistance,\n    std::size_t workerSlot\n  ) const override {\n    // Renderer always prepares the frame first. Destination acknowledgements\n    // live in this dynamic layer too, so an off-view Character may still leave\n    // its destination footprint visible on the currently rendered endpoint.\n    if (\n      runtimeRenderCachePrepared_ &&\n      runtimeRenderEntries_.empty() &&\n      destinationFeedbackMarkers_.empty() &&\n      lowDetailPreviewMarkers_.empty()\n    ) {\n      return environmentColour;\n    }\n    bool found = false;\n    const Vec3 runtime = sampleRuntimeEntities(\n      ray,\n      environmentColour,\n      environmentDistance,\n      found,\n      workerSlot\n    );\n    return found ? runtime : environmentColour;\n  }'''
)

replace_once(
    "src/engine/world/World.hpp",
    '''  struct RuntimeRenderEntry {''',
    '''  struct RuntimeScratch {\n    std::array<RuntimeSample, 8> localSamples;\n    std::vector<RuntimeSample> overflowSamples;\n  };\n\n  struct RuntimeRenderEntry {'''
)

replace_once(
    "src/engine/world/World.hpp",
    '''  Vec3 sampleRuntimeEntities(\n    const Ray& ray,\n    const Vec3& environmentColour,\n    float environmentDistance,\n    bool& found\n  ) const;''',
    '''  Vec3 sampleRuntimeEntities(\n    const Ray& ray,\n    const Vec3& environmentColour,\n    float environmentDistance,\n    bool& found,\n    std::size_t workerSlot\n  ) const;'''
)

replace_once(
    "src/engine/world/World.hpp",
    '''  mutable std::vector<RuntimeRenderEntry> runtimeRenderEntries_;''',
    '''  // Renderer currently caps at six helpers. Eight persistent scratch\n  // slots leave headroom while keeping the hot ray path allocation-free.\n  mutable std::array<RuntimeScratch, 8> runtimeScratchSlots_;\n  mutable std::vector<RuntimeRenderEntry> runtimeRenderEntries_;'''
)

replace_once(
    "src/engine/world/World.cpp",
    '''Vec3 World::sampleRuntimeEntities(\n  const Ray& ray,\n  const Vec3& environmentColour,\n  float environmentHitDistance,\n  bool& found\n) const {''',
    '''Vec3 World::sampleRuntimeEntities(\n  const Ray& ray,\n  const Vec3& environmentColour,\n  float environmentHitDistance,\n  bool& found,\n  std::size_t workerSlot\n) const {'''
)

replace_once(
    "src/engine/world/World.cpp",
    '''  // Runtime samples are normally extremely shallow. Keep the common path\n  // entirely on the calling thread's stack so parallel rows need no shared or\n  // thread-local scratch lookup. The vector is only populated if more than\n  // eight dynamic surfaces overlap one ray, preserving unlimited exact\n  // compositing for pathological scenes without taxing ordinary frames.\n  std::array<RuntimeSample, 8> localSamples;\n  std::size_t localSampleCount = 0;\n  std::vector<RuntimeSample> overflowSamples;\n  const auto appendSample = [&](const RuntimeSample& value) {\n    if (overflowSamples.empty() && localSampleCount < localSamples.size()) {\n      localSamples[localSampleCount++] = value;\n      return;\n    }\n    if (overflowSamples.empty()) {\n      overflowSamples.reserve(std::max<std::size_t>(\n        runtimeRenderEntries_.size(),\n        localSamples.size() + 1\n      ));\n      overflowSamples.insert(\n        overflowSamples.end(),\n        localSamples.begin(),\n        localSamples.begin() + static_cast<std::ptrdiff_t>(localSampleCount)\n      );\n    }\n    overflowSamples.push_back(value);\n  };''',
    '''  // Reuse one scratch arena per renderer worker. This keeps the ordinary\n  // <=8-overlap path allocation-free without a TLS lookup or constructing\n  // containers for every supersample ray. Slots are disjoint during pthread\n  // row rendering; callers outside the renderer use slot zero.\n  const std::size_t safeWorkerSlot = std::min(\n    workerSlot,\n    runtimeScratchSlots_.size() - 1\n  );\n  RuntimeScratch& scratch = runtimeScratchSlots_[safeWorkerSlot];\n  auto& localSamples = scratch.localSamples;\n  auto& overflowSamples = scratch.overflowSamples;\n  std::size_t localSampleCount = 0;\n  overflowSamples.clear();\n  const auto appendSample = [&](const RuntimeSample& value) {\n    if (overflowSamples.empty() && localSampleCount < localSamples.size()) {\n      localSamples[localSampleCount++] = value;\n      return;\n    }\n    if (overflowSamples.empty()) {\n      if (overflowSamples.capacity() < runtimeRenderEntries_.size()) {\n        overflowSamples.reserve(std::max<std::size_t>(\n          runtimeRenderEntries_.size(),\n          localSamples.size() + 1\n        ));\n      }\n      overflowSamples.insert(\n        overflowSamples.end(),\n        localSamples.begin(),\n        localSamples.begin() + static_cast<std::ptrdiff_t>(localSampleCount)\n      );\n    }\n    overflowSamples.push_back(value);\n  };'''
)

replace_once(
    "src/engine/render/Renderer.cpp",
    '''  const auto renderRows = [&](int yBegin, int yEnd) {''',
    '''  const auto renderRows = [&](int yBegin, int yEnd, std::size_t workerSlot) {'''
)

replace_once(
    "src/engine/render/Renderer.cpp",
    '''          colour = colour + world_.compositeRuntime(\n            ray,\n            environmentColour,\n            environmentDistance\n          );''',
    '''          colour = colour + world_.compositeRuntimeForWorker(\n            ray,\n            environmentColour,\n            environmentDistance,\n            workerSlot\n          );'''
)

# Update every renderRows call in this function. These are deliberately exact
# so later renderer edits cannot silently assign two helpers the same arena.
replace_once(
    "src/engine/render/Renderer.cpp",
    '''          renderRows(yBegin, yEnd);''',
    '''          renderRows(yBegin, yEnd, static_cast<std::size_t>(worker));'''
)
replace_once(
    "src/engine/render/Renderer.cpp",
    '''      renderRows(0, mainEnd);''',
    '''      renderRows(0, mainEnd, 0);'''
)
replace_once(
    "src/engine/render/Renderer.cpp",
    '''      renderRows(0, frameHeight_);''',
    '''      renderRows(0, frameHeight_, 0);'''
)
replace_once(
    "src/engine/render/Renderer.cpp",
    '''    renderRows(0, frameHeight_);''',
    '''    renderRows(0, frameHeight_, 0);'''
)
replace_once(
    "src/engine/render/Renderer.cpp",
    '''  renderRows(0, frameHeight_);''',
    '''  renderRows(0, frameHeight_, 0);'''
)

print("Applied persistent per-worker runtime scratch slots.")
