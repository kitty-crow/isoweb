#pragma once

#include <memory>
#include <vector>

#include "engine/world/World.hpp"

namespace isoweb {
namespace demo {

class DemoWorld final : public engine::World {
public:
  DemoWorld();

  // Demo-only geometry metadata. The navigation engine receives generic
  // connections and has no concept of stairs, ramps, or other presentation.
  void configureNavigationConnections();

private:
  static std::vector<std::unique_ptr<engine::IWorldLevel>> makeLevels();
};

} // namespace demo
} // namespace isoweb
