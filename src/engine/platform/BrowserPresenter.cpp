#include "engine/platform/BrowserPresenter.hpp"

#include <cstdint>
#include <emscripten/emscripten.h>

namespace isoweb {
namespace engine {

void BrowserPresenter::present(
  const std::vector<std::uint8_t>& rgba,
  int width,
  int height,
  const Camera& camera,
  bool canPan,
  float viewHeight,
  float wholeZoomScale
) const {
  const std::uintptr_t pointer = reinterpret_cast<std::uintptr_t>(rgba.data());

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
        $10
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
    wholeZoomScale
  );
}

} // namespace engine
} // namespace isoweb
