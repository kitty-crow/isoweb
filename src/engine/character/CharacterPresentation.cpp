#include "engine/character/CharacterPresentation.hpp"

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

CharacterFacing DefaultCharacterPresentationPolicy::facing(
  const Character& character,
  const Camera& camera
) const {
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

CharacterPresentation resolveCharacterPresentation(
  const Character& character,
  const Camera& camera,
  const CharacterPresentationPolicy& policy
) {
  CharacterPresentation result;
  result.facing = policy.facing(character, camera);
  const DirectionalSpriteSet& set = activeSet(character);
  result.animation = set.animation(result.facing, &result.mirror);
  if (result.animation && result.animation->frameCount > 0) {
    result.frame = std::min(character.animation.frame, result.animation->frameCount - 1);
  }
  return result;
}

} // namespace engine
} // namespace isoweb
