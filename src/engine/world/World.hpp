#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "engine/world/IWorld.hpp"

namespace isoweb {
namespace engine {

class IWorldLevel {
public:
  virtual ~IWorldLevel() = default;

  virtual const WorldBounds& bounds() const = 0;
  virtual Vec3 sample(const Ray& ray, float backgroundY) const = 0;
};

class World : public IWorld {
public:
  World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex);

  const WorldBounds& bounds() const override;
  Vec3 sample(const Ray& ray, float backgroundY) const override;

  std::size_t levelCount() const override { return levels_.size(); }
  std::size_t activeLevelIndex() const override { return activeLevelIndex_; }
  std::size_t defaultLevelIndex() const override { return defaultLevelIndex_; }

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
  std::size_t activeLevelIndex_ = 0;
  std::size_t defaultLevelIndex_ = 0;
};

} // namespace engine
} // namespace isoweb
