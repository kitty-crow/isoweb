#include "engine/world/World.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace isoweb {
namespace engine {

World::World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex)
    : levels_(std::move(levels)),
      defaultLevelIndex_(defaultLevelIndex) {
  if (levels_.empty() || defaultLevelIndex_ >= levels_.size()) std::abort();

  levelIds_.reserve(levels_.size());
  for (std::size_t index = 0; index < levels_.size(); ++index) {
    levelIds_.push_back(std::to_string(index));
  }
  activeLevelIndex_ = defaultLevelIndex_;
}

std::size_t World::levelIndexForId(const std::string& levelIdValue) const {
  if (levelIdValue.empty()) return activeLevelIndex_;
  const auto found = std::find(levelIds_.begin(), levelIds_.end(), levelIdValue);
  if (found == levelIds_.end()) return activeLevelIndex_;
  return static_cast<std::size_t>(found - levelIds_.begin());
}

const IWorldLevel& World::activeLevel() const {
  return *levels_[activeLevelIndex_];
}

const WorldBounds& World::bounds() const {
  return activeLevel().bounds();
}

const WorldBounds& World::boundsForLevel(const std::string& levelIdValue) const {
  return levels_[levelIndexForId(levelIdValue)]->bounds();
}

Vec3 World::sample(const Ray& ray, float backgroundY) const {
  return activeLevel().sample(ray, backgroundY);
}

const std::vector<Object>& World::objects() const {
  return activeLevel().objects();
}

const std::vector<Object>& World::objectsForLevel(const std::string& levelIdValue) const {
  return levels_[levelIndexForId(levelIdValue)]->objects();
}

bool World::intersectsSolid(const HitBox& hitBox) const {
  return activeLevel().intersectsSolid(hitBox);
}

bool World::collidesWith(const Object& candidate) const {
  const std::vector<Object>& levelObjects = objectsForLevel(candidate.location.levelId);
  for (const Object& object : levelObjects) {
    if (!candidate.id.empty() && !object.id.empty() && object.id == candidate.id) continue;
    if (object.blocks(candidate)) return true;
  }

  for (const Character& character : characters_) {
    if (&character == &candidate) continue;
    if (!candidate.id.empty() && character.id == candidate.id) continue;
    if (character.blocks(candidate)) return true;
  }
  return false;
}

const std::string& World::levelId(std::size_t index) const {
  if (index >= levelIds_.size()) std::abort();
  return levelIds_[index];
}

bool World::setLevelId(std::size_t index, std::string id) {
  if (index >= levelIds_.size() || id.empty()) return false;
  const auto found = std::find(levelIds_.begin(), levelIds_.end(), id);
  if (found != levelIds_.end() && static_cast<std::size_t>(found - levelIds_.begin()) != index) {
    return false;
  }
  levelIds_[index] = std::move(id);
  return true;
}

void World::setBaseMovementSpeed(float speed) {
  baseMovementSpeed_ = std::max(0.0f, speed);
}

void World::clearCharacters() {
  characters_.clear();
}

Character& World::addCharacter(Character character) {
  characters_.push_back(std::move(character));
  return characters_.back();
}

void World::clearNavigationConnections() {
  navigationConnections_.clear();
}

NavigationConnection& World::addNavigationConnection(NavigationConnection connection) {
  navigationConnections_.push_back(std::move(connection));
  return navigationConnections_.back();
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
