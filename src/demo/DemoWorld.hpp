#pragma once

#include <memory>
#include <vector>

#include "engine/world/World.hpp"

namespace isoweb {
namespace demo {

class DemoWorld final : public engine::World {
public:
  DemoWorld();

private:
  static std::vector<std::unique_ptr<engine::IWorldLevel>> makeLevels();
};

} // namespace demo
} // namespace isoweb
