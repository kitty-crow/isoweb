#include <cstdlib>
#include <iostream>

#include "support/DemoWorldFixture.hpp"
#include "engine/render/GpuStaticScene.hpp"

using namespace isoweb::engine;

namespace {

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "[gpu-static-scene] " << message << "\n";
  std::exit(1);
}

} // namespace

int main() {
  isoweb::test::DemoWorldFixture world;
  GpuStaticScene scene;

  require(world.activeLevelIndex() == world.defaultLevelIndex(), "demo did not start on default level");
  require(world.buildGpuStaticScene(scene), "middle level did not expose GPU static scene");
  require(scene.spheres.size() == 1, "middle GPU scene should contain one sphere");
  require(scene.rooms.size() == 5, "middle GPU scene should contain five floor rooms");
  require(scene.floorHoles.size() == 1, "middle GPU scene should contain one stair floor hole");
  require(!scene.visualBoxes.empty(), "middle GPU scene has no visual boxes");
  require(!scene.shadowBoxes.empty(), "middle GPU scene has no shadow boxes");

  require(world.levelDown(), "could not switch to lower level");
  scene.clear();
  require(!world.buildGpuStaticScene(scene), "lower level should fall back until cone/pyramid GLSL is ported");

  require(world.resetLevel(), "could not return to middle level");
  scene.clear();
  require(world.buildGpuStaticScene(scene), "middle GPU scene disappeared after level reset");

  require(world.levelUp(), "could not switch to upper level");
  scene.clear();
  require(!world.buildGpuStaticScene(scene), "upper level should fall back until polyhedron GLSL is ported");

  std::cout << "GPU static scene support/fallback contract passed.\n";
  return 0;
}
