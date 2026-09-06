#include "engine/character/CharacterAnimation.hpp"

#include <algorithm>

namespace isoweb {
namespace engine {

float DefaultCharacterAnimationPolicy::framesPerSecond(
  const Character& character,
  const SpriteAnimation& animation,
  float effectiveMovementSpeed,
  float baseMovementSpeed
) const {
  if (animation.frameCount <= 1) return 0.0f;

  // Still and action artwork may itself be animated (breathing, blinking,
  // casting, etc.). Those sequences use their nominal rate while stationary.
  if (!character.moving || baseMovementSpeed <= 1e-6f) {
    return std::max(0.0f, animation.nominalFramesPerSecond);
  }

  // Moving artwork is engine-timed rather than animated-WebP timed. Scale the
  // authored nominal cadence with actual effective movement speed so footfall
  // playback remains visually tied to travel speed.
  const float speedRatio = std::max(0.05f, effectiveMovementSpeed / baseMovementSpeed);
  return std::max(0.0f, animation.nominalFramesPerSecond * speedRatio);
}

void advanceCharacterAnimation(
  Character& character,
  float deltaSeconds,
  const SpriteAnimation* animation,
  float framesPerSecond
) {
  if (!animation || animation->frameCount <= 1 || framesPerSecond <= 0.0f) {
    character.animation.frame = 0;
    character.animation.elapsedSeconds = 0.0f;
    return;
  }

  const float secondsPerFrame = 1.0f / framesPerSecond;
  character.animation.elapsedSeconds += std::max(0.0f, deltaSeconds);
  while (character.animation.elapsedSeconds >= secondsPerFrame) {
    character.animation.elapsedSeconds -= secondsPerFrame;
    if (character.animation.frame + 1 < animation->frameCount) {
      ++character.animation.frame;
    } else if (animation->loop) {
      character.animation.frame = 0;
    } else {
      character.animation.frame = animation->frameCount - 1;
      character.animation.elapsedSeconds = 0.0f;
      break;
    }
  }
}

} // namespace engine
} // namespace isoweb
