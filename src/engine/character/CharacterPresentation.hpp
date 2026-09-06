#pragma once

#include <cstddef>

#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterAnimation.hpp"
#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {

struct CharacterPresentation {
  CharacterFacing facing = CharacterFacing::Front;
  const SpriteAnimation* animation = nullptr;
  bool mirror = false;
  std::size_t frame = 0;
};

class CharacterPresentationPolicy {
public:
  virtual ~CharacterPresentationPolicy() = default;
  virtual CharacterFacing facing(const Character& character, const Camera& camera) const = 0;
};

class DefaultCharacterPresentationPolicy final : public CharacterPresentationPolicy {
public:
  CharacterFacing facing(const Character& character, const Camera& camera) const override;

  // Compatibility helper for callers that previously asked the default
  // presentation object for timing. CharacterSystem itself routes timing
  // through the independently replaceable CharacterAnimationPolicy.
  float framesPerSecond(
    const Character& character,
    const SpriteAnimation& animation,
    float effectiveMovementSpeed,
    float baseMovementSpeed
  ) const {
    return DefaultCharacterAnimationPolicy().framesPerSecond(
      character,
      animation,
      effectiveMovementSpeed,
      baseMovementSpeed
    );
  }
};

CharacterPresentation resolveCharacterPresentation(
  const Character& character,
  const Camera& camera,
  const CharacterPresentationPolicy& policy
);

} // namespace engine
} // namespace isoweb
