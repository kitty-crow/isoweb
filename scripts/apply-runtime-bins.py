from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"expected source block not found in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1))

replace_once(
    'src/engine/world/World.hpp',
    '''  struct LowDetailPreviewCharacter {''',
    '''  static std::uint64_t runtimeBinKey(int x, int y) {\n    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |\n      static_cast<std::uint32_t>(y);\n  }\n\n  void buildRuntimeRenderBins(const Vec3& viewDirection) const {\n    runtimeRenderBins_.clear();\n    runtimeAllIndices_.clear();\n    runtimeEmptyIndices_.clear();\n    runtimeRenderDirection_ = viewDirection;\n    runtimeRenderBinsValid_ = false;\n\n    runtimeAllIndices_.reserve(runtimeRenderEntries_.size());\n    for (std::size_t index = 0; index < runtimeRenderEntries_.size(); ++index) {\n      runtimeAllIndices_.push_back(index);\n    }\n\n    // A hash lookup is more expensive than a handful of exact sphere rejects.\n    // Build bins only once the dynamic population is large enough to benefit.\n    if (runtimeRenderEntries_.size() < 8) return;\n\n    Vec3 screenRight = cross({0.0f, 0.0f, 1.0f}, viewDirection);\n    const float rightLengthSquared = dot(screenRight, screenRight);\n    if (rightLengthSquared <= 1e-12f) return;\n    screenRight = screenRight / std::sqrt(rightLengthSquared);\n\n    Vec3 screenUp = cross(screenRight, viewDirection);\n    const float upLengthSquared = dot(screenUp, screenUp);\n    if (upLengthSquared <= 1e-12f) return;\n    screenUp = screenUp / std::sqrt(upLengthSquared);\n\n    runtimeRenderScreenRight_ = screenRight;\n    runtimeRenderScreenUp_ = screenUp;\n    runtimeRenderBins_.reserve(runtimeRenderEntries_.size() * 4 + 8);\n    constexpr float binSize = 0.75f;\n\n    // Bin the already-conservative broad-phase spheres. The square projection\n    // around each sphere may include extra cells but can never omit a ray that\n    // could pass through the exact sprite/proxy geometry.\n    for (std::size_t index = 0; index < runtimeRenderEntries_.size(); ++index) {\n      const RuntimeRenderEntry& entry = runtimeRenderEntries_[index];\n      const float radius = std::sqrt(std::max(0.0f, entry.broadphaseRadiusSquared));\n      const float centreX = dot(entry.broadphaseCentre, screenRight);\n      const float centreY = dot(entry.broadphaseCentre, screenUp);\n      const int minimumCellX = static_cast<int>(std::floor((centreX - radius) / binSize));\n      const int maximumCellX = static_cast<int>(std::floor((centreX + radius) / binSize));\n      const int minimumCellY = static_cast<int>(std::floor((centreY - radius) / binSize));\n      const int maximumCellY = static_cast<int>(std::floor((centreY + radius) / binSize));\n      for (int cellY = minimumCellY; cellY <= maximumCellY; ++cellY) {\n        for (int cellX = minimumCellX; cellX <= maximumCellX; ++cellX) {\n          runtimeRenderBins_[runtimeBinKey(cellX, cellY)].push_back(index);\n        }\n      }\n    }\n    runtimeRenderBinsValid_ = true;\n  }\n\n  const std::vector<std::size_t>& runtimeCandidateIndices(const Ray& ray) const {\n    if (!runtimeRenderBinsValid_) return runtimeAllIndices_;\n    const float directionAgreement = dot(ray.direction, runtimeRenderDirection_);\n    const float directionLengthSquared = dot(ray.direction, ray.direction);\n    const float preparedLengthSquared = dot(runtimeRenderDirection_, runtimeRenderDirection_);\n    if (directionLengthSquared <= 1e-12f || preparedLengthSquared <= 1e-12f) {\n      return runtimeAllIndices_;\n    }\n    const float cosine = directionAgreement /\n      std::sqrt(directionLengthSquared * preparedLengthSquared);\n    if (std::fabs(cosine - 1.0f) > 1e-5f) return runtimeAllIndices_;\n\n    constexpr float binSize = 0.75f;\n    const int cellX = static_cast<int>(\n      std::floor(dot(ray.origin, runtimeRenderScreenRight_) / binSize)\n    );\n    const int cellY = static_cast<int>(\n      std::floor(dot(ray.origin, runtimeRenderScreenUp_) / binSize)\n    );\n    const auto found = runtimeRenderBins_.find(runtimeBinKey(cellX, cellY));\n    return found == runtimeRenderBins_.end() ? runtimeEmptyIndices_ : found->second;\n  }\n\n  struct LowDetailPreviewCharacter {'''
)

replace_once(
    'src/engine/world/World.hpp',
    '''  mutable std::array<RuntimeScratch, 8> runtimeScratchSlots_;\n  mutable std::vector<RuntimeRenderEntry> runtimeRenderEntries_;''',
    '''  mutable std::array<RuntimeScratch, 8> runtimeScratchSlots_;\n  mutable std::vector<RuntimeRenderEntry> runtimeRenderEntries_;\n  mutable Vec3 runtimeRenderDirection_;\n  mutable Vec3 runtimeRenderScreenRight_;\n  mutable Vec3 runtimeRenderScreenUp_;\n  mutable bool runtimeRenderBinsValid_ = false;\n  mutable std::unordered_map<std::uint64_t, std::vector<std::size_t>> runtimeRenderBins_;\n  mutable std::vector<std::size_t> runtimeAllIndices_;\n  mutable std::vector<std::size_t> runtimeEmptyIndices_;'''
)

replace_once(
    'src/engine/world/World.cpp',
    '''  // Lower-preview geometry is static. Characters and destination feedback are\n  // composited separately at full screen resolution, so their motion does not\n  // invalidate or restart progressive floor refinement.\n  runtimeRenderCachePrepared_ = true;''',
    '''  // Lower-preview geometry is static. Characters and destination feedback are\n  // composited separately at full screen resolution, so their motion does not\n  // invalidate or restart progressive floor refinement.\n  buildRuntimeRenderBins(viewDirection);\n  runtimeRenderCachePrepared_ = true;'''
)

replace_once(
    'src/engine/world/World.cpp',
    '''  for (const RuntimeRenderEntry& entry : runtimeRenderEntries_) {\n    const Character* character = entry.character;''',
    '''  for (std::size_t entryIndex : runtimeCandidateIndices(ray)) {\n    const RuntimeRenderEntry& entry = runtimeRenderEntries_[entryIndex];\n    const Character* character = entry.character;'''
)

print('Applied conservative screen-space runtime bins.')
