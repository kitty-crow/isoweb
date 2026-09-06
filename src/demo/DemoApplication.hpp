#pragma once

#include <cstddef>
#include <cstdint>
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
  void setControlStick(int control, float x, float y);

  void levelUp();
  void levelDown();
  void resetLevel();
  std::size_t levelCount() const { return world_.levelCount(); }
  std::size_t activeLevelIndex() const { return world_.activeLevelIndex(); }
  std::size_t defaultLevelIndex() const { return world_.defaultLevelIndex(); }
  std::size_t staticCacheBuildCount() const { return renderer_.staticCacheBuildCount(); }
  std::size_t staticCacheShiftCount() const { return renderer_.staticCacheShiftCount(); }

  bool pointerTap(float x, float y, bool additive);
  bool pointerWalkable(float x, float y) const;
  std::size_t dragSelect(float x0, float y0, float x1, float y1, bool additive);
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

  bool setCharacterSprite(
    const std::string& id,
    int state,
    const std::string& action,
    engine::CharacterFacing facing,
    const engine::SpriteAnimation& animation
  );
  bool setCharacterAction(const std::string& id, const std::string& action);
  bool registerSpriteAtlas(
    const std::string& resource,
    int width,
    int height,
    const std::uint8_t* rgba,
    std::size_t byteCount
  );

  void setBaseMovementSpeed(float speed) { characters_.defaults().baseMovementSpeed = speed; }
  void setSelectionMode(engine::SelectionMode mode);
  void setSelectionStyle(const engine::SelectionStyle& style) { characters_.selection().style = style; }

  engine::CharacterSystem& characterSystem() { return characters_; }
  engine::World& world() { return world_; }

private:
  void redraw(bool refreshPresentation = true);

  DemoWorld world_;
  engine::Camera camera_;
  engine::ControlSprites controls_;
  engine::Renderer renderer_;
  engine::BrowserPresenter presenter_;
  engine::CharacterSystem characters_;
};

} // namespace demo
} // namespace isoweb
