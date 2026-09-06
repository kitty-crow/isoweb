#include "engine/character/CharacterSelection.hpp"

namespace isoweb {
namespace engine {

bool CharacterSelection::select(Character& character, bool additive) {
  if (!policy_.canSelect(character) || character.id.empty()) return false;
  if (!additive || policy_.mode() == SelectionMode::Single) selectedIds_.clear();
  return selectedIds_.insert(character.id).second;
}

bool CharacterSelection::toggle(Character& character, bool additive) {
  if (selected(character.id)) return deselect(character.id);
  return select(character, additive);
}

bool CharacterSelection::deselect(const std::string& id) {
  return selectedIds_.erase(id) > 0;
}

void CharacterSelection::clear() {
  selectedIds_.clear();
}

bool CharacterSelection::selected(const std::string& id) const {
  return selectedIds_.find(id) != selectedIds_.end();
}

std::vector<Character*> CharacterSelection::resolve(EntityStore& store) const {
  std::vector<Character*> result;
  for (Character* character : store.characters()) {
    if (character && selected(character->id)) result.push_back(character);
  }
  return result;
}

} // namespace engine
} // namespace isoweb
