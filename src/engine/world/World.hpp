#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/character/SpriteAtlas.hpp"
#include "engine/world/EntityStore.hpp"
#include "engine/world/IWorld.hpp"

namespace isoweb {
namespace engine {

class CharacterSystem;
class CollisionPolicy;

struct NavigationLink {
  std::string fromLevelId;
  std::string toLevelId;
  Vec3 fromPosition;
  Vec3 toPosition;
  std::vector<Vec3> forwardTraversal;
  std::vector<Vec3> reverseTraversal;
  bool bidirectional = true;
};

class IWorldLevel {
public:
  virtual ~IWorldLevel() = default;

  virtual const WorldBounds& bounds() const = 0;
  virtual WorldSurfaceSample sampleSurface(const Ray& ray, float backgroundY) const = 0;
  virtual Vec3 sample(const Ray& ray, float backgroundY) const {
    return sampleSurface(ray, backgroundY).colour;
  }
  virtual const std::vector<Object>& objects() const = 0;
  virtual bool intersectsSolid(const HitBox& hitBox) const = 0;
};

class World : public IWorld {
public:
  World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex);

  const WorldBounds& bounds() const override;
  Vec3 sample(const Ray& ray, float backgroundY) const override;
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

  const std::vector<NavigationLink>& navigationLinks() const { return navigationLinks_; }
  void setNavigationLinks(std::vector<NavigationLink> links) { navigationLinks_ = std::move(links); }

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

  std::vector<std::unique_ptr<IWorldLevel>> levels_;
  std::vector<std::string> levelIds_;
  std::size_t activeLevelIndex_ = 0;
  std::size_t defaultLevelIndex_ = 0;
  EntityStore entities_;
  SpriteAtlasRegistry spriteAtlases_;
  std::vector<NavigationLink> navigationLinks_;
  const CharacterSystem* characterSystem_ = nullptr;
  const CollisionPolicy* collisionPolicy_ = nullptr;
};

} // namespace engine
} // namespace isoweb
