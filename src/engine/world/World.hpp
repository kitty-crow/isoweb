#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/character/SpriteAtlas.hpp"
#include "engine/world/EntityStore.hpp"
#include "engine/world/IWorld.hpp"
#include "engine/world/LiminalObject.hpp"

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

  // Exact any-hit query for shadow rays. The default is correct for existing
  // levels; optimized levels can terminate on the first blocker.
  virtual bool rayOccluded(const Ray& ray, float maximumDistance) const {
    SceneSurfaceHit hit;
    return traceEnvironment(ray, hit) && hit.distance < maximumDistance;
  }

  virtual bool walkableSurfaceAt(float x, float y, SceneSurfaceHit& hit) const = 0;
  virtual const std::vector<Object>& objects() const = 0;
  virtual bool overlapsStatic(std::size_t objectIndex, const Object& candidate) const = 0;
  virtual bool intersectsSolid(const HitBox& hitBox) const = 0;

protected:
  virtual void onResidencyChanged(bool) {}

private:
  bool resident_ = false;
};

class World : public IWorld {
public:
  World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex);

  const WorldBounds& bounds() const override;
  Vec3 sample(const Ray& ray, float backgroundY) const override;
  bool supportsStaticSampleCache() const override { return true; }

  Vec3 sampleEnvironment(
    const Ray& ray,
    float backgroundY,
    float& environmentDistance
  ) const override {
    SceneSurfaceHit hit;
    const Vec3 colour = activeLevel().sampleWithHit(ray, backgroundY, hit);
    environmentDistance = hit.found
      ? hit.distance
      : std::numeric_limits<float>::max();
    return colour;
  }

  Vec3 compositeRuntime(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance
  ) const override {
    // Renderer always prepares the frame first. This makes the overwhelmingly
    // common no-runtime-entity case a single branch per supersample instead of
    // entering the compositor and clearing scratch storage.
    if (runtimeRenderCachePrepared_ && runtimeRenderEntries_.empty()) {
      return environmentColour;
    }
    bool found = false;
    const Vec3 runtime = sampleRuntimeEntities(
      ray,
      environmentColour,
      environmentDistance,
      found
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
  bool walkableSurfaceAt(const std::string& levelId, float x, float y, SceneSurfaceHit& hit) const;
  bool resolveWalkablePosition(
    const Object& object,
    const std::string& levelId,
    const Vec3& requested,
    float referenceZ,
    float maxStepUp,
    float maxDrop,
    Vec3& resolved
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

  struct RuntimeRenderEntry {
    const Character* character = nullptr;
    Vec3 renderPosition;
    Object proxy;
    bool selected = false;

    // Sprite state and geometry are fixed for one render pass. Cache them once
    // so supersample rays only perform the plane intersection and texel lookup.
    bool artworkReady = false;
    const SpriteAnimation* animation = nullptr;
    std::size_t spriteFrame = 0;
    bool spriteMirror = false;
    Vec3 spriteCentre;
    float spriteInverseWidth = 0.0f;
    float spriteInverseHeight = 0.0f;
  };

  const IWorldLevel& activeLevel() const;
  const IWorldLevel& levelFor(const std::string& levelId) const;
  Vec3 sampleRuntimeEntities(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance,
    bool& found
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
  std::size_t activeLevelIndex_ = 0;
  std::size_t defaultLevelIndex_ = 0;
  EntityStore entities_;
  SpriteAtlasRegistry spriteAtlases_;
  std::vector<LiminalObject> liminalObjects_;
  const CharacterSystem* characterSystem_ = nullptr;
  const CollisionPolicy* collisionPolicy_ = nullptr;

  mutable std::vector<RuntimeRenderEntry> runtimeRenderEntries_;
  mutable std::vector<RuntimeSample> runtimeSampleScratch_;
  mutable Vec3 runtimeSpritePlaneNormal_;
  mutable Vec3 runtimeSpriteScreenRight_;
  mutable float runtimeSpriteInverseDenominator_ = 0.0f;
  mutable bool runtimeSpritePlaneValid_ = false;
  mutable bool runtimeRenderCachePrepared_ = false;
};

} // namespace engine
} // namespace isoweb
