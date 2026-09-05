#pragma once

#include "demo/DemoWorld.hpp"
#include "engine/camera/Camera.hpp"
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
  void zoomIn();
  void zoomOut();
  void resetZoom();
  void setDetailedMode(bool enabled);
  void pan(float right, float down);
  void resetCamera();

private:
  void redraw();

  DemoWorld world_;
  engine::Camera camera_;
  engine::ControlSprites controls_;
  engine::Renderer renderer_;
  engine::BrowserPresenter presenter_;
};

} // namespace demo
} // namespace isoweb
