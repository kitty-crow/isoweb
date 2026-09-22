#pragma once

#include <memory>
#include <vector>

#include "engine/world/World.hpp"

namespace isoweb {
namespace test {

class DemoWorldFixture final : public engine::World {
public:
  DemoWorldFixture();

private:
  static std::vector<std::unique_ptr<engine::IWorldLevel>> makeLevels();
};

} // namespace test
} // namespace isoweb
