#pragma once

#include <set>
#include <string>
#include <vector>

#include "engine/world/Character.hpp"
#include "engine/world/EntityStore.hpp"

namespace isoweb {
namespace engine {

enum class SelectionMode {
  Multiple,
  Single
};

struct SelectionStyle {
  Vec3 tint = {0.20f, 0.48f, 1.0f};
  float strength = 0.45f;
};

class CharacterSelectionPolicy {
public:
  virtual ~CharacterSelectionPolicy() = default;
  virtual bool canSelect(const Character& character) const { return true; }
  virtual SelectionMode mode() const { return SelectionMode::Multiple; }
};

class DefaultCharacterSelectionPolicy final : public CharacterSelectionPolicy {
public:
  explicit DefaultCharacterSelectionPolicy(SelectionMode mode = SelectionMode::Multiple)
      : mode_(mode) {}

  SelectionMode mode() const override { return mode_; }
  void setMode(SelectionMode mode) { mode_ = mode; }

private:
  SelectionMode mode_;
};

class CharacterSelection {
public:
  explicit CharacterSelection(CharacterSelectionPolicy& policy) : policy_(&policy) {}

  void setPolicy(CharacterSelectionPolicy& policy) {
    policy_ = &policy;
    if (policy_->mode() == SelectionMode::Single && selectedIds_.size() > 1) selectedIds_.clear();
  }

  bool select(Character& character, bool additive = true);
  bool toggle(Character& character, bool additive = true);
  bool deselect(const std::string& id);
  void clear();

  bool selected(const std::string& id) const;
  const std::set<std::string>& ids() const { return selectedIds_; }
  std::vector<Character*> resolve(EntityStore& store) const;

  SelectionStyle style;

private:
  CharacterSelectionPolicy* policy_ = nullptr;
  std::set<std::string> selectedIds_;
};

} // namespace engine
} // namespace isoweb
