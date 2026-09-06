#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/world/Object.hpp"

namespace isoweb {
namespace engine {

class Character;

class EntityStore {
public:
  Object& add(std::unique_ptr<Object> entity);
  bool remove(const std::string& id);
  void clear();

  Object* find(const std::string& id);
  const Object* find(const std::string& id) const;

  std::vector<Object*> all();
  std::vector<const Object*> all() const;
  std::vector<Character*> characters();
  std::vector<const Character*> characters() const;

private:
  std::vector<std::unique_ptr<Object>> entities_;
};

} // namespace engine
} // namespace isoweb
