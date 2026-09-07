#include "demo/DemoObstacles.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>

namespace isoweb {
namespace demo {
namespace {

using engine::Character;
using engine::EntityLocation;
using engine::HitBox;
using engine::Object;
using engine::ObjectFace;
using engine::Vec3;

constexpr const char* OBSTACLE_TAG = "demo-obstacle";
constexpr const char* NAVIGATION_TAG = "demo-obstacle-navigation";
constexpr const char* LEVEL_ID = "middle";
constexpr const char* WORLD_ID = "demo";
constexpr const char* TIMELINE_ID = "default";

constexpr const char* BARRIER_LEFT_ID = "demo-obstacle-barrier-left";
constexpr const char* BARRIER_RIGHT_ID = "demo-obstacle-barrier-right";
constexpr const char* GUILLOTINE_ID = "demo-obstacle-guillotine";
constexpr const char* BLADE_A_ID = "demo-obstacle-blade-a";
constexpr const char* BLADE_B_ID = "demo-obstacle-blade-b";

constexpr const char* BARRIER_NAV_ID = "demo-obstacle-nav-barrier";
constexpr const char* GUILLOTINE_NAV_ID = "demo-obstacle-nav-guillotine";
constexpr const char* BLADES_NAV_ID = "demo-obstacle-nav-blades";

const Vec3 BARRIER_BASE(1.15f, 1.05f, 0.0f);
const Vec3 GUILLOTINE_BASE(0.15f, -0.05f, 0.0f);
const Vec3 BLADES_BASE(3.15f, 0.35f, 0.0f);

constexpr float BARRIER_SEGMENT_HALF_LENGTH = 0.625f;
constexpr float BARRIER_GAP = 0.80f;
constexpr float BARRIER_SWEEP = 0.55f;
constexpr float BARRIER_SPEED = 1.15f;
constexpr float BARRIER_HALF_THICKNESS = 0.10f;
constexpr float BARRIER_HEIGHT = 1.15f;

constexpr float GUILLOTINE_UP_Z = 2.15f;
constexpr float GUILLOTINE_DOWN_Z = 0.20f;
constexpr float GUILLOTINE_PERIOD = 2.60f;

constexpr float BLADE_ANGULAR_SPEED = 2.35f;
constexpr float PI = 3.14159265358979323846f;
constexpr float CONTACT_TOLERANCE = 0.028f;

HitBox box(const Vec3& minimum, const Vec3& maximum) {
  HitBox result;
  result.minimum = minimum;
  result.maximum = maximum;
  return result;
}

EntityLocation location(const Vec3& position) {
  EntityLocation result;
  result.worldId = WORLD_ID;
  result.timelineId = TIMELINE_ID;
  result.levelId = LEVEL_ID;
  result.position = position;
  return result;
}

Character& addPart(
  engine::World& world,
  const std::string& id,
  const Vec3& position,
  const Vec3& forward,
  const HitBox& hitBox
) {
  std::unique_ptr<Character> obstacle(new Character());
  obstacle->id = id;
  obstacle->location = location(position);
  obstacle->forward = forward;
  obstacle->hitBox = hitBox;
  obstacle->solid = true;
  obstacle->npc = true;
  obstacle->controllable = false;
  obstacle->movementSpeedMultiplier = 0.0f;
  obstacle->collisionTags.push_back(OBSTACLE_TAG);
  Object& stored = world.entities().add(std::move(obstacle));
  return static_cast<Character&>(stored);
}

void addNavigationEnvelope(
  engine::World& world,
  const std::string& id,
  const Vec3& position,
  const HitBox& hitBox
) {
  std::unique_ptr<Object> envelope(new Object());
  envelope->id = id;
  envelope->location = location(position);
  envelope->hitBox = hitBox;
  envelope->solid = true;
  envelope->collisionTags.push_back(NAVIGATION_TAG);
  world.entities().add(std::move(envelope));
}

float overlapAmount(float minimumA, float maximumA, float minimumB, float maximumB) {
  return std::min(maximumA, maximumB) - std::max(minimumA, minimumB);
}

float guillotineHeight(float phase) {
  const float cycle = phase - std::floor(phase);
  if (cycle < 0.34f) return GUILLOTINE_UP_Z;
  if (cycle < 0.49f) {
    const float t = (cycle - 0.34f) / 0.15f;
    const float eased = t * t * (3.0f - 2.0f * t);
    return GUILLOTINE_UP_Z * (1.0f - eased) + GUILLOTINE_DOWN_Z * eased;
  }
  if (cycle < 0.66f) return GUILLOTINE_DOWN_Z;
  const float t = (cycle - 0.66f) / 0.34f;
  const float eased = t * t * (3.0f - 2.0f * t);
  return GUILLOTINE_DOWN_Z * (1.0f - eased) + GUILLOTINE_UP_Z * eased;
}

} // namespace

DemoObstacleSystem::DemoObstacleSystem(engine::World& world)
    : world_(world) {}

bool DemoObstacleSystem::isObstacleCharacter(const Character& character) const {
  return character.hasCollisionTag(OBSTACLE_TAG);
}

void DemoObstacleSystem::setEnabled(bool enabled) {
  if (enabled == enabled_) return;
  remove();
  enabled_ = enabled;
  barrierPhase_ = 0.0f;
  guillotinePhase_ = 0.0f;
  bladePhase_ = 0.0f;
  if (enabled_) spawn();
}

void DemoObstacleSystem::spawn() {
  partIds_.clear();
  navigationIds_.clear();

  const float barrierOffset = BARRIER_GAP * 0.5f + BARRIER_SEGMENT_HALF_LENGTH;
  const HitBox barrierBox = box(
    {-BARRIER_SEGMENT_HALF_LENGTH, -BARRIER_HALF_THICKNESS, 0.0f},
    { BARRIER_SEGMENT_HALF_LENGTH,  BARRIER_HALF_THICKNESS, BARRIER_HEIGHT}
  );

  addPart(
    world_,
    BARRIER_LEFT_ID,
    BARRIER_BASE + Vec3(-barrierOffset, 0.0f, 0.0f),
    {0.0f, 1.0f, 0.0f},
    barrierBox
  );
  addPart(
    world_,
    BARRIER_RIGHT_ID,
    BARRIER_BASE + Vec3(barrierOffset, 0.0f, 0.0f),
    {0.0f, 1.0f, 0.0f},
    barrierBox
  );
  partIds_.push_back(BARRIER_LEFT_ID);
  partIds_.push_back(BARRIER_RIGHT_ID);

  addPart(
    world_,
    GUILLOTINE_ID,
    GUILLOTINE_BASE + Vec3(0.0f, 0.0f, GUILLOTINE_UP_Z),
    {0.0f, 1.0f, 0.0f},
    box({-0.72f, -0.11f, -0.11f}, {0.72f, 0.11f, 0.11f})
  );
  partIds_.push_back(GUILLOTINE_ID);

  addPart(
    world_,
    BLADE_A_ID,
    BLADES_BASE,
    {0.0f, 1.0f, 0.0f},
    box({-0.11f, 0.16f, 0.08f}, {0.11f, 1.08f, 0.28f})
  );
  addPart(
    world_,
    BLADE_B_ID,
    BLADES_BASE,
    {0.0f, -1.0f, 0.0f},
    box({-0.11f, 0.16f, 0.08f}, {0.11f, 1.08f, 0.28f})
  );
  partIds_.push_back(BLADE_A_ID);
  partIds_.push_back(BLADE_B_ID);

  addNavigationEnvelope(
    world_,
    BARRIER_NAV_ID,
    BARRIER_BASE,
    box({-2.25f, -0.32f, 0.0f}, {2.25f, 0.32f, 1.70f})
  );
  addNavigationEnvelope(
    world_,
    GUILLOTINE_NAV_ID,
    GUILLOTINE_BASE,
    box({-0.90f, -0.42f, 0.0f}, {0.90f, 0.42f, 1.70f})
  );
  addNavigationEnvelope(
    world_,
    BLADES_NAV_ID,
    BLADES_BASE,
    box({-1.25f, -1.25f, 0.0f}, {1.25f, 1.25f, 1.70f})
  );
  navigationIds_.push_back(BARRIER_NAV_ID);
  navigationIds_.push_back(GUILLOTINE_NAV_ID);
  navigationIds_.push_back(BLADES_NAV_ID);
}

void DemoObstacleSystem::remove() {
  for (const std::string& id : partIds_) world_.entities().remove(id);
  for (const std::string& id : navigationIds_) world_.entities().remove(id);
  partIds_.clear();
  navigationIds_.clear();
}

Character* DemoObstacleSystem::part(const std::string& id) {
  return dynamic_cast<Character*>(world_.entities().find(id));
}

const Character* DemoObstacleSystem::part(const std::string& id) const {
  return dynamic_cast<const Character*>(world_.entities().find(id));
}

DemoObstacleSystem::LocalBounds DemoObstacleSystem::boundsInLocalSpace(
  const Object& reference,
  const Object& other
) {
  LocalBounds result;
  const float maximum = std::numeric_limits<float>::max();
  result.minimum = {maximum, maximum, maximum};
  result.maximum = {-maximum, -maximum, -maximum};

  const Vec3 minimum = other.hitBox.minimum;
  const Vec3 maximumPoint = other.hitBox.maximum;
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      for (int z = 0; z < 2; ++z) {
        const Vec3 local(
          x ? maximumPoint.x : minimum.x,
          y ? maximumPoint.y : minimum.y,
          z ? maximumPoint.z : minimum.z
        );
        const Vec3 point = reference.worldToLocal(other.localToWorld(local));
        result.minimum.x = std::min(result.minimum.x, point.x);
        result.minimum.y = std::min(result.minimum.y, point.y);
        result.minimum.z = std::min(result.minimum.z, point.z);
        result.maximum.x = std::max(result.maximum.x, point.x);
        result.maximum.y = std::max(result.maximum.y, point.y);
        result.maximum.z = std::max(result.maximum.z, point.z);
      }
    }
  }
  return result;
}

bool DemoObstacleSystem::touchesFace(
  const Object& obstacle,
  const Object& other,
  ObjectFace face,
  float tolerance
) {
  if (!obstacle.location.sharesSpaceWith(other.location)) return false;

  const LocalBounds local = boundsInLocalSpace(obstacle, other);
  const HitBox& box = obstacle.hitBox;
  const Vec3 centre = local.minimum + (local.maximum - local.minimum) * 0.5f;
  const Vec3 obstacleCentre = box.centre();

  const float overlapX = overlapAmount(
    local.minimum.x - tolerance,
    local.maximum.x + tolerance,
    box.minimum.x,
    box.maximum.x
  );
  const float overlapY = overlapAmount(
    local.minimum.y - tolerance,
    local.maximum.y + tolerance,
    box.minimum.y,
    box.maximum.y
  );
  const float overlapZ = overlapAmount(
    local.minimum.z - tolerance,
    local.maximum.z + tolerance,
    box.minimum.z,
    box.maximum.z
  );

  switch (face) {
    case ObjectFace::Left:
      return centre.x <= obstacleCentre.x && overlapY > 0.0f && overlapZ > 0.0f &&
        local.maximum.x >= box.minimum.x - tolerance &&
        local.minimum.x <= box.minimum.x + tolerance;
    case ObjectFace::Right:
      return centre.x >= obstacleCentre.x && overlapY > 0.0f && overlapZ > 0.0f &&
        local.minimum.x <= box.maximum.x + tolerance &&
        local.maximum.x >= box.maximum.x - tolerance;
    case ObjectFace::Back:
      return centre.y <= obstacleCentre.y && overlapX > 0.0f && overlapZ > 0.0f &&
        local.maximum.y >= box.minimum.y - tolerance &&
        local.minimum.y <= box.minimum.y + tolerance;
    case ObjectFace::Front:
      return centre.y >= obstacleCentre.y && overlapX > 0.0f && overlapZ > 0.0f &&
        local.minimum.y <= box.maximum.y + tolerance &&
        local.maximum.y >= box.maximum.y - tolerance;
    case ObjectFace::Bottom:
      return centre.z <= obstacleCentre.z && overlapX > 0.0f && overlapY > 0.0f &&
        local.maximum.z >= box.minimum.z - tolerance &&
        local.minimum.z <= box.minimum.z + tolerance;
    case ObjectFace::Top:
      return centre.z >= obstacleCentre.z && overlapX > 0.0f && overlapY > 0.0f &&
        local.minimum.z <= box.maximum.z + tolerance &&
        local.maximum.z >= box.maximum.z - tolerance;
  }
  return false;
}

bool DemoObstacleSystem::touchesAnyFace(
  const Object& obstacle,
  const Object& other,
  float tolerance
) {
  return touchesFace(obstacle, other, ObjectFace::Left, tolerance) ||
    touchesFace(obstacle, other, ObjectFace::Right, tolerance) ||
    touchesFace(obstacle, other, ObjectFace::Back, tolerance) ||
    touchesFace(obstacle, other, ObjectFace::Front, tolerance) ||
    touchesFace(obstacle, other, ObjectFace::Bottom, tolerance) ||
    touchesFace(obstacle, other, ObjectFace::Top, tolerance);
}

void DemoObstacleSystem::updateBarrier(float deltaSeconds) {
  Character* left = part(BARRIER_LEFT_ID);
  Character* right = part(BARRIER_RIGHT_ID);
  if (!left || !right) return;

  const float previousPhase = barrierPhase_;
  const Vec3 previousLeft = left->location.position;
  const Vec3 previousRight = right->location.position;
  barrierPhase_ += std::max(0.0f, deltaSeconds) * BARRIER_SPEED;

  const float offset = std::sin(barrierPhase_) * BARRIER_SWEEP;
  const float segmentOffset = BARRIER_GAP * 0.5f + BARRIER_SEGMENT_HALF_LENGTH;
  left->location.position = BARRIER_BASE + Vec3(offset - segmentOffset, 0.0f, 0.0f);
  right->location.position = BARRIER_BASE + Vec3(offset + segmentOffset, 0.0f, 0.0f);

  bool blockedBySafeContact = false;
  for (const Character* character : world_.entities().characters()) {
    if (!character || isObstacleCharacter(*character)) continue;
    if (
      left->overlaps(*character) &&
      !touchesFace(*left, *character, ObjectFace::Right, CONTACT_TOLERANCE)
    ) {
      blockedBySafeContact = true;
    }
    if (
      right->overlaps(*character) &&
      !touchesFace(*right, *character, ObjectFace::Left, CONTACT_TOLERANCE)
    ) {
      blockedBySafeContact = true;
    }
  }

  if (blockedBySafeContact) {
    barrierPhase_ = previousPhase;
    left->location.position = previousLeft;
    right->location.position = previousRight;
  }
}

void DemoObstacleSystem::updateGuillotine(float deltaSeconds) {
  Character* blade = part(GUILLOTINE_ID);
  if (!blade) return;

  const float previousPhase = guillotinePhase_;
  const Vec3 previousPosition = blade->location.position;
  guillotinePhase_ += std::max(0.0f, deltaSeconds) / GUILLOTINE_PERIOD;
  blade->location.position = GUILLOTINE_BASE + Vec3(
    0.0f,
    0.0f,
    guillotineHeight(guillotinePhase_)
  );

  bool blockedBySafeContact = false;
  for (const Character* character : world_.entities().characters()) {
    if (!character || isObstacleCharacter(*character)) continue;
    if (
      blade->overlaps(*character) &&
      !touchesFace(*blade, *character, ObjectFace::Bottom, CONTACT_TOLERANCE)
    ) {
      blockedBySafeContact = true;
    }
  }

  if (blockedBySafeContact) {
    guillotinePhase_ = previousPhase;
    blade->location.position = previousPosition;
  }
}

void DemoObstacleSystem::updateBlades(float deltaSeconds) {
  Character* a = part(BLADE_A_ID);
  Character* b = part(BLADE_B_ID);
  if (!a || !b) return;

  bladePhase_ += std::max(0.0f, deltaSeconds) * BLADE_ANGULAR_SPEED;
  if (bladePhase_ > PI * 2.0f) bladePhase_ = std::fmod(bladePhase_, PI * 2.0f);

  const Vec3 forward(std::sin(bladePhase_), std::cos(bladePhase_), 0.0f);
  a->forward = forward;
  b->forward = forward * -1.0f;
}

std::vector<std::string> DemoObstacleSystem::hazardContacts() const {
  std::set<std::string> hurt;
  const Character* barrierLeft = part(BARRIER_LEFT_ID);
  const Character* barrierRight = part(BARRIER_RIGHT_ID);
  const Character* guillotine = part(GUILLOTINE_ID);
  const Character* bladeA = part(BLADE_A_ID);
  const Character* bladeB = part(BLADE_B_ID);

  for (const Character* character : world_.entities().characters()) {
    if (!character || isObstacleCharacter(*character)) continue;

    if (
      barrierLeft &&
      touchesFace(*barrierLeft, *character, ObjectFace::Right, CONTACT_TOLERANCE)
    ) {
      hurt.insert(character->id);
    }
    if (
      barrierRight &&
      touchesFace(*barrierRight, *character, ObjectFace::Left, CONTACT_TOLERANCE)
    ) {
      hurt.insert(character->id);
    }
    if (
      guillotine &&
      touchesFace(*guillotine, *character, ObjectFace::Bottom, CONTACT_TOLERANCE)
    ) {
      hurt.insert(character->id);
    }
    if (
      (bladeA && touchesAnyFace(*bladeA, *character, CONTACT_TOLERANCE)) ||
      (bladeB && touchesAnyFace(*bladeB, *character, CONTACT_TOLERANCE))
    ) {
      hurt.insert(character->id);
    }
  }

  return std::vector<std::string>(hurt.begin(), hurt.end());
}

std::vector<std::string> DemoObstacleSystem::tick(float deltaSeconds) {
  if (!enabled_) return {};
  updateBarrier(deltaSeconds);
  updateGuillotine(deltaSeconds);
  updateBlades(deltaSeconds);
  return hazardContacts();
}

} // namespace demo
} // namespace isoweb
