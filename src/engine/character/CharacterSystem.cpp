#include "engine/character/CharacterSystem.hpp"

#include <algorithm>
#include <cmath>

namespace isoweb {
namespace engine {
namespace {

float distance3(const Vec3& a, const Vec3& b) {
  return length(b - a);
}

Vec3 horizontalDirection(const Vec3& from, const Vec3& to, const Vec3& fallback) {
  const Vec3 delta(to.x - from.x, to.y - from.y, 0.0f);
  const float magnitude = std::sqrt(delta.x * delta.x + delta.y * delta.y);
  return magnitude > 1e-6f ? delta / magnitude : fallback;
}

} // namespace

CharacterSystem::CharacterSystem(World& world)
    : world_(world),
      defaultSelectionPolicy_(SelectionMode::Multiple),
      selection_(defaultSelectionPolicy_) {
  collisionPolicy_ = &defaultCollisionPolicy_;
  destinationPolicy_ = &defaultDestinationPolicy_;
  navigationPolicy_ = &defaultNavigationPolicy_;
  movementPolicy_ = &defaultMovementPolicy_;
  interactionPolicy_ = &defaultInteractionPolicy_;
  presentationPolicy_ = &defaultPresentationPolicy_;
  world_.setCharacterSystem(this);
  world_.setCollisionPolicy(collisionPolicy_);
}

Character* CharacterSystem::pick(const Ray& ray, float maximumDistance) const {
  Character* closest = nullptr;
  float closestDistance = maximumDistance;
  for (Character* character : const_cast<EntityStore&>(world_.entities()).characters()) {
    if (!character || character->location.levelId != world_.activeLevelId()) continue;
    ObjectRayHit hit;
    if (character->intersectRay(ray, 0.001f, closestDistance, hit)) {
      closestDistance = hit.distance;
      closest = character;
    }
  }
  return closest;
}

bool CharacterSystem::toggleSelection(const Ray& ray, bool additive) {
  Character* character = pick(ray);
  if (!character) return false;
  return selection_.toggle(*character, additive);
}

bool CharacterSystem::command(Character& character, const EntityLocation& requestedDestination) {
  if (!interactionPolicy_->canCommand(character)) return false;
  const EntityLocation destination = destinationPolicy_->resolve(world_, character, requestedDestination, defaults_);
  if (destination.levelId.empty() || destination.worldId != character.location.worldId || destination.timelineId != character.location.timelineId) return false;

  CharacterMovementState nextRoute;
  if (!navigationPolicy_->buildRoute(world_, character, destination, defaults_, nextRoute)) {
    stop(character);
    return false;
  }
  character.movement = std::move(nextRoute);
  character.moving = character.movement.hasDestination;
  return character.moving;
}

std::size_t CharacterSystem::commandSelected(const EntityLocation& requestedDestination) {
  std::size_t count = 0;
  for (Character* character : selection_.resolve(world_.entities())) {
    if (character && command(*character, requestedDestination)) ++count;
  }
  return count;
}

void CharacterSystem::stop(Character& character) {
  character.movement.clear();
  character.moving = false;
}

float CharacterSystem::effectiveSpeed(const Character& character) const {
  return std::max(0.0f, movementPolicy_->effectiveSpeed(character, defaults_.baseMovementSpeed));
}

void CharacterSystem::advance(Character& character, float deltaSeconds) {
  if (!character.movement.hasDestination || character.movement.nextWaypoint >= character.movement.route.size()) {
    character.moving = false;
    character.movement.clear();
    return;
  }

  float remaining = effectiveSpeed(character) * std::max(0.0f, deltaSeconds);
  character.moving = remaining > 0.0f;

  while (remaining > 0.0f && character.movement.nextWaypoint < character.movement.route.size()) {
    const CharacterWaypoint& waypoint = character.movement.route[character.movement.nextWaypoint];

    if (waypoint.levelTransition && waypoint.location.levelId != character.location.levelId) {
      character.location = waypoint.location;
      ++character.movement.nextWaypoint;
      continue;
    }

    const Vec3 target = waypoint.location.position;
    const float distance = distance3(character.location.position, target);
    if (distance <= defaults_.arrivalEpsilon) {
      character.location.position = target;
      ++character.movement.nextWaypoint;
      continue;
    }

    const Vec3 travel = (target - character.location.position) / distance;
    character.forward = horizontalDirection(character.location.position, target, character.forward);
    const float step = std::min(remaining, distance);
    const Vec3 previous = character.location.position;
    character.location.position = previous + travel * step;

    if (world_.collidesWith(character, &character)) {
      character.location.position = previous;
      const EntityLocation destination = character.movement.destination;
      CharacterMovementState replacement;
      if (navigationPolicy_->buildRoute(world_, character, destination, defaults_, replacement)) {
        character.movement = std::move(replacement);
      } else {
        stop(character);
      }
      return;
    }

    remaining -= step;
    if (step + defaults_.arrivalEpsilon >= distance) {
      character.location.position = target;
      ++character.movement.nextWaypoint;
    }
  }

  if (character.movement.nextWaypoint >= character.movement.route.size()) {
    character.location = character.movement.destination;
    character.movement.clear();
    character.moving = false;
  }
}

const SpriteAnimation* CharacterSystem::currentAnimation(
  const Character& character,
  const Camera& camera,
  bool* mirror
) const {
  const CharacterPresentation presentation = resolveCharacterPresentation(
    character,
    camera,
    *presentationPolicy_,
    effectiveSpeed(character),
    defaults_.baseMovementSpeed
  );
  if (mirror) *mirror = presentation.mirror;
  return presentation.animation;
}

void CharacterSystem::tick(float deltaSeconds, const Camera& camera) {
  for (Character* character : world_.entities().characters()) {
    if (!character) continue;
    advance(*character, deltaSeconds);

    const SpriteAnimation* animation = currentAnimation(*character, camera);
    if (!animation) {
      character->animation.reset();
      continue;
    }
    if (character->animation.resource != animation->resource) {
      character->animation.reset(animation->resource);
    }
    const float fps = presentationPolicy_->framesPerSecond(
      *character,
      *animation,
      effectiveSpeed(*character),
      defaults_.baseMovementSpeed
    );
    advanceCharacterAnimation(*character, deltaSeconds, animation, fps);
  }
}

} // namespace engine
} // namespace isoweb
