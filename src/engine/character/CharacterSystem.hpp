#pragma once

#include <memory>
#include <string>

#include "engine/character/CharacterPolicies.hpp"
#include "engine/character/CharacterPresentation.hpp"
#include "engine/character/CharacterSelection.hpp"
#include "engine/world/World.hpp"

namespace isoweb {
namespace engine {

class CharacterSystem {
public:
  explicit CharacterSystem(World& world);

  CharacterEngineDefaults& defaults() { return defaults_; }
  const CharacterEngineDefaults& defaults() const { return defaults_; }

  CharacterSelection& selection() { return selection_; }
  const CharacterSelection& selection() const { return selection_; }
  bool isSelected(const std::string& id) const { return selection_.selected(id); }
  const SelectionStyle& selectionStyle() const { return selection_.style; }
  void setSelectionMode(SelectionMode mode) {
    defaultSelectionPolicy_.setMode(mode);
    if (mode == SelectionMode::Single && selection_.ids().size() > 1) selection_.clear();
  }

  Character* pick(const Ray& ray, float maximumDistance = 1000.0f) const;
  bool toggleSelection(const Ray& ray, bool additive = true);
  void clearSelection() { selection_.clear(); }

  bool command(Character& character, const EntityLocation& requestedDestination);
  std::size_t commandSelected(const EntityLocation& requestedDestination);
  void stop(Character& character);

  void tick(float deltaSeconds, const Camera& camera);
  void updatePresentation(const Camera& camera);
  bool needsTick() const;
  float effectiveSpeed(const Character& character) const;

  void setCollisionPolicy(CollisionPolicy& policy) {
    collisionPolicy_ = &policy;
    world_.setCollisionPolicy(collisionPolicy_);
  }
  void setDestinationPolicy(DestinationPolicy& policy) { destinationPolicy_ = &policy; }
  void setNavigationPolicy(NavigationPolicy& policy) { navigationPolicy_ = &policy; }
  void setMovementPolicy(MovementPolicy& policy) { movementPolicy_ = &policy; }
  void setInteractionPolicy(InteractionPolicy& policy) { interactionPolicy_ = &policy; }
  void setPresentationPolicy(CharacterPresentationPolicy& policy) { presentationPolicy_ = &policy; }

private:
  void advance(Character& character, float deltaSeconds);

  World& world_;
  CharacterEngineDefaults defaults_;

  DefaultCharacterSelectionPolicy defaultSelectionPolicy_;
  CharacterSelection selection_;
  DestinationPolicy defaultDestinationPolicy_;
  DefaultNavigationPolicy defaultNavigationPolicy_;
  MovementPolicy defaultMovementPolicy_;
  InteractionPolicy defaultInteractionPolicy_;
  DefaultCharacterPresentationPolicy defaultPresentationPolicy_;
  CollisionPolicy defaultCollisionPolicy_;

  CollisionPolicy* collisionPolicy_ = nullptr;
  DestinationPolicy* destinationPolicy_ = nullptr;
  NavigationPolicy* navigationPolicy_ = nullptr;
  MovementPolicy* movementPolicy_ = nullptr;
  InteractionPolicy* interactionPolicy_ = nullptr;
  CharacterPresentationPolicy* presentationPolicy_ = nullptr;
};

} // namespace engine
} // namespace isoweb
