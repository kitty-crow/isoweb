#include "engine/character/CharacterSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace isoweb {
namespace engine {
namespace {

constexpr float BLOCKED_REPLAN_INTERVAL_SECONDS = 0.25f;

float horizontalDistance(const Vec3& a, const Vec3& b) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}

Vec3 horizontalDirection(const Vec3& from, const Vec3& to, const Vec3& fallback) {
  const Vec3 delta(to.x - from.x, to.y - from.y, 0.0f);
  const float magnitudeSquared = delta.x * delta.x + delta.y * delta.y;
  if (magnitudeSquared <= 1e-12f) return fallback;
  return delta * (1.0f / std::sqrt(magnitudeSquared));
}

bool resolveCharacterPosition(
  World& world,
  const Character& character,
  const Vec3& desired,
  float referenceZ,
  const CharacterEngineDefaults& defaults,
  Vec3& resolved,
  bool& crouching
) {
  const auto tryPosture = [&](bool useCrouch) {
    Character probe = character;
    probe.crouching = useCrouch;
    probe.location.position = desired;
    if (!world.resolveWalkablePosition(
      probe,
      character.location.levelId,
      desired,
      referenceZ,
      defaults.maxStepHeight,
      defaults.maxDropHeight,
      resolved,
      &character
    )) {
      return false;
    }
    crouching = useCrouch;
    return true;
  };

  if (tryPosture(false)) return true;
  return character.canCrouch() && tryPosture(true);
}

bool standingFits(World& world, const Character& character) {
  if (!character.crouching) return true;
  Character standing = character;
  standing.crouching = false;
  return !world.collidesWith(standing, &character);
}

bool recoverySegmentClear(
  World& world,
  Character& character,
  const Vec3& destination,
  const CharacterEngineDefaults& defaults,
  Vec3& resolvedEnd
) {
  const Vec3 start = character.location.position;
  const float distance = horizontalDistance(start, destination);
  if (distance <= 1e-6f) {
    bool crouching = false;
    return resolveCharacterPosition(
      world, character, destination, start.z, defaults, resolvedEnd, crouching
    );
  }

  const float sampleDistance = std::max(0.04f, defaults.navigationCellSize * 0.35f);
  const int samples = std::max(1, static_cast<int>(std::ceil(distance / sampleDistance)));
  Vec3 current = start;
  for (int index = 1; index <= samples; ++index) {
    const float t = static_cast<float>(index) / samples;
    Vec3 requested = start * (1.0f - t) + destination * t;
    requested.z = current.z;
    Vec3 supported;
    bool crouching = false;
    if (!resolveCharacterPosition(
      world, character, requested, current.z, defaults, supported, crouching
    )) {
      return false;
    }
    current = supported;
  }

  resolvedEnd = current;
  return true;
}

// Mirror advance() exactly enough to know the Character's eventual facing
// without simulating movement. Level-transition waypoints change coordinate
// frame but do not rotate the Character; only physical horizontal segments
// longer than arrivalEpsilon do.
Vec3 finalRouteDirection(
  const Character& character,
  const CharacterMovementState& movement,
  float arrivalEpsilon
) {
  EntityLocation cursor = character.location;
  Vec3 facing = character.forward;

  for (const CharacterWaypoint& waypoint : movement.route) {
    if (waypoint.levelTransition) {
      cursor = waypoint.location;
      continue;
    }

    if (waypoint.location.levelId != cursor.levelId) {
      cursor = waypoint.location;
      continue;
    }

    if (horizontalDistance(cursor.position, waypoint.location.position) > arrivalEpsilon) {
      facing = horizontalDirection(cursor.position, waypoint.location.position, facing);
    }
    cursor = waypoint.location;
  }
  return facing;
}

Object renderProxy(const Character& character, const Vec3& position, const std::string& levelId) {
  Object proxy;
  proxy.id = character.id;
  proxy.location = character.location;
  proxy.location.levelId = levelId;
  proxy.location.position = position;
  proxy.forward = character.forward;
  proxy.hitBox = character.effectiveHitBox();
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

void applyPresentation(Character& character, const CharacterPresentation& presentation) {
  character.animation.facing = presentation.facing;
  character.animation.mirror = presentation.mirror;
  if (!presentation.animation) {
    character.animation.reset();
    return;
  }
  if (character.animation.resource != presentation.animation->resource) {
    character.animation.reset(presentation.animation->resource);
    character.animation.facing = presentation.facing;
    character.animation.mirror = presentation.mirror;
  }
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

bool CharacterSystem::buildRouteWithRecovery(
  Character& character,
  const EntityLocation& destination,
  CharacterMovementState& route
) {
  route.clear();
  if (navigationPolicy_->buildRoute(
    world_,
    character,
    destination,
    defaults_,
    *levelTransitionPolicy_,
    route
  )) {
    route.pathBlocked = false;
    route.failedPathAttempts = 0;
    route.replanElapsedSeconds = 0.0f;
    route.destinationForward = finalRouteDirection(character, route, defaults_.arrivalEpsilon);
    return true;
  }

  // A planner failure should not force the player to manually provide the
  // missing side-step. Probe a small local ring for a physically reachable
  // point from which the normal planner can complete the original command.
  // This is only a recovery path after a full route search has failed.
  const float cell = std::max(0.08f, defaults_.navigationCellSize);
  const int maximumRecoveryRings = 4;
  const EntityLocation originalLocation = character.location;
  const Vec3 originalForward = character.forward;

  for (int ring = 1; ring <= maximumRecoveryRings; ++ring) {
    bool found = false;
    float bestScore = std::numeric_limits<float>::max();
    Vec3 bestPosition;
    CharacterMovementState bestContinuation;

    for (int x = -ring; x <= ring; ++x) {
      for (int y = -ring; y <= ring; ++y) {
        if (std::max(std::abs(x), std::abs(y)) != ring) continue;

        Vec3 requested = originalLocation.position + Vec3(x * cell, y * cell, 0.0f);
        Vec3 supported;
        character.location = originalLocation;
        character.forward = originalForward;
        if (!recoverySegmentClear(world_, character, requested, defaults_, supported)) continue;

        const Vec3 recoveryDirection = horizontalDirection(
          originalLocation.position,
          supported,
          originalForward
        );
        character.location = originalLocation;
        character.location.position = supported;
        character.location.liminalObjectId = world_.liminalObjectAt(
          character.location.levelId,
          supported,
          std::max(0.18f, defaults_.navigationCellSize * 1.3f)
        );
        character.forward = recoveryDirection;

        CharacterMovementState continuation;
        const bool continuationFound = navigationPolicy_->buildRoute(
          world_,
          character,
          destination,
          defaults_,
          *levelTransitionPolicy_,
          continuation
        );

        character.location = originalLocation;
        character.forward = originalForward;
        if (!continuationFound) continue;

        const float score = horizontalDistance(originalLocation.position, supported) +
          horizontalDistance(supported, destination.position);
        if (score >= bestScore) continue;
        bestScore = score;
        bestPosition = supported;
        bestContinuation = std::move(continuation);
        found = true;
      }
    }

    if (!found) continue;

    route.clear();
    route.destination = destination;
    route.hasDestination = true;
    CharacterWaypoint recovery;
    recovery.location = originalLocation;
    recovery.location.position = bestPosition;
    recovery.location.liminalObjectId = world_.liminalObjectAt(
      recovery.location.levelId,
      bestPosition,
      std::max(0.18f, defaults_.navigationCellSize * 1.3f)
    );
    route.route.push_back(recovery);
    route.route.insert(
      route.route.end(),
      bestContinuation.route.begin(),
      bestContinuation.route.end()
    );
    route.pathBlocked = false;
    route.failedPathAttempts = 0;
    route.replanElapsedSeconds = 0.0f;
    route.destinationForward = finalRouteDirection(character, route, defaults_.arrivalEpsilon);
    return true;
  }

  character.location = originalLocation;
  character.forward = originalForward;
  return false;
}

void CharacterSystem::keepBlockedIntent(
  Character& character,
  const EntityLocation& destination,
  float feedbackElapsedSeconds,
  std::size_t failedAttempts
) {
  CharacterMovementState blocked;
  blocked.hasDestination = true;
  blocked.destination = destination;
  blocked.pathBlocked = true;
  blocked.failedPathAttempts = failedAttempts + 1;
  blocked.replanElapsedSeconds = 0.0f;
  blocked.destinationForward = character.forward;
  blocked.feedbackElapsedSeconds = feedbackElapsedSeconds;
  character.movement = std::move(blocked);
  character.moving = false;
}

bool CharacterSystem::reachedDestination(const Character& character) const {
  if (!character.movement.hasDestination) return false;
  if (character.location.levelId != character.movement.destination.levelId) return false;
  if (
    horizontalDistance(
      character.location.position,
      character.movement.destination.position
    ) > defaults_.arrivalEpsilon * 2.0f
  ) {
    return false;
  }
  const float verticalTolerance = std::max(
    defaults_.arrivalEpsilon * 2.0f,
    std::max(defaults_.maxStepHeight, defaults_.maxDropHeight)
  );
  return std::fabs(
    character.location.position.z - character.movement.destination.position.z
  ) <= verticalTolerance;
}

bool CharacterSystem::command(Character& character, const EntityLocation& requestedDestination) {
  if (!interactionPolicy_->canCommand(character)) return false;
  const EntityLocation destination = destinationPolicy_->resolve(world_, character, requestedDestination, defaults_);
  if (
    destination.levelId.empty() ||
    destination.worldId != character.location.worldId ||
    destination.timelineId != character.location.timelineId
  ) {
    return false;
  }

  CharacterMovementState nextRoute;
  if (buildRouteWithRecovery(character, destination, nextRoute)) {
    nextRoute.feedbackElapsedSeconds = 0.0f;
    character.movement = std::move(nextRoute);
    character.moving = character.movement.hasDestination;
    return true;
  }

  // The command itself is valid even when no route exists right now. Preserve
  // the destination, mark the failed planning attempt, and keep retrying from
  // tick(). A later game layer can interpret pathBlocked as a complaint.
  keepBlockedIntent(character, destination, 0.0f, 0);
  return true;
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
  if (character.crouching && standingFits(world_, character)) character.crouching = false;
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

  if (!character.movement.hasDestination) {
    character.moving = false;
    if (character.crouching && standingFits(world_, character)) character.crouching = false;
    return;
  }

  const float positiveDeltaSeconds = std::max(0.0f, deltaSeconds);
  character.movement.feedbackElapsedSeconds += positiveDeltaSeconds;

  auto replan = [&]() {
    const EntityLocation destination = character.movement.destination;
    const float feedbackElapsedSeconds = character.movement.feedbackElapsedSeconds;
    const std::size_t failedAttempts = character.movement.failedPathAttempts;
    CharacterMovementState replacement;
    if (buildRouteWithRecovery(character, destination, replacement)) {
      replacement.feedbackElapsedSeconds = feedbackElapsedSeconds;
      character.movement = std::move(replacement);
      character.moving = character.movement.hasDestination;
      return true;
    }
    keepBlockedIntent(
      character,
      destination,
      feedbackElapsedSeconds,
      failedAttempts
    );
    return false;
  };

  if (character.movement.nextWaypoint >= character.movement.route.size()) {
    if (reachedDestination(character)) {
      character.location = character.movement.destination;
      refreshLiminalMembership(world_, character, liminalTolerance);
      if (character.crouching && standingFits(world_, character)) character.crouching = false;
      character.movement.clear();
      character.moving = false;
      return;
    }

    character.moving = false;
    character.movement.replanElapsedSeconds += positiveDeltaSeconds;
    if (
      character.movement.pathBlocked &&
      character.movement.replanElapsedSeconds < BLOCKED_REPLAN_INTERVAL_SECONDS
    ) {
      return;
    }
    replan();
    return;
  }

  character.moving = true;
  float remaining = effectiveSpeed(character) * positiveDeltaSeconds;
  // Route planning samples complete segments, but runtime blockers can appear
  // after planning. Bound each physical advance to the same collision sampling
  // scale so high movement multipliers cannot tunnel through a newly occupied
  // interval in a single tick.
  const float maximumCollisionStep = std::max(0.04f, defaults_.navigationCellSize * 0.35f);

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
      bool resolvedCrouching = false;
      if (resolveCharacterPosition(
        world_,
        character,
        target,
        character.location.position.z,
        defaults_,
        supported,
        resolvedCrouching
      )) {
        character.crouching = resolvedCrouching;
        character.location.position = supported;
        refreshLiminalMembership(world_, character, liminalTolerance);
        ++character.movement.nextWaypoint;
        continue;
      }

      replan();
      return;
    }

    character.forward = horizontalDirection(character.location.position, target, character.forward);
    const float step = std::min(std::min(remaining, distance), maximumCollisionStep);
    const Vec3 previous = character.location.position;
    Vec3 proposed = previous + character.forward * step;
    proposed.z = previous.z;

    Vec3 supported;
    bool resolvedCrouching = false;
    if (!resolveCharacterPosition(
      world_,
      character,
      proposed,
      previous.z,
      defaults_,
      supported,
      resolvedCrouching
    )) {
      character.location.position = previous;
      refreshLiminalMembership(world_, character, liminalTolerance);
      replan();
      return;
    }

    character.crouching = resolvedCrouching;
    character.location.position = supported;
    refreshLiminalMembership(world_, character, liminalTolerance);
    remaining -= step;
    if (step + defaults_.arrivalEpsilon >= distance) {
      ++character.movement.nextWaypoint;
    }
  }

  if (character.movement.nextWaypoint >= character.movement.route.size()) {
    if (reachedDestination(character)) {
      character.location = character.movement.destination;
      refreshLiminalMembership(world_, character, liminalTolerance);
      if (character.crouching && standingFits(world_, character)) character.crouching = false;
      character.movement.clear();
      character.moving = false;
      return;
    }
    replan();
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
    applyPresentation(*character, presentation);
  }
}

void CharacterSystem::tick(float deltaSeconds, const Camera& camera) {
  // Movement, camera-relative presentation and animation timing are all
  // Character-local. Resolve presentation once after movement and reuse that
  // exact result for animation instead of making three passes over the store.
  for (Character* character : world_.entities().characters()) {
    if (!character) continue;
    advance(*character, deltaSeconds);

    const CharacterPresentation presentation = resolveCharacterPresentation(
      *character,
      camera,
      *presentationPolicy_
    );
    applyPresentation(*character, presentation);

    if (!presentation.animation || character->animation.resource.empty()) continue;
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