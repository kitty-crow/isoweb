#pragma once

#include <cstdint>
#include <vector>

#include "engine/camera/Camera.hpp"

namespace isoweb {
namespace engine {

class BrowserPresenter {
public:
  void present(
    const std::vector<std::uint8_t>& rgba,
    int width,
    int height,
    const Camera& camera,
    bool canPan,
    float viewHeight,
    float wholeZoomScale
  ) const;
};

} // namespace engine
} // namespace isoweb
