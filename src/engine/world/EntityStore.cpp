#include "engine/world/EntityStore.hpp"

#include <algorithm>

#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {

Object& EntityStore::add(std::unique_ptr<Object> entity) {
  Object& reference = *entity;
  entities_.push_back(std::move(entity));
  return reference;
}

bool EntityStore::remove(const std::string& id) {
  const auto it = std::remove_if(entities_.begin(), entities_.end(), [&](const std::unique_ptr<Object>& entity) {
    return entity && entity->id == id;
  });
  if (it == entities_.end()) return false;
  entities_.erase(it, entities_.end());
  return true;
}

void EntityStore::clear() {
  entities_.clear();
}

Object* EntityStore::find(const std::string& id) {
  for (const auto& entity : entities_) {
    if (entity && entity->id == id) return entity.get();
  }
  return nullptr;
}

const Object* EntityStore::find(const std::string& id) const {
  for (const auto& entity : entities_) {
    if (entity && entity->id == id) return entity.get();
  }
  return nullptr;
}

std::vector<Object*> EntityStore::all() {
  std::vector<Object*> result;
  result.reserve(entities_.size());
  for (const auto& entity : entities_) if (entity) result.push_back(entity.get());
  return result;
}

std::vector<const Object*> EntityStore::all() const {
  std::vector<const Object*> result;
  result.reserve(entities_.size());
  for (const auto& entity : entities_) if (entity) result.push_back(entity.get());
  return result;
}

std::vector<Character*> EntityStore::characters() {
  std::vector<Character*> result;
  for (const auto& entity : entities_) {
    if (Character* character = dynamic_cast<Character*>(entity.get())) result.push_back(character);
  }
  return result;
}

std::vector<const Character*> EntityStore::characters() const {
  std::vector<const Character*> result;
  for (const auto& entity : entities_) {
    if (const Character* character = dynamic_cast<const Character*>(entity.get())) result.push_back(character);
  }
  return result;
}

} // namespace engine
} // namespace isoweb
