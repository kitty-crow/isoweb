#pragma once

#include "engine/math/Vec3.hpp"

namespace isoweb {
namespace engine {

enum class SceneSurfaceKind {
  Object,
  Ground,
  Stair,
  Proxy
};

struct SceneSurfaceHit {
  bool found = false;
  float distance = 0.0f;
  Vec3 point;
  Vec3 normal;
  Vec3 colour;
  SceneSurfaceKind kind = SceneSurfaceKind::Object;
  bool walkable = false;
};

} // namespace engine
} // namespace isoweb
