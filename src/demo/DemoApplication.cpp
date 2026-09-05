#include "demo/DemoApplication.hpp"

#include <algorithm>
#include <string>

namespace isoweb {
namespace demo {

DemoApplication::DemoApplication()
    : camera_(engine::CameraConfig(3.25f, 6.15f, 5.50f)),
      renderer_(world_, camera_, controls_) {
  world_.configureNavigationConnections();
}

void DemoApplication::redraw() {
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

void DemoApplication::resetRuntimeState(float baseMovementSpeed) {
  world_.clearCharacters();
  world_.setBaseMovementSpeed(baseMovementSpeed);
}

void DemoApplication::addCharacter(
  float x,
  float y,
  float z,
  float forwardX,
  float forwardY,
  float width,
  float depth,
  float height,
  int levelIndex,
  bool solid,
  bool npc,
  bool controllable,
  float speedMultiplier,
  float selectionRed,
  float selectionGreen,
  float selectionBlue
) {
  if (levelIndex < 0 || static_cast<std::size_t>(levelIndex) >= world_.levelCount()) return;

  engine::Character character;
  character.id = "character-" + std::to_string(world_.characters().size());
  character.location.worldId = "demo";
  character.location.timelineId = "default";
  character.location.levelId = world_.levelId(static_cast<std::size_t>(levelIndex));
  character.location.position = {x, y, z};
  character.forward = {forwardX, forwardY, 0.0f};

  const float halfWidth = std::max(0.01f, width) * 0.5f;
  const float halfDepth = std::max(0.01f, depth) * 0.5f;
  const float safeHeight = std::max(0.01f, height);
  character.hitBox.minimum = {-halfWidth, -halfDepth, 0.0f};
  character.hitBox.maximum = {halfWidth, halfDepth, safeHeight};

  character.solid = solid;
  character.npc = npc;
  character.controllable = controllable;
  character.movementSpeedMultiplier = std::max(0.0f, speedMultiplier);
  character.selectionTint = {
    std::max(0.0f, std::min(1.0f, selectionRed)),
    std::max(0.0f, std::min(1.0f, selectionGreen)),
    std::max(0.0f, std::min(1.0f, selectionBlue))
  };

  world_.addCharacter(character);
}

void DemoApplication::pointerAction(float normalisedX, float normalisedY) {
  const float x = std::max(0.0f, std::min(1.0f, normalisedX)) *
    std::max(1, renderer_.width() - 1);
  const float y = std::max(0.0f, std::min(1.0f, normalisedY)) *
    std::max(1, renderer_.height() - 1);

  const int pickedIndex = renderer_.pickCharacter(x, y);
  if (pickedIndex >= 0) {
    std::vector<engine::Character>& characters = world_.characters();
    engine::Character& character = characters[static_cast<std::size_t>(pickedIndex)];
    if (character.controllable) {
      character.selected = !character.selected;
      redraw();
    }
    return;
  }

  engine::Vec3 destinationPoint;
  if (!renderer_.screenToGround(x, y, destinationPoint)) return;

  bool changed = false;
  for (engine::Character& character : world_.characters()) {
    if (!character.selected || !character.controllable) continue;

    engine::EntityLocation destination = character.location;
    destination.levelId = world_.activeLevelId();
    destination.position = destinationPoint;
    destination.position.z = 0.0f;
    changed = movement_.setDestination(world_, character, destination) || changed;
  }

  if (changed) redraw();
}

void DemoApplication::clearSelection() {
  bool changed = false;
  for (engine::Character& character : world_.characters()) {
    if (!character.selected) continue;
    character.selected = false;
    changed = true;
  }
  if (changed) redraw();
}

void DemoApplication::tick(float deltaSeconds) {
  bool changed = false;
  for (engine::Character& character : world_.characters()) {
    changed = movement_.advance(world_, character, deltaSeconds) || changed;
  }
  if (changed) redraw();
}

} // namespace demo
} // namespace isoweb
