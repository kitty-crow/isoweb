#pragma once

#include <cmath>
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
    float& environmentDistance,
    SceneSurfaceHit* environmentHit = nullptr
  ) const override {
    SceneSurfaceHit hit;
    const Vec3 colour = activeLevel().sampleWithHit(ray, backgroundY, hit);
    environmentDistance = hit.found
      ? hit.distance
      : std::numeric_limits<float>::max();
    if (environmentHit) *environmentHit = hit;
    return colour;
  }

  Vec3 compositeRuntime(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance,
    const SceneSurfaceHit* environmentHit = nullptr
  ) const override {
    // Renderer always prepares the frame first. Destination acknowledgements
    // live in this dynamic layer too, so an off-view Character may still leave
    // its destination footprint visible on the currently rendered endpoint.
    if (
      runtimeRenderCachePrepared_ &&
      runtimeRenderEntries_.empty() &&
      destinationFeedbackMarkers_.empty()
    ) {
      return environmentColour;
    }
    const Vec3 shadowedEnvironment = applyRuntimeEnvironmentShadow(
      ray,
      environmentColour,
      environmentDistance,
      environmentHit
    );
    bool found = false;
    const Vec3 runtime = sampleRuntimeEntities(
      ray,
      shadowedEnvironment,
      environmentDistance,
      found
    );
    return found ? runtime : shadowedEnvironment;
  }

  bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const override;
  const std::vector<Object>& objects() const override;
  bool intersectsSolid(const HitBox& hitBox) const override;
  bool collidesWith(const Object& candidate) const override;
  void prepareRenderFrame(const Vec3& viewDirection) const override;
  void prepareRuntimeAcceleration(const Vec3& viewDirection) const override {
    buildRuntimeRenderBins(viewDirection);
    buildRuntimeShadowMap();
  }

  std::size_t runtimeRenderCandidateCount(const Ray& ray) const {
    return runtimeCandidateIndices(ray).size();
  }
  float runtimeDynamicLightVisibility(const std::string& levelId, const Vec3& point) const {
    return levelIndex(levelId) == activeLevelIndex_ ? runtimeDynamicShadowVisibility(point) : 1.0f;
  }
  std::size_t runtimeShadowRasterTestCount() const { return runtimeShadowRasterTests_; }

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
    mutable bool spriteLightValid = false;
    mutable float spriteLightFactor = 1.0f;
    mutable bool faceLightValid[6] = {false, false, false, false, false, false};
    mutable float faceLightFactor[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
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

  enum { RUNTIME_SHADOW_RESOLUTION = 96 };

  static std::uint64_t runtimeBinKey(int x, int y) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
      static_cast<std::uint32_t>(y);
  }

  static float signNotZero(float value) {
    return value < 0.0f ? -1.0f : 1.0f;
  }

  static SurfaceUV encodeOctahedral(const Vec3& direction) {
    const float denominator = std::fabs(direction.x) + std::fabs(direction.y) + std::fabs(direction.z);
    if (denominator <= 1e-12f) return {0.5f, 0.5f};
    Vec3 p = direction / denominator;
    if (p.z < 0.0f) {
      const float oldX = p.x;
      p.x = (1.0f - std::fabs(p.y)) * signNotZero(oldX);
      p.y = (1.0f - std::fabs(oldX)) * signNotZero(p.y);
    }
    return {p.x * 0.5f + 0.5f, p.y * 0.5f + 0.5f};
  }

  static Vec3 decodeOctahedral(float u, float v) {
    Vec3 p(u, v, 1.0f - std::fabs(u) - std::fabs(v));
    if (p.z < 0.0f) {
      const float oldX = p.x;
      p.x = (1.0f - std::fabs(p.y)) * signNotZero(oldX);
      p.y = (1.0f - std::fabs(oldX)) * signNotZero(p.y);
    }
    const float magnitudeSquared = dot(p, p);
    return magnitudeSquared > 1e-12f ? p / std::sqrt(magnitudeSquared) : Vec3(0.0f, 0.0f, 1.0f);
  }

  void buildRuntimeRenderBins(const Vec3& viewDirection) const {
    runtimeRenderBins_.clear();
    runtimeAllIndices_.clear();
    runtimeEmptyIndices_.clear();
    runtimeRenderDirection_ = viewDirection;

    Vec3 screenRight = cross({0.0f, 0.0f, 1.0f}, viewDirection);
    float rightLengthSquared = dot(screenRight, screenRight);
    if (rightLengthSquared <= 1e-12f) {
      screenRight = {1.0f, 0.0f, 0.0f};
    } else {
      screenRight = screenRight / std::sqrt(rightLengthSquared);
    }
    Vec3 screenUp = cross(screenRight, viewDirection);
    const float upLengthSquared = dot(screenUp, screenUp);
    if (upLengthSquared <= 1e-12f) {
      runtimeRenderBinsValid_ = false;
      return;
    }
    screenUp = screenUp / std::sqrt(upLengthSquared);
    runtimeRenderScreenRight_ = screenRight;
    runtimeRenderScreenUp_ = screenUp;
    runtimeRenderBinsValid_ = true;
    runtimeAllIndices_.reserve(runtimeRenderEntries_.size());
    runtimeRenderBins_.reserve(runtimeRenderEntries_.size() * 4 + 8);

    constexpr float binSize = 0.75f;
    for (std::size_t index = 0; index < runtimeRenderEntries_.size(); ++index) {
      const RuntimeRenderEntry& entry = runtimeRenderEntries_[index];
      runtimeAllIndices_.push_back(index);
      float minimumX = std::numeric_limits<float>::max();
      float minimumY = std::numeric_limits<float>::max();
      float maximumX = -std::numeric_limits<float>::max();
      float maximumY = -std::numeric_limits<float>::max();
      auto includePoint = [&](const Vec3& point) {
        const float x = dot(point, screenRight);
        const float y = dot(point, screenUp);
        minimumX = std::min(minimumX, x);
        minimumY = std::min(minimumY, y);
        maximumX = std::max(maximumX, x);
        maximumY = std::max(maximumY, y);
      };

      if (entry.artworkReady && entry.spriteInverseWidth > 0.0f && entry.spriteInverseHeight > 0.0f) {
        const float halfWidth = 0.5f / entry.spriteInverseWidth;
        const float halfHeight = 0.5f / entry.spriteInverseHeight;
        const Vec3 horizontal = screenRight * halfWidth;
        const Vec3 vertical(0.0f, 0.0f, halfHeight);
        includePoint(entry.spriteCentre - horizontal - vertical);
        includePoint(entry.spriteCentre - horizontal + vertical);
        includePoint(entry.spriteCentre + horizontal - vertical);
        includePoint(entry.spriteCentre + horizontal + vertical);
      } else {
        for (int x = 0; x < 2; ++x) {
          for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
              includePoint(entry.proxy.localToWorld({
                x ? entry.proxy.hitBox.maximum.x : entry.proxy.hitBox.minimum.x,
                y ? entry.proxy.hitBox.maximum.y : entry.proxy.hitBox.minimum.y,
                z ? entry.proxy.hitBox.maximum.z : entry.proxy.hitBox.minimum.z
              }));
            }
          }
        }
      }

      const int minimumCellX = static_cast<int>(std::floor(minimumX / binSize));
      const int maximumCellX = static_cast<int>(std::floor(maximumX / binSize));
      const int minimumCellY = static_cast<int>(std::floor(minimumY / binSize));
      const int maximumCellY = static_cast<int>(std::floor(maximumY / binSize));
      for (int cellY = minimumCellY; cellY <= maximumCellY; ++cellY) {
        for (int cellX = minimumCellX; cellX <= maximumCellX; ++cellX) {
          runtimeRenderBins_[runtimeBinKey(cellX, cellY)].push_back(index);
        }
      }
    }
  }

  const std::vector<std::size_t>& runtimeCandidateIndices(const Ray& ray) const {
    if (!runtimeRenderBinsValid_) return runtimeAllIndices_;
    const float directionAgreement = dot(ray.direction, runtimeRenderDirection_);
    if (std::fabs(directionAgreement - 1.0f) > 1e-5f) return runtimeAllIndices_;
    constexpr float binSize = 0.75f;
    const int cellX = static_cast<int>(std::floor(dot(ray.origin, runtimeRenderScreenRight_) / binSize));
    const int cellY = static_cast<int>(std::floor(dot(ray.origin, runtimeRenderScreenUp_) / binSize));
    const auto found = runtimeRenderBins_.find(runtimeBinKey(cellX, cellY));
    return found == runtimeRenderBins_.end() ? runtimeEmptyIndices_ : found->second;
  }

  void buildRuntimeShadowMap() const {
    runtimeShadowMapValid_ = false;
    runtimeShadowRasterTests_ = 0;
    if (activeLevelIndex_ >= levelLights_.size() || !levelLights_[activeLevelIndex_].configured) return;

    const LevelLight& light = levelLights_[activeLevelIndex_];
    const std::size_t texelCount =
      static_cast<std::size_t>(RUNTIME_SHADOW_RESOLUTION) * RUNTIME_SHADOW_RESOLUTION;
    runtimeShadowDepth_.assign(texelCount, std::numeric_limits<float>::max());
    bool hasCaster = false;

    for (const RuntimeRenderEntry& entry : runtimeRenderEntries_) {
      if (!entry.character || !entry.character->castsShadow || !entry.proxy.solid || entry.artworkReady) continue;
      hasCaster = true;
      float minimumU = 1.0f;
      float minimumV = 1.0f;
      float maximumU = 0.0f;
      float maximumV = 0.0f;
      for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
          for (int z = 0; z < 2; ++z) {
            const Vec3 point = entry.proxy.localToWorld({
              x ? entry.proxy.hitBox.maximum.x : entry.proxy.hitBox.minimum.x,
              y ? entry.proxy.hitBox.maximum.y : entry.proxy.hitBox.minimum.y,
              z ? entry.proxy.hitBox.maximum.z : entry.proxy.hitBox.minimum.z
            });
            const Vec3 delta = point - light.position;
            const float lengthSquared = dot(delta, delta);
            if (lengthSquared <= 1e-12f) continue;
            const SurfaceUV uv = encodeOctahedral(delta / std::sqrt(lengthSquared));
            minimumU = std::min(minimumU, uv.u);
            minimumV = std::min(minimumV, uv.v);
            maximumU = std::max(maximumU, uv.u);
            maximumV = std::max(maximumV, uv.v);
          }
        }
      }

      const bool crossesSeam = maximumU - minimumU > 0.75f || maximumV - minimumV > 0.75f;
      int x0 = 0;
      int y0 = 0;
      int x1 = RUNTIME_SHADOW_RESOLUTION - 1;
      int y1 = RUNTIME_SHADOW_RESOLUTION - 1;
      if (!crossesSeam) {
        x0 = std::max(0, static_cast<int>(std::floor(minimumU * RUNTIME_SHADOW_RESOLUTION)) - 1);
        y0 = std::max(0, static_cast<int>(std::floor(minimumV * RUNTIME_SHADOW_RESOLUTION)) - 1);
        x1 = std::min(RUNTIME_SHADOW_RESOLUTION - 1,
          static_cast<int>(std::floor(maximumU * RUNTIME_SHADOW_RESOLUTION)) + 1);
        y1 = std::min(RUNTIME_SHADOW_RESOLUTION - 1,
          static_cast<int>(std::floor(maximumV * RUNTIME_SHADOW_RESOLUTION)) + 1);
      }

      for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
          const float u = ((static_cast<float>(x) + 0.5f) / RUNTIME_SHADOW_RESOLUTION) * 2.0f - 1.0f;
          const float v = ((static_cast<float>(y) + 0.5f) / RUNTIME_SHADOW_RESOLUTION) * 2.0f - 1.0f;
          const Vec3 direction = decodeOctahedral(u, v);
          ObjectRayHit hit;
          ++runtimeShadowRasterTests_;
          if (!entry.proxy.intersectRayExact({light.position, direction}, 0.01f, 1000.0f, hit)) continue;
          const std::size_t offset = static_cast<std::size_t>(y) * RUNTIME_SHADOW_RESOLUTION + x;
          runtimeShadowDepth_[offset] = std::min(runtimeShadowDepth_[offset], hit.distance);
        }
      }
    }
    runtimeShadowMapValid_ = hasCaster;
  }

  float runtimeDynamicShadowVisibility(const Vec3& point) const {
    if (!runtimeShadowMapValid_ || activeLevelIndex_ >= levelLights_.size()) return 1.0f;
    const Vec3 delta = point - levelLights_[activeLevelIndex_].position;
    const float distanceSquared = dot(delta, delta);
    if (distanceSquared <= 1e-12f) return 1.0f;
    const float distance = std::sqrt(distanceSquared);
    const SurfaceUV uv = encodeOctahedral(delta / distance);
    const int x = std::max(0, std::min(RUNTIME_SHADOW_RESOLUTION - 1,
      static_cast<int>(uv.u * RUNTIME_SHADOW_RESOLUTION)));
    const int y = std::max(0, std::min(RUNTIME_SHADOW_RESOLUTION - 1,
      static_cast<int>(uv.v * RUNTIME_SHADOW_RESOLUTION)));
    const float depth = runtimeShadowDepth_[static_cast<std::size_t>(y) * RUNTIME_SHADOW_RESOLUTION + x];
    const float bias = std::max(0.045f, distance * 0.0025f);
    return depth + bias < distance ? 0.0f : 1.0f;
  }

  Vec3 applyRuntimeEnvironmentShadow(
    const Ray& ray,
    const Vec3& environmentColour,
    float environmentDistance,
    const SceneSurfaceHit* environmentHit
  ) const {
    if (!environmentHit || !environmentHit->found || !runtimeShadowMapValid_) return environmentColour;
    if (activeLevelIndex_ >= levelLights_.size() || !levelLights_[activeLevelIndex_].configured) return environmentColour;
    const Vec3 point = ray.origin + ray.direction * environmentDistance;
    if (runtimeDynamicShadowVisibility(point) > 0.5f) return environmentColour;

    const LevelLight& light = levelLights_[activeLevelIndex_];
    const Vec3 toLight = light.position - point;
    const float distanceSquared = dot(toLight, toLight);
    if (distanceSquared <= 1e-12f) return environmentColour;
    const Vec3 direction = toLight / std::sqrt(distanceSquared);
    if (dot(environmentHit->normal, direction) <= 0.0f) return environmentColour;
    const Vec3 ambient = environmentHit->colour * light.ambient;
    return {
      std::min(ambient.x, 1.0f),
      std::min(ambient.y, 1.0f),
      std::min(ambient.z, 1.0f)
    };
  }

  float runtimeEntryLightFactor(const RuntimeRenderEntry& entry, ObjectFace face, const Vec3& normal) const {
    const int faceIndex = static_cast<int>(face);
    if (entry.faceLightValid[faceIndex]) return entry.faceLightFactor[faceIndex];
    if (activeLevelIndex_ >= levelLights_.size() || !levelLights_[activeLevelIndex_].configured) {
      entry.faceLightValid[faceIndex] = true;
      entry.faceLightFactor[faceIndex] = 1.0f;
      return 1.0f;
    }

    Vec3 local = entry.proxy.hitBox.centre();
    switch (face) {
      case ObjectFace::Left: local.x = entry.proxy.hitBox.minimum.x; break;
      case ObjectFace::Right: local.x = entry.proxy.hitBox.maximum.x; break;
      case ObjectFace::Back: local.y = entry.proxy.hitBox.minimum.y; break;
      case ObjectFace::Front: local.y = entry.proxy.hitBox.maximum.y; break;
      case ObjectFace::Bottom: local.z = entry.proxy.hitBox.minimum.z; break;
      case ObjectFace::Top: local.z = entry.proxy.hitBox.maximum.z; break;
    }
    const Vec3 point = entry.proxy.localToWorld(local);
    const LevelLight& light = levelLights_[activeLevelIndex_];
    const Vec3 toLight = light.position - point;
    const float distanceSquared = dot(toLight, toLight);
    float factor = 1.0f;
    if (distanceSquared > 1e-12f) {
      const float distance = std::sqrt(distanceSquared);
      const Vec3 direction = toLight / distance;
      const float diffuse = std::max(0.0f, dot(normal, direction));
      const float attenuation = 1.0f / (1.0f + light.attenuation * distanceSquared);
      const float maximumDistance = distance - 0.006f;
      const bool staticBlocked = maximumDistance > 0.0f &&
        levels_[activeLevelIndex_]->rayOccluded({point + direction * 0.003f, direction}, maximumDistance);
      const float visibility = staticBlocked || runtimeDynamicShadowVisibility(point) < 0.5f ? 0.0f : 1.0f;
      factor = light.ambient + visibility * diffuse * attenuation * light.directScale;
    }
    entry.faceLightValid[faceIndex] = true;
    entry.faceLightFactor[faceIndex] = factor;
    return factor;
  }

  Vec3 shadeRuntimeEntryFace(
    const RuntimeRenderEntry& entry,
    const ObjectRayHit& hit,
    const Vec3& colour
  ) const {
    const Vec3 shaded = colour * runtimeEntryLightFactor(entry, hit.face, hit.worldNormal);
    return {std::min(shaded.x, 1.0f), std::min(shaded.y, 1.0f), std::min(shaded.z, 1.0f)};
  }

  float runtimeEntrySpriteLightFactor(const RuntimeRenderEntry& entry) const {
    if (entry.spriteLightValid) return entry.spriteLightFactor;
    if (activeLevelIndex_ >= levelLights_.size() || !levelLights_[activeLevelIndex_].configured) {
      entry.spriteLightValid = true;
      entry.spriteLightFactor = 1.0f;
      return 1.0f;
    }
    const LevelLight& light = levelLights_[activeLevelIndex_];
    const Vec3 toLight = light.position - entry.spriteCentre;
    const float distanceSquared = dot(toLight, toLight);
    float factor = 1.0f;
    if (distanceSquared > 1e-12f) {
      const float distance = std::sqrt(distanceSquared);
      const Vec3 direction = toLight / distance;
      const float maximumDistance = distance - 0.006f;
      const bool staticBlocked = maximumDistance > 0.0f &&
        levels_[activeLevelIndex_]->rayOccluded(
          {entry.spriteCentre + direction * 0.003f, direction}, maximumDistance
        );
      const float visibility = staticBlocked || runtimeDynamicShadowVisibility(entry.spriteCentre) < 0.5f ? 0.0f : 1.0f;
      factor = light.ambient + visibility * (1.0f - light.ambient);
    }
    entry.spriteLightValid = true;
    entry.spriteLightFactor = factor;
    return factor;
  }

  const IWorldLevel& activeLevel() const;
  const IWorldLevel& levelFor(const std::string& levelId) const;
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
  mutable std::vector<DestinationFeedbackMarker> destinationFeedbackMarkers_;
  mutable std::vector<RuntimeSample> runtimeSampleScratch_;
  mutable Vec3 runtimeSpritePlaneNormal_;
  mutable Vec3 runtimeSpriteScreenRight_;
  mutable float runtimeSpriteInverseDenominator_ = 0.0f;
  mutable bool runtimeSpritePlaneValid_ = false;
  mutable bool runtimeRenderCachePrepared_ = false;

  mutable Vec3 runtimeRenderDirection_;
  mutable Vec3 runtimeRenderScreenRight_;
  mutable Vec3 runtimeRenderScreenUp_;
  mutable bool runtimeRenderBinsValid_ = false;
  mutable std::unordered_map<std::uint64_t, std::vector<std::size_t>> runtimeRenderBins_;
  mutable std::vector<std::size_t> runtimeAllIndices_;
  mutable std::vector<std::size_t> runtimeEmptyIndices_;

  mutable std::vector<float> runtimeShadowDepth_;
  mutable bool runtimeShadowMapValid_ = false;
  mutable std::size_t runtimeShadowRasterTests_ = 0;
};

} // namespace engine
} // namespace isoweb
