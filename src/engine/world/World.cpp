#include "engine/world/World.hpp"

#include <cstdlib>
#include <utility>

namespace isoweb {
namespace engine {

World::World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex)
    : levels_(std::move(levels)),
      defaultLevelIndex_(defaultLevelIndex) {
  if (levels_.empty() || defaultLevelIndex_ >= levels_.size()) std::abort();
  activeLevelIndex_ = defaultLevelIndex_;
}

const IWorldLevel& World::activeLevel() const {
  return *levels_[activeLevelIndex_];
}

const WorldBounds& World::bounds() const {
  return activeLevel().bounds();
}

Vec3 World::sample(const Ray& ray, float backgroundY) const {
  return activeLevel().sample(ray, backgroundY);
}

const std::vector<WorldObject>& World::objects() const {
  return activeLevel().objects();
}

bool World::intersectsSolid(const HitBox& hitBox) const {
  return activeLevel().intersectsSolid(hitBox);
}

bool World::canMoveLevelUp() const {
  return activeLevelIndex_ + 1 < levels_.size();
}

bool World::canMoveLevelDown() const {
  return activeLevelIndex_ > 0;
}

bool World::isDefaultLevel() const {
  return activeLevelIndex_ == defaultLevelIndex_;
}

bool World::setActiveLevel(std::size_t index) {
  if (index >= levels_.size() || index == activeLevelIndex_) return false;
  activeLevelIndex_ = index;
  return true;
}

bool World::levelUp() {
  return canMoveLevelUp() && setActiveLevel(activeLevelIndex_ + 1);
}

bool World::levelDown() {
  return canMoveLevelDown() && setActiveLevel(activeLevelIndex_ - 1);
}

bool World::resetLevel() {
  return setActiveLevel(defaultLevelIndex_);
}

} // namespace engine
} // namespace isoweb
