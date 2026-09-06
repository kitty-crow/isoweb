#include "engine/character/CharacterSystem.hpp"

#include <algorithm>
#include <cmath>

namespace isoweb {
namespace engine {
namespace {

float horizontalDistance(const Vec3& a, const Vec3& b) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}

Vec3 horizontalDirection(const Vec3& from, const Vec3& to, const Vec3& fallback) {
  const Vec3 delta(to.x - from.x, to.y - from.y, 0.0f);
  const float magnitude = std::sqrt(delta.x * delta.x + delta.y * delta.y);
  return magnitude > 1e-6f ? delta / magnitude : fallback;
}

Object renderProxy(const Character& character, const Vec3& position, const std::string& levelId) {
  Object proxy;
  proxy.id = character.id;
  proxy.location = character.location;
  proxy.location.levelId = levelId;
  proxy.location.position = position;
  proxy.forward = character.forward;
  proxy.hitBox = character.hitBox;
  proxy.solid = character.solid;
  return proxy;
}

void refreshLiminalMembership(World& world, Character& character, float tolerance) {
  character.location.liminalObjectId = world.liminalObjectAt(
    character.location.levelId,
    character.location.position,
    tolerance
  );
}

} // namespace

CharacterSystem::CharacterSystem(World& world)
    : world_(world),
      defaultSelectionPolicy_(SelectionMode::Multiple),
      selection_(defaultSelectionPolicy_) {
  collisionPolicy_ = &defaultCollisionPolicy_;
  destinationPolicy_ = &defaultDestinationPolicy_;
  navigationPolicy_ = &defaultNavigationPolicy_;
  levelTransitionPolicy_ = &defaultLevelTransitionPolicy_;
  movementPolicy_ = &defaultMovementPolicy_;
  interactionPolicy_ = &defaultInteractionPolicy_;
  presentationPolicy_ = &defaultPresentationPolicy_;
  animationPolicy_ = &defaultAnimationPolicy_;
  world_.setCharacterSystem(this);
  world_.setCollisionPolicy(collisionPolicy_);
}

Character* CharacterSystem::pick(const Ray& ray, float maximumDistance) const {
  Character* closest = nullptr;
  const float environmentDistance = world_.environmentDistance(ray);
  float closestDistance = std::min(maximumDistance, environmentDistance);
  for (Character* character : const_cast<EntityStore&>(world_.entities()).characters()) {
    if (!character) continue;
    Vec3 renderPosition;
    if (!world_.renderPositionFor(*character, renderPosition)) continue;
    const Object proxy = renderProxy(*character, renderPosition, world_.activeLevelId());
    ObjectRayHit hit;
    if (proxy.intersectRay(ray, 0.001f, closestDistance, hit)) {
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
  if (!navigationPolicy_->buildRoute(
    world_,
    character,
    destination,
    defaults_,
    *levelTransitionPolicy_,
    nextRoute
  )) {
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

bool CharacterSystem::needsTick() const {
  for (const Character* character : world_.entities().characters()) {
    if (!character) continue;
    if (character->moving || character->movement.hasDestination) return true;
    bool mirror = false;
    const SpriteAnimation* animation = character->currentSpriteAnimation(&mirror);
    if (animation && animation->animated()) return true;
  }
  return false;
}

void CharacterSystem::advance(Character& character, float deltaSeconds) {
  const float liminalTolerance = std::max(0.18f, defaults_.navigationCellSize * 1.3f);
  refreshLiminalMembership(world_, character, liminalTolerance);

  if (!character.movement.hasDestination || character.movement.nextWaypoint >= character.movement.route.size()) {
    character.moving = false;
    character.movement.clear();
    return;
  }

  character.moving = true;
  float remaining = effectiveSpeed(character) * std::max(0.0f, deltaSeconds);

  while (remaining > 0.0f && character.movement.nextWaypoint < character.movement.route.size()) {
    const CharacterWaypoint& waypoint = character.movement.route[character.movement.nextWaypoint];

    if (waypoint.levelTransition && waypoint.location.levelId != character.location.levelId) {
      character.location = waypoint.location;
      refreshLiminalMembership(world_, character, liminalTolerance);
      ++character.movement.nextWaypoint;
      continue;
    }

    const Vec3 target = waypoint.location.position;
    const float distance = horizontalDistance(character.location.position, target);
    if (distance <= defaults_.arrivalEpsilon) {
      Vec3 supported;
      if (world_.resolveWalkablePosition(
        character,
        character.location.levelId,
        target,
        character.location.position.z,
        defaults_.maxStepHeight,
        defaults_.maxDropHeight,
        supported
      )) {
        character.location.position = supported;
        refreshLiminalMembership(world_, character, liminalTolerance);
        ++character.movement.nextWaypoint;
        continue;
      }

      const EntityLocation destination = character.movement.destination;
      CharacterMovementState replacement;
      if (navigationPolicy_->buildRoute(
        world_,
        character,
        destination,
        defaults_,
        *levelTransitionPolicy_,
        replacement
      )) {
        character.movement = std::move(replacement);
      } else {
        stop(character);
      }
      return;
    }

    character.forward = horizontalDirection(character.location.position, target, character.forward);
    const float step = std::min(remaining, distance);
    const Vec3 previous = character.location.position;
    Vec3 proposed = previous + character.forward * step;
    proposed.z = previous.z;

    Vec3 supported;
    if (!world_.resolveWalkablePosition(
      character,
      character.location.levelId,
      proposed,
      previous.z,
      defaults_.maxStepHeight,
      defaults_.maxDropHeight,
      supported
    )) {
      character.location.position = previous;
      refreshLiminalMembership(world_, character, liminalTolerance);
      const EntityLocation destination = character.movement.destination;
      CharacterMovementState replacement;
      if (navigationPolicy_->buildRoute(
        world_,
        character,
        destination,
        defaults_,
        *levelTransitionPolicy_,
        replacement
      )) {
        character.movement = std::move(replacement);
      } else {
        stop(character);
      }
      return;
    }

    character.location.position = supported;
    refreshLiminalMembership(world_, character, liminalTolerance);
    remaining -= step;
    if (step + defaults_.arrivalEpsilon >= distance) {
      ++character.movement.nextWaypoint;
    }
  }

  if (character.movement.nextWaypoint >= character.movement.route.size()) {
    character.location = character.movement.destination;
    refreshLiminalMembership(world_, character, liminalTolerance);
    character.movement.clear();
    character.moving = false;
  }
}

void CharacterSystem::updatePresentation(const Camera& camera) {
  for (Character* character : world_.entities().characters()) {
    if (!character) continue;
    const CharacterPresentation presentation = resolveCharacterPresentation(
      *character,
      camera,
      *presentationPolicy_
    );
    character->animation.facing = presentation.facing;
    character->animation.mirror = presentation.mirror;
    if (!presentation.animation) {
      character->animation.reset();
      continue;
    }
    if (character->animation.resource != presentation.animation->resource) {
      character->animation.reset(presentation.animation->resource);
      character->animation.facing = presentation.facing;
      character->animation.mirror = presentation.mirror;
    }
  }
}

void CharacterSystem::tick(float deltaSeconds, const Camera& camera) {
  for (Character* character : world_.entities().characters()) {
    if (character) advance(*character, deltaSeconds);
  }

  updatePresentation(camera);

  for (Character* character : world_.entities().characters()) {
    if (!character || character->animation.resource.empty()) continue;
    const CharacterPresentation presentation = resolveCharacterPresentation(
      *character,
      camera,
      *presentationPolicy_
    );
    if (!presentation.animation) continue;
    const float fps = animationPolicy_->framesPerSecond(
      *character,
      *presentation.animation,
      effectiveSpeed(*character),
      defaults_.baseMovementSpeed
    );
    advanceCharacterAnimation(*character, deltaSeconds, presentation.animation, fps);
  }
}

} // namespace engine
} // namespace isoweb
