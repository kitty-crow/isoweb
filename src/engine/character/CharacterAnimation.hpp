#pragma once

#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {

class CharacterAnimationPolicy {
public:
  virtual ~CharacterAnimationPolicy() = default;

  virtual float framesPerSecond(
    const Character& character,
    const SpriteAnimation& animation,
    float effectiveMovementSpeed,
    float baseMovementSpeed
  ) const = 0;
};

class DefaultCharacterAnimationPolicy final : public CharacterAnimationPolicy {
public:
  float framesPerSecond(
    const Character& character,
    const SpriteAnimation& animation,
    float effectiveMovementSpeed,
    float baseMovementSpeed
  ) const override;
};

void advanceCharacterAnimation(
  Character& character,
  float deltaSeconds,
  const SpriteAnimation* animation,
  float framesPerSecond
);

} // namespace engine
} // namespace isoweb
