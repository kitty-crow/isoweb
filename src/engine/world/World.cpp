#include "engine/world/World.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

#include "engine/character/CharacterPolicies.hpp"
#include "engine/character/CharacterSystem.hpp"
#include "engine/world/Character.hpp"

namespace isoweb {
namespace engine {
namespace {

struct RuntimeSample {
  float distance = 0.0f;
  Vec3 colour;
  float alpha = 1.0f;
};

bool glyph(char letter, int x, int y) {
  static const char* S[5] = {"111", "100", "111", "001", "111"};
  static const char* R[5] = {"110", "101", "110", "101", "101"};
  static const char* L[5] = {"100", "100", "100", "100", "111"};
  static const char* B[5] = {"110", "101", "110", "101", "110"};
  static const char* T[5] = {"111", "010", "010", "010", "010"};
  static const char* t[5] = {"010", "111", "010", "010", "011"};
  const char** rows = nullptr;
  switch (letter) {
    case 'S': rows = S; break;
    case 'R': rows = R; break;
    case 'L': rows = L; break;
    case 'B': rows = B; break;
    case 'T': rows = T; break;
    case 't': rows = t; break;
    default: return false;
  }
  return x >= 0 && x < 3 && y >= 0 && y < 5 && rows[y][x] == '1';
}

const char* faceLabel(ObjectFace face) {
  switch (face) {
    case ObjectFace::Front: return "S";
    case ObjectFace::Right: return "R";
    case ObjectFace::Left: return "L";
    case ObjectFace::Back: return "B";
    case ObjectFace::Top: return "T";
    case ObjectFace::Bottom: return "Bt";
  }
  return "?";
}

Vec3 faceColour(ObjectFace face) {
  switch (face) {
    case ObjectFace::Front: return {0.18f, 0.70f, 0.38f};
    case ObjectFace::Right: return {0.20f, 0.62f, 0.88f};
    case ObjectFace::Left: return {0.92f, 0.66f, 0.18f};
    case ObjectFace::Back: return {0.76f, 0.24f, 0.31f};
    case ObjectFace::Top: return {0.72f, 0.74f, 0.78f};
    case ObjectFace::Bottom: return {0.52f, 0.30f, 0.72f};
  }
  return {0.6f, 0.6f, 0.6f};
}

float normalised(float value, float minimum, float maximum) {
  const float range = maximum - minimum;
  return range > 1e-7f ? (value - minimum) / range : 0.5f;
}

bool labelPixel(const Character& character, const ObjectRayHit& hit) {
  float u = 0.5f;
  float v = 0.5f;
  switch (hit.face) {
    case ObjectFace::Front:
    case ObjectFace::Back:
      u = normalised(hit.localPoint.x, character.hitBox.minimum.x, character.hitBox.maximum.x);
      v = 1.0f - normalised(hit.localPoint.z, character.hitBox.minimum.z, character.hitBox.maximum.z);
      break;
    case ObjectFace::Left:
    case ObjectFace::Right:
      u = normalised(hit.localPoint.y, character.hitBox.minimum.y, character.hitBox.maximum.y);
      v = 1.0f - normalised(hit.localPoint.z, character.hitBox.minimum.z, character.hitBox.maximum.z);
      break;
    case ObjectFace::Top:
    case ObjectFace::Bottom:
      u = normalised(hit.localPoint.x, character.hitBox.minimum.x, character.hitBox.maximum.x);
      v = 1.0f - normalised(hit.localPoint.y, character.hitBox.minimum.y, character.hitBox.maximum.y);
      break;
  }

  const char* text = faceLabel(hit.face);
  const int count = text[1] == '\0' ? 1 : 2;
  const int virtualWidth = count == 1 ? 7 : 11;
  const int virtualHeight = 9;
  const int px = static_cast<int>(u * virtualWidth);
  const int py = static_cast<int>(v * virtualHeight);
  if (py < 2 || py >= 7) return false;

  if (count == 1) return glyph(text[0], px - 2, py - 2);
  if (px >= 2 && px < 5) return glyph(text[0], px - 2, py - 2);
  if (px >= 6 && px < 9) return glyph(text[1], px - 6, py - 2);
  return false;
}

Vec3 applyTint(const Vec3& colour, const SelectionStyle& style) {
  const float strength = std::max(0.0f, std::min(1.0f, style.strength));
  return colour * (1.0f - strength) + style.tint * strength;
}

bool spriteSample(
  const Character& character,
  const Ray& ray,
  const SpriteAtlasRegistry& atlases,
  float maximumDistance,
  bool& artworkReady,
  RuntimeSample& result
) {
  artworkReady = false;
  bool implicitMirror = false;
  const SpriteAnimation* animation = character.currentSpriteAnimation(&implicitMirror);
  if (!animation || !animation->assigned() || !atlases.contains(animation->resource)) return false;
  artworkReady = true;

  const Vec3 horizontalRay(ray.direction.x, ray.direction.y, 0.0f);
  const float horizontalLength = length(horizontalRay);
  if (horizontalLength < 1e-7f) return false;
  const Vec3 planeNormal = horizontalRay / horizontalLength;
  const float denominator = dot(ray.direction, planeNormal);
  if (std::fabs(denominator) < 1e-7f) return false;

  const Vec3 hitSize = character.hitBox.size();
  const float defaultWidth = std::max(std::fabs(hitSize.x), std::fabs(hitSize.y));
  const float defaultHeight = std::fabs(hitSize.z);
  const float width = animation->worldWidth > 0.0f ? animation->worldWidth : std::max(0.05f, defaultWidth);
  const float height = animation->worldHeight > 0.0f ? animation->worldHeight : std::max(0.05f, defaultHeight);
  const float bottom = character.location.position.z + character.hitBox.minimum.z;
  const Vec3 centre(character.location.position.x, character.location.position.y, bottom + height * 0.5f);

  const float t = dot(centre - ray.origin, planeNormal) / denominator;
  if (t <= 0.001f || t >= maximumDistance) return false;

  const Vec3 point = ray.origin + ray.direction * t;
  const Vec3 screenRight = normalise(cross(ray.direction, Vec3(0.0f, 0.0f, 1.0f)));
  const Vec3 delta = point - centre;
  const float u = dot(delta, screenRight) / width + 0.5f;
  const float v = 0.5f - delta.z / height;
  if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) return false;

  const SpritePixel pixel = atlases.sample(
    *animation,
    character.animation.frame,
    u,
    v,
    character.animation.mirror || implicitMirror
  );
  if (pixel.alpha <= 0.01f) return false;

  result.distance = t;
  result.colour = pixel.colour;
  result.alpha = pixel.alpha;
  return true;
}

} // namespace

World::World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex)
    : levels_(std::move(levels)),
      defaultLevelIndex_(defaultLevelIndex) {
  if (levels_.empty() || defaultLevelIndex_ >= levels_.size()) std::abort();
  activeLevelIndex_ = defaultLevelIndex_;
  levelIds_.reserve(levels_.size());
  for (std::size_t index = 0; index < levels_.size(); ++index) levelIds_.push_back(std::to_string(index));
}

const IWorldLevel& World::activeLevel() const {
  return *levels_[activeLevelIndex_];
}

const std::string& World::activeLevelId() const {
  return levelIds_[activeLevelIndex_];
}

bool World::setLevelId(std::size_t index, const std::string& id) {
  if (index >= levelIds_.size() || id.empty()) return false;
  for (std::size_t other = 0; other < levelIds_.size(); ++other) {
    if (other != index && levelIds_[other] == id) return false;
  }
  levelIds_[index] = id;
  return true;
}

std::size_t World::levelIndex(const std::string& levelId) const {
  for (std::size_t index = 0; index < levelIds_.size(); ++index) {
    if (levelIds_[index] == levelId) return index;
  }
  return levels_.size();
}

const WorldBounds& World::bounds() const {
  return activeLevel().bounds();
}

const WorldBounds& World::bounds(const std::string& levelId) const {
  const std::size_t index = levelIndex(levelId);
  return index < levels_.size() ? levels_[index]->bounds() : activeLevel().bounds();
}

float World::staticOccluderDistance(const Ray& ray) const {
  float closest = std::numeric_limits<float>::max();
  for (const Object& object : objects()) {
    ObjectRayHit hit;
    if (object.intersectRay(ray, 0.001f, closest, hit)) closest = hit.distance;
  }
  return closest;
}

Vec3 World::sample(const Ray& ray, float backgroundY) const {
  const float staticDistance = staticOccluderDistance(ray);
  bool runtimeFound = false;
  const Vec3 runtime = sampleRuntimeEntities(ray, backgroundY, staticDistance, runtimeFound);
  return runtimeFound ? runtime : activeLevel().sample(ray, backgroundY);
}

Vec3 World::sampleRuntimeEntities(const Ray& ray, float backgroundY, float staticDistance, bool& found) const {
  std::vector<RuntimeSample> samples;

  for (const Character* character : entities_.characters()) {
    if (!character || character->location.levelId != activeLevelId()) continue;

    RuntimeSample sample;
    bool sampleFound = false;
    bool artworkReady = false;

    if (character->hasArtwork()) {
      sampleFound = spriteSample(
        *character,
        ray,
        spriteAtlases_,
        staticDistance,
        artworkReady,
        sample
      );

      // A loaded sprite fully replaces the debug hitbox. Transparent pixels
      // remain transparent and reveal the scene/characters behind them.
      if (artworkReady) {
        if (sampleFound) {
          if (characterSystem_ && characterSystem_->isSelected(character->id)) {
            sample.colour = applyTint(sample.colour, characterSystem_->selectionStyle());
          }
          samples.push_back(sample);
        }
        continue;
      }
    }

    // Before an assigned WebP has finished decoding/registration, retain the
    // labelled debug representation so the character never disappears.
    ObjectRayHit hit;
    if (!character->intersectRay(ray, 0.001f, staticDistance, hit)) continue;
    sample.distance = hit.distance;
    sample.colour = labelPixel(*character, hit)
      ? Vec3(0.96f, 0.97f, 1.0f)
      : faceColour(hit.face);
    const float facingLight = 0.66f + 0.34f * std::max(
      0.0f,
      dot(hit.worldNormal, normalise(Vec3(-0.4f, -0.6f, 1.0f)))
    );
    sample.colour = sample.colour * facingLight;
    sample.alpha = 1.0f;
    if (characterSystem_ && characterSystem_->isSelected(character->id)) {
      sample.colour = applyTint(sample.colour, characterSystem_->selectionStyle());
    }
    samples.push_back(sample);
  }

  if (samples.empty()) {
    found = false;
    return Vec3();
  }

  std::sort(samples.begin(), samples.end(), [](const RuntimeSample& a, const RuntimeSample& b) {
    return a.distance > b.distance;
  });

  Vec3 colour = activeLevel().sample(ray, backgroundY);
  for (const RuntimeSample& sample : samples) {
    const float alpha = std::max(0.0f, std::min(1.0f, sample.alpha));
    colour = sample.colour * alpha + colour * (1.0f - alpha);
  }
  found = true;
  return colour;
}

const std::vector<Object>& World::objects() const {
  return activeLevel().objects();
}

const std::vector<Object>& World::objects(const std::string& levelId) const {
  const std::size_t index = levelIndex(levelId);
  return index < levels_.size() ? levels_[index]->objects() : activeLevel().objects();
}

bool World::intersectsSolid(const HitBox& hitBox) const {
  return activeLevel().intersectsSolid(hitBox);
}

bool World::collidesWith(const Object& candidate) const {
  return collidesWith(candidate, nullptr);
}

bool World::collidesWith(const Object& candidate, const Object* ignored) const {
  const std::vector<Object>& staticObjects = candidate.location.levelId.empty()
    ? objects()
    : objects(candidate.location.levelId);

  for (const Object& object : staticObjects) {
    if (&object == ignored) continue;
    const bool enabled = collisionPolicy_
      ? collisionPolicy_->shouldCollide(object, candidate)
      : object.collisionEnabledWith(candidate);
    if (enabled && object.overlaps(candidate)) return true;
  }

  for (const Object* object : entities_.all()) {
    if (!object || object == ignored || object == &candidate) continue;
    const bool enabled = collisionPolicy_
      ? collisionPolicy_->shouldCollide(*object, candidate)
      : object->collisionEnabledWith(candidate);
    if (enabled && object->overlaps(candidate)) return true;
  }
  return false;
}

bool World::containsPosition(const std::string& levelId, const Vec3& position) const {
  const WorldBounds& targetBounds = bounds(levelId);
  if (targetBounds.points.empty()) return true;
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = -std::numeric_limits<float>::max();
  float maxY = -std::numeric_limits<float>::max();
  for (const Vec3& point : targetBounds.points) {
    minX = std::min(minX, point.x);
    minY = std::min(minY, point.y);
    maxX = std::max(maxX, point.x);
    maxY = std::max(maxY, point.y);
  }
  return position.x >= minX && position.x <= maxX && position.y >= minY && position.y <= maxY;
}

bool World::canMoveLevelUp() const {
  return activeLevelIndex_ + 1 < levels_.size();
}

bool World::canMoveLevelDown() const {
  return activeLevelIndex_ > 0;
}

bool World::isDefaultLevel() const {
  return activeLevelIndex_ == defaultLevelIndex_;
}

bool World::setActiveLevel(std::size_t index) {
  if (index >= levels_.size() || index == activeLevelIndex_) return false;
  activeLevelIndex_ = index;
  return true;
}

bool World::levelUp() {
  return canMoveLevelUp() && setActiveLevel(activeLevelIndex_ + 1);
}

bool World::levelDown() {
  return canMoveLevelDown() && setActiveLevel(activeLevelIndex_ - 1);
}

bool World::resetLevel() {
  return setActiveLevel(defaultLevelIndex_);
}

} // namespace engine
} // namespace isoweb
