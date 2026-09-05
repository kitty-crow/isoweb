#pragma once

#include <cstddef>
#include <map>
#include <string>

#include "engine/world/Object.hpp"

namespace isoweb {
namespace engine {

enum class CharacterFacing {
  Front,
  Back,
  Left,
  Right
};

struct SpriteAnimation {
  std::string resource;
  std::size_t frameCount = 1;

  bool assigned() const {
    return !resource.empty();
  }
};

struct DirectionalSpriteSet {
  SpriteAnimation front;
  SpriteAnimation back;
  SpriteAnimation left;
  SpriteAnimation right;

  bool hasExplicitRight() const {
    return right.assigned();
  }

  bool hasBaselineDirections() const {
    return front.assigned() && back.assigned() && left.assigned();
  }

  bool hasAnyArtwork() const {
    return front.assigned() || back.assigned() || left.assigned() || right.assigned();
  }
};

struct CharacterSpriteSet {
  DirectionalSpriteSet still;
  DirectionalSpriteSet moving;
  std::map<std::string, DirectionalSpriteSet> actions;

  bool hasRequiredMovementArtwork() const {
    return still.hasBaselineDirections() && moving.hasBaselineDirections();
  }

  bool hasAnyArtwork() const {
    if (still.hasAnyArtwork() || moving.hasAnyArtwork()) return true;
    for (const auto& action : actions) {
      if (action.second.hasAnyArtwork()) return true;
    }
    return false;
  }
};

class Character : public Object {
public:
  bool npc = false;
  bool controllable = true;
  float movementSpeedMultiplier = 1.0f;

  bool moving = false;
  std::string activeAction;
  CharacterSpriteSet sprites;

  bool hasArtwork() const {
    return sprites.hasAnyArtwork();
  }

  bool hasRequiredMovementArtwork() const {
    return sprites.hasRequiredMovementArtwork();
  }
};

} // namespace engine
} // namespace isoweb
