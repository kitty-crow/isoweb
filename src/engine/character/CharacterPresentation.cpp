#include "engine/character/CharacterPresentation.hpp"

#include <algorithm>
#include <cmath>

namespace isoweb {
namespace engine {
namespace {

Vec3 horizontal(const Vec3& value) {
  const float magnitude = std::sqrt(value.x * value.x + value.y * value.y);
  return magnitude > 1e-7f
    ? Vec3(value.x / magnitude, value.y / magnitude, 0.0f)
    : Vec3(0.0f, 1.0f, 0.0f);
}

const DirectionalSpriteSet& activeSet(const Character& character) {
  if (!character.activeAction.empty()) {
    const auto action = character.sprites.actions.find(character.activeAction);
    if (action != character.sprites.actions.end()) return action->second;
  }
  return character.moving ? character.sprites.moving : character.sprites.still;
}

} // namespace

CharacterFacing DefaultCharacterPresentationPolicy::facing(const Character& character, const Camera& camera) const {
  const Vec3 characterForward = horizontal(character.forward);
  const Vec3 cameraForward = horizontal(camera.forward());
  const Vec3 towardsCamera = cameraForward * -1.0f;
  const Vec3 screenRight = horizontal(camera.groundRight());
  const float frontBack = dot(characterForward, towardsCamera);
  const float leftRight = dot(characterForward, screenRight);

  if (std::fabs(frontBack) >= std::fabs(leftRight)) {
    return frontBack >= 0.0f ? CharacterFacing::Front : CharacterFacing::Back;
  }
  return leftRight >= 0.0f ? CharacterFacing::Right : CharacterFacing::Left;
}

float DefaultCharacterPresentationPolicy::framesPerSecond(
  const Character& character,
  const SpriteAnimation& animation,
  float effectiveMovementSpeed,
  float baseMovementSpeed
) const {
  if (animation.frameCount <= 1) return 0.0f;
  if (!character.moving || baseMovementSpeed <= 1e-6f) {
    return std::max(0.0f, animation.nominalFramesPerSecond);
  }
  const float speedRatio = std::max(0.05f, effectiveMovementSpeed / baseMovementSpeed);
  return std::max(0.0f, animation.nominalFramesPerSecond * speedRatio);
}

CharacterPresentation resolveCharacterPresentation(
  const Character& character,
  const Camera& camera,
  const CharacterPresentationPolicy& policy,
  float effectiveMovementSpeed,
  float baseMovementSpeed
) {
  CharacterPresentation result;
  result.facing = policy.facing(character, camera);
  const DirectionalSpriteSet& set = activeSet(character);
  result.animation = set.animation(result.facing, &result.mirror);
  if (result.animation && result.animation->frameCount > 0) {
    result.frame = std::min(character.animation.frame, result.animation->frameCount - 1);
  }
  (void)effectiveMovementSpeed;
  (void)baseMovementSpeed;
  return result;
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
