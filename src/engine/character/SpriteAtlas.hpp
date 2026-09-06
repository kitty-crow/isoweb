#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
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

  struct LayoutCache {
    const SpriteAnimation* animation = nullptr;
    const Atlas* atlas = nullptr;
    std::size_t columns = 1;
    std::size_t rows = 1;
    std::size_t count = 1;
    int atlasWidth = 0;
    int atlasHeight = 0;
    int frameWidth = 0;
    int frameHeight = 0;
  };

  const Atlas* resolve(const std::string& resource) const;
  void invalidateCaches() const;

  std::unordered_map<std::string, Atlas> atlases_;
  mutable std::string cachedResource_;
  mutable const Atlas* cachedAtlas_ = nullptr;
  mutable LayoutCache layoutCache_;
};

} // namespace engine
} // namespace isoweb
