#include "engine/world/EntityStore.hpp"

#include <algorithm>

#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {

Object& EntityStore::add(std::unique_ptr<Object> entity) {
  Object& reference = *entity;
  Object* pointer = entity.get();
  entities_.push_back(std::move(entity));
  allView_.push_back(pointer);
  constAllView_.push_back(pointer);
  if (Character* character = dynamic_cast<Character*>(pointer)) {
    characterView_.push_back(character);
    constCharacterView_.push_back(character);
  }
  return reference;
}

bool EntityStore::remove(const std::string& id) {
  const auto it = std::remove_if(entities_.begin(), entities_.end(), [&](const std::unique_ptr<Object>& entity) {
    return entity && entity->id == id;
  });
  if (it == entities_.end()) return false;
  entities_.erase(it, entities_.end());
  rebuildViews();
  return true;
}

void EntityStore::clear() {
  entities_.clear();
  allView_.clear();
  constAllView_.clear();
  characterView_.clear();
  constCharacterView_.clear();
}

Object* EntityStore::find(const std::string& id) {
  for (Object* entity : allView_) {
    if (entity && entity->id == id) return entity;
  }
  return nullptr;
}

const Object* EntityStore::find(const std::string& id) const {
  for (const Object* entity : constAllView_) {
    if (entity && entity->id == id) return entity;
  }
  return nullptr;
}

const std::vector<Object*>& EntityStore::all() {
  return allView_;
}

const std::vector<const Object*>& EntityStore::all() const {
  return constAllView_;
}

const std::vector<Character*>& EntityStore::characters() {
  return characterView_;
}

const std::vector<const Character*>& EntityStore::characters() const {
  return constCharacterView_;
}

void EntityStore::rebuildViews() {
  allView_.clear();
  constAllView_.clear();
  characterView_.clear();
  constCharacterView_.clear();
  allView_.reserve(entities_.size());
  constAllView_.reserve(entities_.size());
  characterView_.reserve(entities_.size());
  constCharacterView_.reserve(entities_.size());

  for (const auto& entity : entities_) {
    if (!entity) continue;
    Object* pointer = entity.get();
    allView_.push_back(pointer);
    constAllView_.push_back(pointer);
    if (Character* character = dynamic_cast<Character*>(pointer)) {
      characterView_.push_back(character);
      constCharacterView_.push_back(character);
    }
  }
}

} // namespace engine
} // namespace isoweb
