#include "demo/DemoApplication.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace isoweb {
namespace demo {
namespace {

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
  world_.setLevelLight("lower", {4.20f, -3.20f, 5.60f});
  world_.setLevelLight("middle", {-3.60f, -4.20f, 6.50f});
  world_.setLevelLight("upper", {3.80f, 4.40f, 7.20f});
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

void DemoApplication::setControlStick(int control, float x, float y) {
  if (control < 0 || control > 3) return;
  controls_.setStickOffset(static_cast<engine::ControlStick>(control), x, y);
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
    characters_.selection().toggle(*hit, additive);
    redraw();
    return true;
  }

  if (characters_.selection().ids().empty()) return false;

  engine::SceneSurfaceHit surface;
  if (!world_.pickWalkableSurface(ray, surface)) return false;

  engine::EntityLocation destination;
  destination.levelId = world_.activeLevelId();
  destination.position = surface.point;
  const std::size_t commanded = characters_.commandSelected(destination);
  if (commanded > 0) redraw();
  return commanded > 0;
}

bool DemoApplication::pointerWalkable(float x, float y) const {
  const engine::Ray ray = renderer_.rayForPixel(x, y);
  if (characters_.pick(ray)) return false;
  engine::SceneSurfaceHit surface;
  return world_.pickWalkableSurface(ray, surface);
}

std::size_t DemoApplication::dragSelect(float x0, float y0, float x1, float y1, bool additive) {
  const float minimumX = std::min(x0, x1);
  const float maximumX = std::max(x0, x1);
  const float minimumY = std::min(y0, y1);
  const float maximumY = std::max(y0, y1);
  if (!additive) characters_.clearSelection();

  std::size_t count = 0;
  for (engine::Character* character : world_.entities().characters()) {
    if (!character) continue;
    engine::Vec3 renderPosition;
    if (!world_.renderPositionFor(*character, renderPosition)) continue;
    const engine::Vec3 localCentre = character->hitBox.centre();
    const engine::Vec3 centre = renderPosition +
      character->horizontalRight() * localCentre.x +
      character->horizontalForward() * localCentre.y +
      engine::Vec3(0.0f, 0.0f, localCentre.z);
    float px = 0.0f;
    float py = 0.0f;
    if (!renderer_.worldPointToPixel(centre, px, py)) continue;
    if (px < minimumX || px > maximumX || py < minimumY || py > maximumY) continue;
    if (characters_.selection().select(*character, true)) ++count;
  }

  redraw();
  return count;
}

void DemoApplication::clearSelection() {
  characters_.clearSelection();
  redraw();
}

bool DemoApplication::clearEntities() {
  characters_.clearSelection();
  world_.entities().clear();
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
  character->location.liminalObjectId = world_.liminalObjectAt(
    character->location.levelId,
    character->location.position
  );
  character->forward = forward;
  character->hitBox = hitBox;
  character->solid = solid;
  character->npc = npc;
  character->controllable = controllable;
  character->movementSpeedMultiplier = movementSpeedMultiplier;
  world_.entities().add(std::move(character));
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
  return world_.spriteAtlases().registerRgba(resource, width, height, rgba, byteCount);
}

void DemoApplication::setSelectionMode(engine::SelectionMode mode) {
  characters_.setSelectionMode(mode);
}

} // namespace demo
} // namespace isoweb
