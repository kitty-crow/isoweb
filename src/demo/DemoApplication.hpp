#pragma once

#include <cstddef>

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/navigation/CharacterMovement.hpp"
#include "engine/platform/BrowserPresenter.hpp"
#include "engine/render/Renderer.hpp"
#include "engine/ui/ControlSprites.hpp"

namespace isoweb {
namespace demo {

class DemoApplication {
public:
  DemoApplication();

  void render();
  void resize(int width, int height);
  void rotateClockwise();
  void rotateCounterClockwise();
  void resetYaw();
  void setDetailedYawMode(bool enabled);
  void zoomIn();
  void zoomOut();
  void resetZoom();
  void setDetailedMode(bool enabled);
  void pan(float right, float down);
  void resetCamera();

  void levelUp();
  void levelDown();
  void resetLevel();
  std::size_t levelCount() const { return world_.levelCount(); }
  std::size_t activeLevelIndex() const { return world_.activeLevelIndex(); }
  std::size_t defaultLevelIndex() const { return world_.defaultLevelIndex(); }

  void resetRuntimeState(float baseMovementSpeed);
  void addCharacter(
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
  );
  std::size_t characterCount() const { return world_.characters().size(); }

  void pointerAction(float normalisedX, float normalisedY);
  void clearSelection();
  void tick(float deltaSeconds);

private:
  void redraw();

  DemoWorld world_;
  engine::Camera camera_;
  engine::ControlSprites controls_;
  engine::Renderer renderer_;
  engine::BrowserPresenter presenter_;
  engine::CharacterMovement movement_;
};

} // namespace demo
} // namespace isoweb
