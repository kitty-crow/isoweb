#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "engine/world/Object.hpp"

namespace isoweb {
namespace engine {

enum class CharacterFacing {
  Front,
  Back,
  Left,
  Right
};

// Runtime character artwork is a WebP-backed frame sequence. frameCount may
// be one for a static pose or greater than one for an animation. The engine,
// not the WebP file, owns frame timing so playback can follow character state
// and effective movement speed.
struct SpriteAnimation {
  std::string resource;
  std::size_t frameCount = 1;
  std::size_t columns = 1;
  std::size_t rows = 1;
  float nominalFramesPerSecond = 6.0f;
  float worldWidth = 0.0f;
  float worldHeight = 0.0f;
  bool loop = true;

  bool assigned() const { return !resource.empty(); }
  bool animated() const { return assigned() && frameCount > 1; }
};

struct DirectionalSpriteSet {
  SpriteAnimation front;
  SpriteAnimation back;
  SpriteAnimation left;
  SpriteAnimation right;

  bool hasExplicitRight() const { return right.assigned(); }
  bool hasBaselineDirections() const { return front.assigned() && back.assigned() && left.assigned(); }
  bool hasAnyArtwork() const {
    return front.assigned() || back.assigned() || left.assigned() || right.assigned();
  }

  const SpriteAnimation* animation(CharacterFacing facing, bool* mirror = nullptr) const {
    if (mirror) *mirror = false;
    switch (facing) {
      case CharacterFacing::Front: return front.assigned() ? &front : nullptr;
      case CharacterFacing::Back: return back.assigned() ? &back : nullptr;
      case CharacterFacing::Left: return left.assigned() ? &left : nullptr;
      case CharacterFacing::Right:
        if (right.assigned()) return &right;
        if (left.assigned()) {
          if (mirror) *mirror = true;
          return &left;
        }
        return nullptr;
    }
    return nullptr;
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
    for (const auto& action : actions) if (action.second.hasAnyArtwork()) return true;
    return false;
  }
};

struct CharacterWaypoint {
  EntityLocation location;
  bool levelTransition = false;
};

struct CharacterMovementState {
  std::vector<CharacterWaypoint> route;
  std::size_t nextWaypoint = 0;
  bool hasDestination = false;
  EntityLocation destination;

  void clear() {
    route.clear();
    nextWaypoint = 0;
    hasDestination = false;
    destination = EntityLocation();
  }
};

struct CharacterAnimationState {
  std::string resource;
  CharacterFacing facing = CharacterFacing::Front;
  bool mirror = false;
  float elapsedSeconds = 0.0f;
  std::size_t frame = 0;

  void reset(const std::string& nextResource = std::string()) {
    resource = nextResource;
    elapsedSeconds = 0.0f;
    frame = 0;
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
  CharacterMovementState movement;
  CharacterAnimationState animation;

  bool hasArtwork() const { return sprites.hasAnyArtwork(); }
  bool hasRequiredMovementArtwork() const { return sprites.hasRequiredMovementArtwork(); }
};

} // namespace engine
} // namespace isoweb
