#include "engine/character/SpriteAtlas.hpp"

#include <algorithm>

namespace isoweb {
namespace engine {

bool SpriteAtlasRegistry::registerRgba(
  const std::string& resource,
  int width,
  int height,
  const std::uint8_t* rgba,
  std::size_t byteCount
) {
  if (resource.empty() || width <= 0 || height <= 0 || !rgba) return false;
  const std::size_t required = static_cast<std::size_t>(width) * height * 4;
  if (byteCount < required) return false;

  Atlas atlas;
  atlas.width = width;
  atlas.height = height;
  atlas.rgba.assign(rgba, rgba + required);
  atlases_[resource] = std::move(atlas);
  return true;
}

bool SpriteAtlasRegistry::remove(const std::string& resource) {
  return atlases_.erase(resource) > 0;
}

void SpriteAtlasRegistry::clear() {
  atlases_.clear();
}

bool SpriteAtlasRegistry::contains(const std::string& resource) const {
  return atlases_.find(resource) != atlases_.end();
}

SpritePixel SpriteAtlasRegistry::sample(
  const SpriteAnimation& animation,
  std::size_t frame,
  float u,
  float v,
  bool mirror
) const {
  SpritePixel result;
  const auto found = atlases_.find(animation.resource);
  if (found == atlases_.end()) return result;
  const Atlas& atlas = found->second;

  const std::size_t columns = std::max<std::size_t>(1, animation.columns);
  const std::size_t rows = std::max<std::size_t>(1, animation.rows);
  const int frameWidth = atlas.width / static_cast<int>(columns);
  const int frameHeight = atlas.height / static_cast<int>(rows);
  if (frameWidth <= 0 || frameHeight <= 0) return result;

  const std::size_t count = std::max<std::size_t>(1, animation.frameCount);
  frame = std::min(frame, count - 1);
  const std::size_t frameColumn = frame % columns;
  const std::size_t frameRow = frame / columns;
  if (frameRow >= rows) return result;

  u = std::max(0.0f, std::min(0.999999f, mirror ? 1.0f - u : u));
  v = std::max(0.0f, std::min(0.999999f, v));
  const int x = static_cast<int>(frameColumn) * frameWidth + static_cast<int>(u * frameWidth);
  const int y = static_cast<int>(frameRow) * frameHeight + static_cast<int>(v * frameHeight);
  if (x < 0 || y < 0 || x >= atlas.width || y >= atlas.height) return result;

  const std::size_t offset = static_cast<std::size_t>((y * atlas.width + x) * 4);
  result.colour = {
    atlas.rgba[offset] / 255.0f,
    atlas.rgba[offset + 1] / 255.0f,
    atlas.rgba[offset + 2] / 255.0f
  };
  result.alpha = atlas.rgba[offset + 3] / 255.0f;
  return result;
}

} // namespace engine
} // namespace isoweb
