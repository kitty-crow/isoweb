#pragma once

#include <cstddef>

#include "engine/camera/Camera.hpp"
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
  virtual float framesPerSecond(const Character& character, const SpriteAnimation& animation, float effectiveMovementSpeed, float baseMovementSpeed) const = 0;
};

class DefaultCharacterPresentationPolicy final : public CharacterPresentationPolicy {
public:
  CharacterFacing facing(const Character& character, const Camera& camera) const override;
  float framesPerSecond(const Character& character, const SpriteAnimation& animation, float effectiveMovementSpeed, float baseMovementSpeed) const override;
};

CharacterPresentation resolveCharacterPresentation(
  const Character& character,
  const Camera& camera,
  const CharacterPresentationPolicy& policy,
  float effectiveMovementSpeed,
  float baseMovementSpeed
);

void advanceCharacterAnimation(
  Character& character,
  float deltaSeconds,
  const SpriteAnimation* animation,
  float framesPerSecond
);

} // namespace engine
} // namespace isoweb
