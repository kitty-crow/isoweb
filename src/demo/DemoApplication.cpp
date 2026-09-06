#include "demo/DemoApplication.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace isoweb {
namespace demo {
namespace {

constexpr int STAIR_STEPS = 7;
constexpr float STAIR_RISE = 1.40f;
constexpr float STAIR_LOW_Y = -3.30f;
constexpr float STAIR_HIGH_Y = -1.20f;
constexpr float LOWER_MIDDLE_STAIR_X = 2.15f;
constexpr float MIDDLE_UPPER_STAIR_X = 3.20f;

std::vector<engine::Vec3> ascendingStair(float x) {
  std::vector<engine::Vec3> result;
  for (int step = 1; step <= STAIR_STEPS; ++step) {
    const float t = static_cast<float>(step) / STAIR_STEPS;
    result.push_back({x, STAIR_LOW_Y + (STAIR_HIGH_Y - STAIR_LOW_Y) * t, STAIR_RISE * t});
  }
  return result;
}

std::vector<engine::Vec3> descendingStair(float x) {
  std::vector<engine::Vec3> result;
  for (int step = 1; step <= STAIR_STEPS; ++step) {
    const float t = static_cast<float>(step) / STAIR_STEPS;
    result.push_back({x, STAIR_HIGH_Y + (STAIR_LOW_Y - STAIR_HIGH_Y) * t, -STAIR_RISE * t});
  }
  return result;
}

engine::DirectionalSpriteSet* spriteSet(engine::Character& character, int state, const std::string& action) {
  if (state == 0) return &character.sprites.still;
  if (state == 1) return &character.sprites.moving;
  if (state == 2 && !action.empty()) return &character.sprites.actions[action];
  return nullptr;
}

engine::SpriteAnimation* directionalAnimation(engine::DirectionalSpriteSet& set, engine::CharacterFacing facing) {
  switch (facing) {
    case engine::CharacterFacing::Front: return &set.front;
    case engine::CharacterFacing::Back: return &set.back;
    case engine::CharacterFacing::Left: return &set.left;
    case engine::CharacterFacing::Right: return &set.right;
  }
  return nullptr;
}

} // namespace

DemoApplication::DemoApplication()
    : camera_(engine::CameraConfig(3.25f, 6.15f, 5.50f)),
      renderer_(world_, camera_, controls_),
      characters_(world_) {
  configureDemoWorldNavigation();
}

void DemoApplication::configureDemoWorldNavigation() {
  world_.setLevelId(0, "lower");
  world_.setLevelId(1, "middle");
  world_.setLevelId(2, "upper");

  engine::NavigationLink lowerMiddle;
  lowerMiddle.fromLevelId = "lower";
  lowerMiddle.toLevelId = "middle";
  lowerMiddle.fromPosition = {LOWER_MIDDLE_STAIR_X, STAIR_LOW_Y, 0.0f};
  lowerMiddle.toPosition = {LOWER_MIDDLE_STAIR_X, STAIR_HIGH_Y, 0.0f};
  lowerMiddle.forwardTraversal = ascendingStair(LOWER_MIDDLE_STAIR_X);
  lowerMiddle.reverseTraversal = descendingStair(LOWER_MIDDLE_STAIR_X);

  engine::NavigationLink middleUpper;
  middleUpper.fromLevelId = "middle";
  middleUpper.toLevelId = "upper";
  middleUpper.fromPosition = {MIDDLE_UPPER_STAIR_X, STAIR_LOW_Y, 0.0f};
  middleUpper.toPosition = {MIDDLE_UPPER_STAIR_X, STAIR_HIGH_Y, 0.0f};
  middleUpper.forwardTraversal = ascendingStair(MIDDLE_UPPER_STAIR_X);
  middleUpper.reverseTraversal = descendingStair(MIDDLE_UPPER_STAIR_X);

  world_.setNavigationLinks({lowerMiddle, middleUpper});
}

void DemoApplication::redraw() {
  characters_.updatePresentation(camera_);
  renderer_.render();
  presenter_.present(
    renderer_.rgba(),
    renderer_.width(),
    renderer_.height(),
    camera_,
    renderer_.cameraControlState(),
    renderer_.canPan(),
    renderer_.viewHeight(),
    renderer_.wholeZoomScale()
  );
}

void DemoApplication::render() {
  redraw();
}

void DemoApplication::tick(float deltaSeconds) {
  characters_.tick(std::max(0.0f, std::min(0.10f, deltaSeconds)), camera_);
  redraw();
}

void DemoApplication::resize(int width, int height) {
  renderer_.resize(width, height);
  redraw();
}

void DemoApplication::rotateClockwise() {
  camera_.rotateClockwise();
  redraw();
}

void DemoApplication::rotateCounterClockwise() {
  camera_.rotateCounterClockwise();
  redraw();
}

void DemoApplication::resetYaw() {
  camera_.resetYaw();
  redraw();
}

void DemoApplication::setDetailedYawMode(bool enabled) {
  camera_.setDetailedYawMode(enabled);
  redraw();
}

void DemoApplication::zoomIn() {
  camera_.stepZoom(1, renderer_.width(), renderer_.height(), world_.bounds());
  redraw();
}

void DemoApplication::zoomOut() {
  camera_.stepZoom(-1, renderer_.width(), renderer_.height(), world_.bounds());
  redraw();
}

void DemoApplication::resetZoom() {
  camera_.resetZoom();
  redraw();
}

void DemoApplication::setDetailedMode(bool enabled) {
  camera_.setDetailedMode(enabled);
  redraw();
}

void DemoApplication::pan(float right, float down) {
  camera_.pan(right, down, renderer_.width(), renderer_.height(), world_.bounds());
  redraw();
}

void DemoApplication::resetCamera() {
  camera_.resetPan();
  redraw();
}

void DemoApplication::levelUp() {
  if (world_.levelUp()) redraw();
}

void DemoApplication::levelDown() {
  if (world_.levelDown()) redraw();
}

void DemoApplication::resetLevel() {
  if (world_.resetLevel()) redraw();
}

bool DemoApplication::pointerTap(float x, float y, bool additive) {
  const engine::Ray ray = renderer_.rayForPixel(x, y);
  if (engine::Character* hit = characters_.pick(ray)) {
    if (hit->moving && characters_.selection().selected(hit->id)) {
      movementCommandArmed_ = true;
    } else {
      characters_.selection().toggle(*hit, additive);
      movementCommandArmed_ = characters_.selection().selected(hit->id);
    }
    redraw();
    return true;
  }

  if (!movementCommandArmed_ || characters_.selection().ids().empty()) return false;

  engine::Vec3 destinationPoint;
  if (!renderer_.groundPointForPixel(x, y, 0.0f, destinationPoint)) return false;
  if (!world_.containsPosition(world_.activeLevelId(), destinationPoint)) return false;

  engine::EntityLocation destination;
  destination.levelId = world_.activeLevelId();
  destination.position = destinationPoint;
  const std::size_t commanded = characters_.commandSelected(destination);
  movementCommandArmed_ = false;
  if (commanded > 0) redraw();
  return commanded > 0;
}

void DemoApplication::clearSelection() {
  characters_.clearSelection();
  movementCommandArmed_ = false;
  redraw();
}

bool DemoApplication::clearEntities() {
  characters_.clearSelection();
  world_.entities().clear();
  movementCommandArmed_ = false;
  redraw();
  return true;
}

bool DemoApplication::createCharacter(
  const std::string& id,
  const engine::EntityLocation& location,
  const engine::Vec3& forward,
  const engine::HitBox& hitBox,
  bool solid,
  bool npc,
  bool controllable,
  float movementSpeedMultiplier
) {
  if (id.empty() || world_.entities().find(id)) return false;
  std::unique_ptr<engine::Character> character(new engine::Character());
  character->id = id;
  character->location = location;
  character->forward = forward;
  character->hitBox = hitBox;
  character->solid = solid;
  character->npc = npc;
  character->controllable = controllable;
  character->movementSpeedMultiplier = movementSpeedMultiplier;
  world_.entities().add(std::move(character));
  redraw();
  return true;
}

engine::Character* DemoApplication::character(const std::string& id) {
  return dynamic_cast<engine::Character*>(world_.entities().find(id));
}

bool DemoApplication::setCharacterSprite(
  const std::string& id,
  int state,
  const std::string& action,
  engine::CharacterFacing facing,
  const engine::SpriteAnimation& animation
) {
  engine::Character* target = character(id);
  if (!target) return false;
  engine::DirectionalSpriteSet* set = spriteSet(*target, state, action);
  if (!set) return false;
  engine::SpriteAnimation* destination = directionalAnimation(*set, facing);
  if (!destination) return false;
  *destination = animation;
  redraw();
  return true;
}

bool DemoApplication::setCharacterAction(const std::string& id, const std::string& action) {
  engine::Character* target = character(id);
  if (!target) return false;
  target->activeAction = action;
  target->animation.reset();
  redraw();
  return true;
}

bool DemoApplication::registerSpriteAtlas(
  const std::string& resource,
  int width,
  int height,
  const std::uint8_t* rgba,
  std::size_t byteCount
) {
  const bool registered = world_.spriteAtlases().registerRgba(resource, width, height, rgba, byteCount);
  if (registered) redraw();
  return registered;
}

void DemoApplication::setSelectionMode(engine::SelectionMode mode) {
  characters_.setSelectionMode(mode);
  movementCommandArmed_ = false;
  redraw();
}

} // namespace demo
} // namespace isoweb
