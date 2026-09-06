#pragma once

#include <cstddef>
#include <memory>
#include <string>
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
  Vec3 position;
  float ambient = 0.19f;
  float attenuation = 0.018f;
  float directScale = 1.18f;
};

class IWorldLevel {
public:
  virtual ~IWorldLevel() = default;

  virtual const WorldBounds& bounds() const = 0;
  virtual Vec3 sample(const Ray& ray, float backgroundY) const = 0;
  virtual bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const = 0;
  virtual bool walkableSurfaceAt(float x, float y, SceneSurfaceHit& hit) const = 0;
  virtual const LevelLight& light() const = 0;
  virtual const std::vector<Object>& objects() const = 0;
  virtual bool overlapsStatic(std::size_t objectIndex, const Object& candidate) const = 0;
  virtual bool intersectsSolid(const HitBox& hitBox) const = 0;
};

class World : public IWorld {
public:
  World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex);

  const WorldBounds& bounds() const override;
  Vec3 sample(const Ray& ray, float backgroundY) const override;
  bool traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const override;
  const std::vector<Object>& objects() const override;
  bool intersectsSolid(const HitBox& hitBox) const override;
  bool collidesWith(const Object& candidate) const override;

  std::size_t levelCount() const override { return levels_.size(); }
  std::size_t activeLevelIndex() const override { return activeLevelIndex_; }
  std::size_t defaultLevelIndex() const override { return defaultLevelIndex_; }

  const std::string& activeLevelId() const;
  bool setLevelId(std::size_t index, const std::string& id);
  std::size_t levelIndex(const std::string& levelId) const;
  const WorldBounds& bounds(const std::string& levelId) const;
  const std::vector<Object>& objects(const std::string& levelId) const;

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

  // Runtime entities query the same light owned by the same level that traces
  // their environment. There is no parallel Character-light configuration.
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
  const IWorldLevel& activeLevel() const;
  const IWorldLevel& levelFor(const std::string& levelId) const;
  Vec3 sampleRuntimeEntities(const Ray& ray, float backgroundY, float environmentDistance, bool& found) const;

  std::vector<std::unique_ptr<IWorldLevel>> levels_;
  std::vector<std::string> levelIds_;
  std::size_t activeLevelIndex_ = 0;
  std::size_t defaultLevelIndex_ = 0;
  EntityStore entities_;
  SpriteAtlasRegistry spriteAtlases_;
  std::vector<LiminalObject> liminalObjects_;
  const CharacterSystem* characterSystem_ = nullptr;
  const CollisionPolicy* collisionPolicy_ = nullptr;
};

} // namespace engine
} // namespace isoweb
