#pragma once

#include <cstddef>
#include <string>

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/platform/BrowserPresenter.hpp"
#include "engine/render/Renderer.hpp"
#include "engine/ui/ControlSprites.hpp"

namespace isoweb {
namespace demo {

class DemoApplication {
public:
  DemoApplication();

  void render();
  void tick(float deltaSeconds);
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

  bool pointerTap(float x, float y, bool additive);
  void clearSelection();

  bool clearEntities();
  bool createCharacter(
    const std::string& id,
    const engine::EntityLocation& location,
    const engine::Vec3& forward,
    const engine::HitBox& hitBox,
    bool solid,
    bool npc,
    bool controllable,
    float movementSpeedMultiplier
  );
  engine::Character* character(const std::string& id);

  engine::CharacterSystem& characterSystem() { return characters_; }
  engine::World& world() { return world_; }

private:
  void redraw();
  void configureDemoWorldNavigation();

  DemoWorld world_;
  engine::Camera camera_;
  engine::ControlSprites controls_;
  engine::Renderer renderer_;
  engine::BrowserPresenter presenter_;
  engine::CharacterSystem characters_;
  bool movementCommandArmed_ = false;
};

} // namespace demo
} // namespace isoweb
