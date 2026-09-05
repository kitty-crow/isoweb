#pragma once

#include "DFPSR/api/imageAPI.h"
#include "engine/camera/Camera.hpp"

namespace isoweb {
namespace engine {

struct LevelControlState {
  bool canMoveUp = false;
  bool canMoveDown = false;
  bool atDefault = true;
};

class ControlSprites {
public:
  void draw(
    dsr::OrderedImageRgbaU8& frame,
    int frameWidth,
    int frameHeight,
    const CameraControlState& cameraState,
    const LevelControlState& levelState
  );

private:
  void ensureSprites();
  void buildRotate(dsr::OrderedImageRgbaU8& sprite, bool mirror, bool disabled = false);
  void buildPan(dsr::OrderedImageRgbaU8& sprite, float dx, float dy, bool disabled = false);
  void buildReset(dsr::OrderedImageRgbaU8& sprite, bool disabled = false);
  void buildLevelReset(dsr::OrderedImageRgbaU8& sprite, bool disabled = false);
  void buildZoom(dsr::OrderedImageRgbaU8& sprite, bool plus, bool disabled = false);
  void spritePixel(dsr::OrderedImageRgbaU8& sprite, int x, int y, float distance, float strength = 1.0f);

  dsr::OrderedImageRgbaU8 clockwiseSprite_;
  dsr::OrderedImageRgbaU8 counterClockwiseSprite_;
  dsr::OrderedImageRgbaU8 upSprite_;
  dsr::OrderedImageRgbaU8 downSprite_;
  dsr::OrderedImageRgbaU8 leftSprite_;
  dsr::OrderedImageRgbaU8 rightSprite_;
  dsr::OrderedImageRgbaU8 upDisabled_;
  dsr::OrderedImageRgbaU8 downDisabled_;
  dsr::OrderedImageRgbaU8 leftDisabled_;
  dsr::OrderedImageRgbaU8 rightDisabled_;
  dsr::OrderedImageRgbaU8 resetSprite_;
  dsr::OrderedImageRgbaU8 resetDisabled_;
  dsr::OrderedImageRgbaU8 levelResetSprite_;
  dsr::OrderedImageRgbaU8 levelResetDisabled_;
  dsr::OrderedImageRgbaU8 plusSprite_;
  dsr::OrderedImageRgbaU8 minusSprite_;
  dsr::OrderedImageRgbaU8 plusDisabled_;
  dsr::OrderedImageRgbaU8 minusDisabled_;
};

} // namespace engine
} // namespace isoweb
