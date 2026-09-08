#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "demo/DemoObstacles.hpp"
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
  bool refinePreview(std::size_t maxTiles);
  bool previewNeedsRefinement() const { return renderer_.previewNeedsRefinement(); }
  void tick(float deltaSeconds);
  bool needsTick() const { return obstacles_.enabled() || characters_.needsTick(); }
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
  std::size_t previewCoarseSampleCount() const { return renderer_.previewCoarseSampleCount(); }
  std::size_t previewRefinedSampleCount() const { return renderer_.previewRefinedSampleCount(); }
  std::size_t previewDemandedTexelCount() const { return renderer_.previewDemandedTexelCount(); }
  std::size_t previewPotentialTexelCount() const { return renderer_.previewPotentialTexelCount(); }

  void setObstaclesEnabled(bool enabled);
  bool obstaclesEnabled() const { return obstacles_.enabled(); }

  bool pointerTap(float x, float y, bool additive);
  bool pointerDoubleTap(float x, float y);
  bool pointerWalkable(float x, float y) const;
  std::size_t dragSelect(float x0, float y0, float x1, float y1, bool additive);
  void clearSelection();

  bool clearEntities();
  std::size_t characterCount() const;
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
  bool hurtCharacter(const std::string& id);

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
  DemoObstacleSystem obstacles_;
  std::unordered_map<std::string, engine::EntityLocation> characterSpawns_;
};

} // namespace demo
} // namespace isoweb
