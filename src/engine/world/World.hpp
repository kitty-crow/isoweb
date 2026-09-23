#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/character/SpriteAtlas.hpp"
#include "engine/world/EntityStore.hpp"
#include "engine/world/IWorld.hpp"
#include "engine/world/LiminalObject.hpp"
#include "engine/world/Room.hpp"

namespace isoweb {
namespace engine {

class Character;
class CharacterSystem;
class CollisionPolicy;

struct LevelLight {
  bool configured = false;
  Vec3 position;
  float ambient = 0.19f;
  float attenuation = 0.018f;
  float directScale = 1.18f;
};

class IWorldLevel {
public:
  virtual ~IWorldLevel() = default;

  // Exactly one level is render-resident at a time. Implementations with
  // heavyweight assets/resources should release them when residency becomes
  // false and reacquire them when it becomes true. Lightweight simulation
  // topology may remain available so off-view Characters can keep simulating.
  void setResident(bool resident) {
    if (resident_ == resident) return;
    resident_ = resident;
    onResidencyChanged(resident_);
  }

  bool isResident() const { return resident_; }

  virtual const WorldBounds& bounds() const = 0;
  virtual Vec3 sample(const Ray& ray, float backgroundY) const = 0;

  // Renderers that can return the resolved surface together with its shaded
  // colour should override this. The default preserves compatibility but may
  // perform two traversals; optimized levels can do both from one traversal.
  virtual Vec3 sampleWithHit(const Ray& ray, float backgroundY, SceneSurfaceHit& hit) const {
    const bool found = traceEnvironment(ray, hit);
    if (!found) hit = SceneSurfaceHit();
    return sample(ray, backgroundY);
  }

  virtual bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const = 0;
  virtual bool buildGpuStaticScene(GpuStaticScene&) const { return false; }

  // Exact any-hit query for shadow rays. The default is correct for existing
  // levels; optimized levels can terminate on the first blocker.
  virtual bool rayOccluded(const Ray& ray, float maximumDistance) const {
    SceneSurfaceHit hit;
    return traceEnvironment(ray, hit) && hit.distance < maximumDistance;
  }

  // Static presentation may depend on camera orientation (for example,
  // cutaway walls). This is render state only and must not mutate collision.
  virtual void prepareRenderFrame(const Vec3&) const {}

  virtual bool walkableSurfaceAt(float x, float y, SceneSurfaceHit& hit) const = 0;
  virtual const std::vector<Object>& objects() const = 0;
  virtual const RoomLayout* roomLayout() const { return nullptr; }
  virtual bool overlapsStatic(std::size_t objectIndex, const Object& candidate) const = 0;
  // Static boundaries that are structural rather than ordinary world objects
  // may participate in collision without changing objects() API semantics.
  virtual bool overlapsAdditionalStatic(const Object&) const { return false; }
  virtual bool intersectsSolid(const HitBox& hitBox) const = 0;

protected:
  virtual void onResidencyChanged(bool) {}

private:
  bool resident_ = false;
};

class World : public IWorld {
public:
  // Starts in an inert, non-authored state so package/document loading can be
  // the sole source of runtime world content.
  World();
  World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex);

  // Replace the authored level projection without replacing the World object.
  // Runtime systems retain references to World, so package-driven rebuilds
  // can atomically swap level data while preserving those system bindings.
  bool replaceLevels(
    std::vector<std::unique_ptr<IWorldLevel>> levels,
    std::size_t defaultLevelIndex
  );

  const WorldBounds& bounds() const override;
  const WorldBounds& cameraBounds() const override;
  Vec3 sample(const Ray& ray, float backgroundY) const override;
  bool supportsStaticSampleCache() const override { return true; }
  bool runtimeCompositeThreadSafe() const override { return true; }
  bool buildGpuStaticScene(GpuStaticScene& scene) const override;
  bool supportsLowDetailLowerPreview() const override {
    return lowerLevelPreviewDepth_ > 0 && activeLevelIndex_ > 0;
  }
  float lowerPreviewResolutionScale() const override { return lowerPreviewResolutionScale_; }
  std::uint64_t lowDetailPreviewRevision() const override { return lowDetailPreviewRevision_; }
  bool sampleLowDetailLowerPreview(const Ray& ray, Vec3& colour) const override;

  Vec3 sampleEnvironment(
    const Ray& ray,
    float backgroundY,
    float& environmentDistance
  ) const override;

  Vec3 compositeRuntime(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance
  ) const override {
    return compositeRuntimeForWorker(ray, environmentColour, environmentDistance, 0);
  }

  Vec3 compositeRuntimeForWorker(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance,
    std::size_t workerSlot
  ) const override {
    // Renderer always prepares the frame first. Destination acknowledgements
    // live in this dynamic layer too, so an off-view Character may still leave
    // its destination footprint visible on the currently rendered endpoint.
    if (
      runtimeRenderCachePrepared_ &&
      runtimeRenderEntries_.empty() &&
      destinationFeedbackMarkers_.empty() &&
      lowDetailPreviewMarkers_.empty()
    ) {
      return environmentColour;
    }
    bool found = false;
    const Vec3 runtime = sampleRuntimeEntities(
      ray,
      environmentColour,
      environmentDistance,
      found,
      workerSlot
    );
    return found ? runtime : environmentColour;
  }

  bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const override;
  const std::vector<Object>& objects() const override;
  bool intersectsSolid(const HitBox& hitBox) const override;
  bool collidesWith(const Object& candidate) const override;
  void prepareRenderFrame(const Vec3& viewDirection) const override;

  std::size_t levelCount() const override { return levels_.size(); }
  std::size_t activeLevelIndex() const override { return activeLevelIndex_; }
  std::size_t defaultLevelIndex() const override { return defaultLevelIndex_; }

  const std::string& activeLevelId() const;
  bool setLevelId(std::size_t index, const std::string& id);
  std::size_t levelIndex(const std::string& levelId) const;
  const WorldBounds& bounds(const std::string& levelId) const;
  const RoomLayout* roomLayout(const std::string& levelId) const;

  // Preview depth is configuration, not a fixed engine limit. Lower levels
  // remain available for topology/input but normal rendering uses only a cheap
  // reduced-resolution floor-plan preview; only the active level is full quality.
  void setLowerLevelPreviewDepth(std::size_t depth);
  std::size_t lowerLevelPreviewDepth() const { return lowerLevelPreviewDepth_; }
  void setLowerPreviewResolutionScale(float scale);
  bool setLevelViewOrigin(const std::string& levelId, const Vec3& origin);
  Vec3 levelViewOrigin(const std::string& levelId) const;

  const std::vector<Object>& objects(const std::string& levelId) const;

  bool isLevelResident(std::size_t index) const;
  bool isLevelResident(const std::string& levelId) const;
  std::size_t residentLevelCount() const;

  EntityStore& entities() { return entities_; }
  const EntityStore& entities() const { return entities_; }
  SpriteAtlasRegistry& spriteAtlases() { return spriteAtlases_; }
  const SpriteAtlasRegistry& spriteAtlases() const { return spriteAtlases_; }

  bool collidesWith(const Object& candidate, const Object* ignored) const;
  bool containsPosition(const std::string& levelId, const Vec3& position) const;

  bool traceEnvironment(const std::string& levelId, const Ray& ray, SceneSurfaceHit& hit) const;
  float environmentDistance(const Ray& ray) const;
  bool pickWalkableSurface(const Ray& ray, SceneSurfaceHit& hit) const;
  bool pickWalkableDestination(
    const Ray& ray,
    EntityLocation& destination,
    SceneSurfaceHit* hit = nullptr
  ) const;
  bool walkableSurfaceAt(const std::string& levelId, float x, float y, SceneSurfaceHit& hit) const;
  bool resolveWalkablePosition(
    const Object& object,
    const std::string& levelId,
    const Vec3& requested,
    float referenceZ,
    float maxStepUp,
    float maxDrop,
    Vec3& resolved,
    const Object* ignored = nullptr
  ) const;

  // Liminal objects are the single authoritative connectors between levels
  // (and, at a higher world-manager layer, worlds). Only the active endpoint
  // level is rendered; a Character physically inside a liminal object can be
  // projected into either endpoint's local coordinate frame.
  const std::vector<LiminalObject>& liminalObjects() const { return liminalObjects_; }
  const std::vector<NavigationLink>& navigationLinks() const { return liminalObjects_; }
  void setNavigationLinks(std::vector<NavigationLink> links);
  const LiminalObject* liminalObject(const std::string& id) const;
  std::string liminalObjectAt(
    const std::string& levelId,
    const Vec3& position,
    float tolerance = 0.32f
  ) const;
  bool mapLiminalPosition(
    const EntityLocation& location,
    const std::string& targetLevelId,
    Vec3& mapped
  ) const;
  bool characterVisibleOnActiveLevel(const Character& character) const;
  bool renderPositionFor(const Character& character, Vec3& position) const;

  // Runtime entities use the same point light and the same environment ray
  // geometry as the level renderer. This makes static object shadows affect
  // Characters instead of treating them as an unlit post-process overlay.
  bool setLevelLight(
    const std::string& levelId,
    const Vec3& position,
    float ambient = 0.19f,
    float attenuation = 0.018f,
    float directScale = 1.18f
  );
  float runtimeLightVisibility(const std::string& levelId, const Vec3& point) const;
  Vec3 shadeRuntimeSurface(
    const std::string& levelId,
    const Vec3& point,
    const Vec3& normal,
    const Vec3& colour
  ) const;
  float runtimeSpriteLightFactor(const std::string& levelId, const Vec3& point) const;

  void setCharacterSystem(const CharacterSystem* system) { characterSystem_ = system; }
  void setCollisionPolicy(const CollisionPolicy* policy) { collisionPolicy_ = policy; }

  bool canMoveLevelUp() const;
  bool canMoveLevelDown() const;
  bool isDefaultLevel() const;

  bool setActiveLevel(std::size_t index);
  bool levelUp();
  bool levelDown();
  bool resetLevel();

private:
  struct LevelXYBounds {
    bool unrestricted = true;
    float minimumX = 0.0f;
    float minimumY = 0.0f;
    float maximumX = 0.0f;
    float maximumY = 0.0f;
  };

  struct RuntimeSample {
    float distance = 0.0f;
    Vec3 point;
    Vec3 colour;
    float alpha = 1.0f;
  };

  struct RuntimeScratch {
    std::array<RuntimeSample, 8> localSamples;
    std::vector<RuntimeSample> overflowSamples;
  };

  struct RuntimeRenderEntry {
    const Character* character = nullptr;
    Vec3 renderPosition;
    Vec3 viewOffset;
    std::size_t levelIndex = 0;
    Object proxy;
    bool selected = false;
    bool previewOverlay = false;

    // Sprite state and geometry are fixed for one render pass. Cache them once
    // so supersample rays only perform the plane intersection and texel lookup.
    bool artworkReady = false;
    const SpriteAnimation* animation = nullptr;
    std::size_t spriteFrame = 0;
    bool spriteMirror = false;
    Vec3 spriteCentre;
    float spriteInverseWidth = 0.0f;
    float spriteInverseHeight = 0.0f;

    // Conservative world-space sphere around the rendered Character. The
    // runtime ray loop uses it only as a rejection test, so it cannot change
    // which exact sprite/proxy intersection wins.
    Vec3 broadphaseCentre;
    float broadphaseRadiusSquared = 0.0f;
  };

  struct DestinationFeedbackMarker {
    Vec3 position;
    Vec3 forward = {0.0f, 1.0f, 0.0f};
    Vec3 right = {1.0f, 0.0f, 0.0f};
    float minimumX = 0.0f;
    float maximumX = 0.0f;
    float minimumY = 0.0f;
    float maximumY = 0.0f;
    float floorZ = 0.0f;
    float elapsedSeconds = 0.0f;
  };

  static std::uint64_t runtimeBinKey(int x, int y) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
      static_cast<std::uint32_t>(y);
  }

  void buildRuntimeRenderBins(const Vec3& viewDirection) const {
    runtimeRenderBins_.clear();
    runtimeAllIndices_.clear();
    runtimeEmptyIndices_.clear();
    runtimeRenderDirection_ = viewDirection;
    runtimeRenderBinsValid_ = false;

    runtimeAllIndices_.reserve(runtimeRenderEntries_.size());
    for (std::size_t index = 0; index < runtimeRenderEntries_.size(); ++index) {
      runtimeAllIndices_.push_back(index);
    }

    // A hash lookup is more expensive than a handful of exact sphere rejects.
    // Build bins only once the dynamic population is large enough to benefit.
    if (runtimeRenderEntries_.size() < 8) return;

    Vec3 screenRight = cross({0.0f, 0.0f, 1.0f}, viewDirection);
    const float rightLengthSquared = dot(screenRight, screenRight);
    if (rightLengthSquared <= 1e-12f) return;
    screenRight = screenRight / std::sqrt(rightLengthSquared);

    Vec3 screenUp = cross(screenRight, viewDirection);
    const float upLengthSquared = dot(screenUp, screenUp);
    if (upLengthSquared <= 1e-12f) return;
    screenUp = screenUp / std::sqrt(upLengthSquared);

    runtimeRenderScreenRight_ = screenRight;
    runtimeRenderScreenUp_ = screenUp;
    runtimeRenderBins_.reserve(runtimeRenderEntries_.size() * 4 + 8);
    constexpr float binSize = 0.75f;

    // Bin the already-conservative broad-phase spheres. The square projection
    // around each sphere may include extra cells but can never omit a ray that
    // could pass through the exact sprite/proxy geometry.
    for (std::size_t index = 0; index < runtimeRenderEntries_.size(); ++index) {
      const RuntimeRenderEntry& entry = runtimeRenderEntries_[index];
      const float radius = std::sqrt(std::max(0.0f, entry.broadphaseRadiusSquared));
      const float centreX = dot(entry.broadphaseCentre, screenRight);
      const float centreY = dot(entry.broadphaseCentre, screenUp);
      const int minimumCellX = static_cast<int>(std::floor((centreX - radius) / binSize));
      const int maximumCellX = static_cast<int>(std::floor((centreX + radius) / binSize));
      const int minimumCellY = static_cast<int>(std::floor((centreY - radius) / binSize));
      const int maximumCellY = static_cast<int>(std::floor((centreY + radius) / binSize));
      for (int cellY = minimumCellY; cellY <= maximumCellY; ++cellY) {
        for (int cellX = minimumCellX; cellX <= maximumCellX; ++cellX) {
          runtimeRenderBins_[runtimeBinKey(cellX, cellY)].push_back(index);
        }
      }
    }
    runtimeRenderBinsValid_ = true;
  }

  const std::vector<std::size_t>& runtimeCandidateIndices(const Ray& ray) const {
    if (!runtimeRenderBinsValid_) return runtimeAllIndices_;
    const float directionAgreement = dot(ray.direction, runtimeRenderDirection_);
    const float directionLengthSquared = dot(ray.direction, ray.direction);
    const float preparedLengthSquared = dot(runtimeRenderDirection_, runtimeRenderDirection_);
    if (directionLengthSquared <= 1e-12f || preparedLengthSquared <= 1e-12f) {
      return runtimeAllIndices_;
    }
    const float cosine = directionAgreement /
      std::sqrt(directionLengthSquared * preparedLengthSquared);
    if (std::fabs(cosine - 1.0f) > 1e-5f) return runtimeAllIndices_;

    constexpr float binSize = 0.75f;
    const int cellX = static_cast<int>(
      std::floor(dot(ray.origin, runtimeRenderScreenRight_) / binSize)
    );
    const int cellY = static_cast<int>(
      std::floor(dot(ray.origin, runtimeRenderScreenUp_) / binSize)
    );
    const auto found = runtimeRenderBins_.find(runtimeBinKey(cellX, cellY));
    return found == runtimeRenderBins_.end() ? runtimeEmptyIndices_ : found->second;
  }

  struct LowDetailPreviewCharacter {
    std::size_t levelIndex = 0;
    Vec3 position;
    Vec3 forward = {0.0f, 1.0f, 0.0f};
    Vec3 right = {1.0f, 0.0f, 0.0f};
    float minimumX = 0.0f;
    float maximumX = 0.0f;
    float minimumY = 0.0f;
    float maximumY = 0.0f;
    bool selected = false;
  };

  struct LowDetailPreviewMarker {
    std::size_t levelIndex = 0;
    Vec3 position;
    Vec3 forward = {0.0f, 1.0f, 0.0f};
    Vec3 right = {1.0f, 0.0f, 0.0f};
    float minimumX = 0.0f;
    float maximumX = 0.0f;
    float minimumY = 0.0f;
    float maximumY = 0.0f;
    float elapsedSeconds = 0.0f;
  };

  const IWorldLevel& activeLevel() const;
  const IWorldLevel& levelFor(const std::string& levelId) const;
  Vec3 sampleVisibleEnvironment(
    const Ray& ray,
    float backgroundY,
    SceneSurfaceHit& hit,
    std::size_t* sourceLevelIndex = nullptr,
    Vec3* sourceLocalPoint = nullptr
  ) const;
  bool traceVisibleEnvironment(
    const Ray& ray,
    SceneSurfaceHit& hit,
    std::size_t* sourceLevelIndex = nullptr,
    Vec3* sourceLocalPoint = nullptr
  ) const;
  Vec3 levelOffsetInActiveView(std::size_t levelIndex) const;
  void updateLevelResidency();
  void updateVisibleBounds();
  Vec3 compositeDestinationFeedback(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance,
    bool& found
  ) const;
  Vec3 sampleRuntimeEntities(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance,
    bool& found,
    std::size_t workerSlot
  ) const;
  float runtimeLightVisibility(std::size_t levelIndex, const Vec3& point) const;
  Vec3 shadeRuntimeSurface(
    std::size_t levelIndex,
    const Vec3& point,
    const Vec3& normal,
    const Vec3& colour
  ) const;
  float runtimeSpriteLightFactor(std::size_t levelIndex, const Vec3& point) const;

  std::vector<std::unique_ptr<IWorldLevel>> levels_;
  std::vector<std::string> levelIds_;
  std::unordered_map<std::string, std::size_t> levelLookup_;
  std::vector<LevelXYBounds> levelXYBounds_;
  std::vector<LevelLight> levelLights_;
  std::vector<Vec3> levelViewOrigins_;
  std::size_t lowerLevelPreviewDepth_ = 0;
  float lowerPreviewResolutionScale_ = 0.25f;
  WorldBounds visibleBounds_;
  std::size_t activeLevelIndex_ = 0;
  std::size_t defaultLevelIndex_ = 0;
  EntityStore entities_;
  SpriteAtlasRegistry spriteAtlases_;
  std::vector<LiminalObject> liminalObjects_;
  const CharacterSystem* characterSystem_ = nullptr;
  const CollisionPolicy* collisionPolicy_ = nullptr;

  // Renderer currently caps at six helpers. Eight persistent scratch
  // slots leave headroom while keeping the hot ray path allocation-free.
  mutable std::array<RuntimeScratch, 8> runtimeScratchSlots_;
  mutable std::vector<RuntimeRenderEntry> runtimeRenderEntries_;
  mutable Vec3 runtimeRenderDirection_;
  mutable Vec3 runtimeRenderScreenRight_;
  mutable Vec3 runtimeRenderScreenUp_;
  mutable bool runtimeRenderBinsValid_ = false;
  mutable std::unordered_map<std::uint64_t, std::vector<std::size_t>> runtimeRenderBins_;
  mutable std::vector<std::size_t> runtimeAllIndices_;
  mutable std::vector<std::size_t> runtimeEmptyIndices_;
  mutable std::vector<DestinationFeedbackMarker> destinationFeedbackMarkers_;
  mutable std::vector<LowDetailPreviewCharacter> lowDetailPreviewCharacters_;
  mutable std::vector<LowDetailPreviewMarker> lowDetailPreviewMarkers_;
  mutable Vec3 runtimeSpritePlaneNormal_;
  mutable Vec3 runtimeSpriteScreenRight_;
  mutable float runtimeSpriteInverseDenominator_ = 0.0f;
  mutable bool runtimeSpritePlaneValid_ = false;
  mutable bool runtimeRenderCachePrepared_ = false;
  mutable std::uint64_t lowDetailPreviewRevision_ = 1;
  mutable std::uint64_t lowDetailPreviewSignature_ = 0;
};

} // namespace engine
} // namespace isoweb
