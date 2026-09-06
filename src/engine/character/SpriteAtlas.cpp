#include "engine/character/SpriteAtlas.hpp"

#include <algorithm>

namespace isoweb {
namespace engine {

void SpriteAtlasRegistry::invalidateCaches() const {
  cachedResource_.clear();
  cachedAtlas_ = nullptr;
  layoutCache_ = LayoutCache();
}

const SpriteAtlasRegistry::Atlas* SpriteAtlasRegistry::resolve(const std::string& resource) const {
  if (cachedAtlas_ && cachedResource_ == resource) return cachedAtlas_;
  const auto found = atlases_.find(resource);
  if (found == atlases_.end()) return nullptr;
  cachedResource_ = resource;
  cachedAtlas_ = &found->second;
  return cachedAtlas_;
}

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
  invalidateCaches();
  return true;
}

bool SpriteAtlasRegistry::remove(const std::string& resource) {
  const bool removed = atlases_.erase(resource) > 0;
  if (removed) invalidateCaches();
  return removed;
}

void SpriteAtlasRegistry::clear() {
  atlases_.clear();
  invalidateCaches();
}

bool SpriteAtlasRegistry::contains(const std::string& resource) const {
  return resolve(resource) != nullptr;
}

SpritePixel SpriteAtlasRegistry::sample(
  const SpriteAnimation& animation,
  std::size_t frame,
  float u,
  float v,
  bool mirror
) const {
  SpritePixel result;
  const Atlas* atlas = resolve(animation.resource);
  if (!atlas) return result;

  const std::size_t columns = std::max<std::size_t>(1, animation.columns);
  const std::size_t rows = std::max<std::size_t>(1, animation.rows);
  const std::size_t count = std::max<std::size_t>(1, animation.frameCount);

  if (
    layoutCache_.animation != &animation ||
    layoutCache_.atlas != atlas ||
    layoutCache_.columns != columns ||
    layoutCache_.rows != rows ||
    layoutCache_.count != count ||
    layoutCache_.atlasWidth != atlas->width ||
    layoutCache_.atlasHeight != atlas->height
  ) {
    layoutCache_.animation = &animation;
    layoutCache_.atlas = atlas;
    layoutCache_.columns = columns;
    layoutCache_.rows = rows;
    layoutCache_.count = count;
    layoutCache_.atlasWidth = atlas->width;
    layoutCache_.atlasHeight = atlas->height;
    layoutCache_.frameWidth = atlas->width / static_cast<int>(columns);
    layoutCache_.frameHeight = atlas->height / static_cast<int>(rows);
  }

  const int frameWidth = layoutCache_.frameWidth;
  const int frameHeight = layoutCache_.frameHeight;
  if (frameWidth <= 0 || frameHeight <= 0) return result;

  frame = std::min(frame, count - 1);
  const std::size_t frameColumn = frame % columns;
  const std::size_t frameRow = frame / columns;
  if (frameRow >= rows) return result;

  u = std::max(0.0f, std::min(0.999999f, mirror ? 1.0f - u : u));
  v = std::max(0.0f, std::min(0.999999f, v));
  const int x = static_cast<int>(frameColumn) * frameWidth + static_cast<int>(u * frameWidth);
  const int y = static_cast<int>(frameRow) * frameHeight + static_cast<int>(v * frameHeight);
  if (x < 0 || y < 0 || x >= atlas->width || y >= atlas->height) return result;

  const std::size_t offset = static_cast<std::size_t>((y * atlas->width + x) * 4);
  constexpr float BYTE_TO_UNIT = 1.0f / 255.0f;
  result.colour = {
    atlas->rgba[offset] * BYTE_TO_UNIT,
    atlas->rgba[offset + 1] * BYTE_TO_UNIT,
    atlas->rgba[offset + 2] * BYTE_TO_UNIT
  };
  result.alpha = atlas->rgba[offset + 3] * BYTE_TO_UNIT;
  return result;
}

} // namespace engine
} // namespace isoweb
