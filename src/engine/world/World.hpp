#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "engine/world/IWorld.hpp"

namespace isoweb {
namespace engine {

class IWorldLevel {
public:
  virtual ~IWorldLevel() = default;

  virtual const WorldBounds& bounds() const = 0;
  virtual Vec3 sample(const Ray& ray, float backgroundY) const = 0;
  virtual const std::vector<Object>& objects() const = 0;
  virtual bool intersectsSolid(const HitBox& hitBox) const = 0;
};

class World : public IWorld {
public:
  World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex);

  const WorldBounds& bounds() const override;
  Vec3 sample(const Ray& ray, float backgroundY) const override;
  const std::vector<Object>& objects() const override;
  const std::vector<Character>& characters() const override { return characters_; }
  std::vector<Character>& characters() { return characters_; }
  bool intersectsSolid(const HitBox& hitBox) const override;
  bool collidesWith(const Object& candidate) const override;

  std::size_t levelCount() const override { return levels_.size(); }
  std::size_t activeLevelIndex() const override { return activeLevelIndex_; }
  std::size_t defaultLevelIndex() const override { return defaultLevelIndex_; }
  const std::string& activeLevelId() const override { return levelIds_[activeLevelIndex_]; }
  const std::string& levelId(std::size_t index) const;
  bool setLevelId(std::size_t index, std::string id);

  float baseMovementSpeed() const override { return baseMovementSpeed_; }
  void setBaseMovementSpeed(float speed);

  void clearCharacters();
  Character& addCharacter(Character character);

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
  std::vector<Character> characters_;
  std::size_t activeLevelIndex_ = 0;
  std::size_t defaultLevelIndex_ = 0;
  float baseMovementSpeed_ = 1.0f;
};

} // namespace engine
} // namespace isoweb
