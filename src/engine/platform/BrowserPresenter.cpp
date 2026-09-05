#include "engine/platform/BrowserPresenter.hpp"

#include <cstdint>
#include <emscripten/emscripten.h>

namespace isoweb {
namespace engine {
namespace {

constexpr std::uint32_t CONTROL_ZOOM_IN = 1u << 0;
constexpr std::uint32_t CONTROL_ZOOM_OUT = 1u << 1;
constexpr std::uint32_t CONTROL_RESET_ZOOM = 1u << 2;
constexpr std::uint32_t CONTROL_RESET_YAW = 1u << 3;
constexpr std::uint32_t CONTROL_PAN_UP = 1u << 4;
constexpr std::uint32_t CONTROL_PAN_DOWN = 1u << 5;
constexpr std::uint32_t CONTROL_PAN_LEFT = 1u << 6;
constexpr std::uint32_t CONTROL_PAN_RIGHT = 1u << 7;
constexpr std::uint32_t CONTROL_RESET_PAN = 1u << 8;

std::uint32_t controlMask(const CameraControlState& state) {
  std::uint32_t mask = 0;
  if (state.canZoomIn) mask |= CONTROL_ZOOM_IN;
  if (state.canZoomOut) mask |= CONTROL_ZOOM_OUT;
  if (state.canResetZoom) mask |= CONTROL_RESET_ZOOM;
  if (state.canResetYaw) mask |= CONTROL_RESET_YAW;
  if (state.canPanUp) mask |= CONTROL_PAN_UP;
  if (state.canPanDown) mask |= CONTROL_PAN_DOWN;
  if (state.canPanLeft) mask |= CONTROL_PAN_LEFT;
  if (state.canPanRight) mask |= CONTROL_PAN_RIGHT;
  if (state.canResetPan) mask |= CONTROL_RESET_PAN;
  return mask;
}

} // namespace

void BrowserPresenter::present(
  const std::vector<std::uint8_t>& rgba,
  int width,
  int height,
  const Camera& camera,
  const CameraControlState& controlState,
  bool canPan,
  float viewHeight,
  float wholeZoomScale
) const {
  const std::uintptr_t pointer = reinterpret_cast<std::uintptr_t>(rgba.data());
  const std::uint32_t controls = controlMask(controlState);

  EM_ASM({
    const present = globalThis.isowebPresent;
    if (typeof present === 'function') {
      present(
        HEAPU8,
        $0,
        $1,
        $2,
        $3,
        $4,
        $5,
        $6,
        !!$7,
        !!$8,
        $9,
        $10,
        $11
      );
    }
  },
    static_cast<int>(pointer),
    width,
    height,
    camera.yawStep() * 45,
    camera.panX(),
    camera.panY(),
    camera.zoomPreset(),
    camera.detailedMode() ? 1 : 0,
    canPan ? 1 : 0,
    viewHeight,
    wholeZoomScale,
    static_cast<int>(controls)
  );
}

} // namespace engine
} // namespace isoweb
