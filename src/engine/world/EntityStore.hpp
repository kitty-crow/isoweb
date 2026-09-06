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

  const std::vector<Object*>& all();
  const std::vector<const Object*>& all() const;
  const std::vector<Character*>& characters();
  const std::vector<const Character*>& characters() const;

private:
  void rebuildViews();

  std::vector<std::unique_ptr<Object>> entities_;
  std::vector<Object*> allView_;
  std::vector<const Object*> constAllView_;
  std::vector<Character*> characterView_;
  std::vector<const Character*> constCharacterView_;
};

} // namespace engine
} // namespace isoweb
