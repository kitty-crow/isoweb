#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "engine/math/Vec3.hpp"
#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {

struct SpritePixel {
  Vec3 colour;
  float alpha = 0.0f;
};

class SpriteAtlasRegistry {
public:
  bool registerRgba(
    const std::string& resource,
    int width,
    int height,
    const std::uint8_t* rgba,
    std::size_t byteCount
  );

  bool remove(const std::string& resource);
  void clear();
  bool contains(const std::string& resource) const;

  SpritePixel sample(
    const SpriteAnimation& animation,
    std::size_t frame,
    float u,
    float v,
    bool mirror
  ) const;

private:
  struct Atlas {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
  };

  std::map<std::string, Atlas> atlases_;
};

} // namespace engine
} // namespace isoweb
