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

float distanceSquared(const Vec3& a, const Vec3& b) {
  const Vec3 delta = a - b;
  return dot(delta, delta);
}

float pointSegmentDistanceSquared(const Vec3& point, const Vec3& a, const Vec3& b) {
  const Vec3 segment = b - a;
  const float lengthSquared = dot(segment, segment);
  if (lengthSquared < 1e-10f) return distanceSquared(point, a);
  const float t = std::max(0.0f, std::min(1.0f, dot(point - a, segment) / lengthSquared));
  return distanceSquared(point, a + segment * t);
}

bool glyph(char letter, int x, int y) {
  static const char* F[5] = {"111", "100", "110", "100", "100"};
  static const char* R[5] = {"110", "101", "110", "101", "101"};
  static const char* L[5] = {"100", "100", "100", "100", "111"};
  static const char* B[5] = {"110", "101", "110", "101", "110"};
  static const char* T[5] = {"111", "010", "010", "010", "010"};
  static const char* t[5] = {"010", "111", "010", "010", "011"};
  const char** rows = nullptr;
  switch (letter) {
    case 'F': rows = F; break;
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
    case ObjectFace::Front: return "F";
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
  const int px = static_cast<int>(u * virtualWidth);
  const int py = static_cast<int>(v * 9);
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

Object renderProxy(const Object& object, const Vec3& position, const std::string& levelId) {
  Object proxy;
  proxy.id = object.id;
  proxy.location = object.location;
  proxy.location.levelId = levelId;
  proxy.location.position = position;
  proxy.forward = object.forward;
  proxy.hitBox = object.hitBox;
  proxy.solid = object.solid;
  return proxy;
}

} // namespace

World::World(std::vector<std::unique_ptr<IWorldLevel>> levels, std::size_t defaultLevelIndex)
    : levels_(std::move(levels)),
      defaultLevelIndex_(defaultLevelIndex) {
  if (levels_.empty() || defaultLevelIndex_ >= levels_.size()) std::abort();
  activeLevelIndex_ = defaultLevelIndex_;
  levelIds_.reserve(levels_.size());
  levelXYBounds_.resize(levels_.size());

  for (std::size_t index = 0; index < levels_.size(); ++index) {
    levelIds_.push_back(std::to_string(index));
    levelLookup_[levelIds_.back()] = index;
    levels_[index]->setResident(index == defaultLevelIndex_);

    const WorldBounds& bounds = levels_[index]->bounds();
    LevelXYBounds& cached = levelXYBounds_[index];
    if (!bounds.points.empty()) {
      cached.unrestricted = false;
      cached.minimumX = cached.minimumY = std::numeric_limits<float>::max();
      cached.maximumX = cached.maximumY = -std::numeric_limits<float>::max();
      for (const Vec3& point : bounds.points) {
        cached.minimumX = std::min(cached.minimumX, point.x);
        cached.minimumY = std::min(cached.minimumY, point.y);
        cached.maximumX = std::max(cached.maximumX, point.x);
        cached.maximumY = std::max(cached.maximumY, point.y);
      }
    }
  }
  levelLights_.resize(levels_.size());
}

const IWorldLevel& World::activeLevel() const {
  return *levels_[activeLevelIndex_];
}

const IWorldLevel& World::levelFor(const std::string& levelId) const {
  const std::size_t index = levelIndex(levelId);
  return index < levels_.size() ? *levels_[index] : activeLevel();
}

const std::string& World::activeLevelId() const {
  return levelIds_[activeLevelIndex_];
}

bool World::setLevelId(std::size_t index, const std::string& id) {
  if (index >= levelIds_.size() || id.empty()) return false;
  const auto existing = levelLookup_.find(id);
  if (existing != levelLookup_.end() && existing->second != index) return false;

  levelLookup_.erase(levelIds_[index]);
  levelIds_[index] = id;
  levelLookup_[id] = index;
  runtimeRenderCachePrepared_ = false;
  return true;
}

std::size_t World::levelIndex(const std::string& levelId) const {
  const auto found = levelLookup_.find(levelId);
  return found == levelLookup_.end() ? levels_.size() : found->second;
}

const WorldBounds& World::bounds() const {
  return activeLevel().bounds();
}

const WorldBounds& World::bounds(const std::string& levelId) const {
  return levelFor(levelId).bounds();
}

bool World::traceEnvironment(const Ray& ray, SceneSurfaceHit& hit) const {
  return activeLevel().traceEnvironment(ray, hit);
}

bool World::traceEnvironment(const std::string& levelId, const Ray& ray, SceneSurfaceHit& hit) const {
  const std::size_t index = levelIndex(levelId);
  if (index >= levels_.size()) return false;
  return levels_[index]->traceEnvironment(ray, hit);
}

float World::environmentDistance(const Ray& ray) const {
  SceneSurfaceHit hit;
  return traceEnvironment(ray, hit) ? hit.distance : std::numeric_limits<float>::max();
}

void World::setNavigationLinks(std::vector<NavigationLink> links) {
  for (std::size_t index = 0; index < links.size(); ++index) {
    LiminalObject& link = links[index];
    if (link.id.empty()) link.id = "liminal-" + std::to_string(index);
    if (link.category.empty()) link.category = "liminal";
    if (link.type.empty()) link.type = "connector";

    const std::size_t pairCount = std::min(link.forwardTraversal.size(), link.reverseTraversal.size());
    if (pairCount > 0) {
      Vec3 accumulated;
      for (std::size_t i = 0; i < pairCount; ++i) {
        accumulated = accumulated +
          (link.reverseTraversal[pairCount - 1 - i] - link.forwardTraversal[i]);
      }
      link.fromToViewOffset = accumulated / static_cast<float>(pairCount);
      link.hasViewOffset = true;
    } else {
      link.fromToViewOffset = link.toPosition - link.fromPosition;
      link.hasViewOffset = true;
    }
  }
  liminalObjects_ = std::move(links);
  runtimeRenderCachePrepared_ = false;
}

const LiminalObject* World::liminalObject(const std::string& id) const {
  if (id.empty()) return nullptr;
  for (const LiminalObject& object : liminalObjects_) {
    if (object.id == id) return &object;
  }
  return nullptr;
}

std::string World::liminalObjectAt(
  const std::string& levelId,
  const Vec3& position,
  float tolerance
) const {
  const float maximumDistanceSquared = tolerance * tolerance;
  for (const LiminalObject& object : liminalObjects_) {
    const std::vector<Vec3>* traversal = nullptr;
    Vec3 endpoint;
    if (levelId == object.fromLevelId) {
      traversal = &object.forwardTraversal;
      endpoint = object.fromPosition;
    } else if (levelId == object.toLevelId) {
      traversal = &object.reverseTraversal;
      endpoint = object.toPosition;
    } else {
      continue;
    }

    if (distanceSquared(position, endpoint) <= maximumDistanceSquared) return object.id;
    Vec3 previous = endpoint;
    for (const Vec3& point : *traversal) {
      if (pointSegmentDistanceSquared(position, previous, point) <= maximumDistanceSquared) {
        return object.id;
      }
      previous = point;
    }
  }
  return std::string();
}

bool World::mapLiminalPosition(
  const EntityLocation& location,
  const std::string& targetLevelId,
  Vec3& mapped
) const {
  if (location.liminalObjectId.empty()) {
    if (location.levelId != targetLevelId) return false;
    mapped = location.position;
    return true;
  }

  const LiminalObject* object = liminalObject(location.liminalObjectId);
  if (!object || !object->touchesLevel(targetLevelId)) return false;
  if (location.levelId == targetLevelId) {
    mapped = location.position;
    return true;
  }
  if (!object->hasViewOffset) return false;

  if (location.levelId == object->fromLevelId && targetLevelId == object->toLevelId) {
    mapped = location.position + object->fromToViewOffset;
    return true;
  }
  if (location.levelId == object->toLevelId && targetLevelId == object->fromLevelId) {
    mapped = location.position - object->fromToViewOffset;
    return true;
  }
  return false;
}

bool World::characterVisibleOnActiveLevel(const Character& character) const {
  Vec3 ignored;
  return renderPositionFor(character, ignored);
}

bool World::renderPositionFor(const Character& character, Vec3& position) const {
  return mapLiminalPosition(character.location, activeLevelId(), position);
}

void World::prepareRenderFrame(const Vec3& viewDirection) const {
  runtimeRenderEntries_.clear();
  runtimeSpritePlaneValid_ = false;

  const float horizontalLengthSquared =
    viewDirection.x * viewDirection.x + viewDirection.y * viewDirection.y;
  if (horizontalLengthSquared >= 1e-14f) {
    const float inverseHorizontalLength = 1.0f / std::sqrt(horizontalLengthSquared);
    runtimeSpritePlaneNormal_ = {
      viewDirection.x * inverseHorizontalLength,
      viewDirection.y * inverseHorizontalLength,
      0.0f
    };
    const float denominator = dot(viewDirection, runtimeSpritePlaneNormal_);
    if (std::fabs(denominator) >= 1e-7f) {
      runtimeSpriteScreenRight_ = {
        -runtimeSpritePlaneNormal_.y,
        runtimeSpritePlaneNormal_.x,
        0.0f
      };
      runtimeSpriteInverseDenominator_ = 1.0f / denominator;
      runtimeSpritePlaneValid_ = true;
    }
  }

  const auto& characters = entities_.characters();
  runtimeRenderEntries_.reserve(characters.size());
  const std::string& levelId = activeLevelId();

  for (const Character* character : characters) {
    if (!character) continue;
    Vec3 renderPosition;
    if (!renderPositionFor(*character, renderPosition)) continue;

    RuntimeRenderEntry entry;
    entry.character = character;
    entry.renderPosition = renderPosition;
    entry.proxy = renderProxy(*character, renderPosition, levelId);
    entry.selected = characterSystem_ && characterSystem_->isSelected(character->id);

    if (character->hasArtwork() && runtimeSpritePlaneValid_) {
      bool implicitMirror = false;
      const SpriteAnimation* animation = character->currentSpriteAnimation(&implicitMirror);
      if (animation && animation->assigned() && spriteAtlases_.contains(animation->resource)) {
        const Vec3 hitSize = character->hitBox.size();
        const float defaultWidth = std::max(std::fabs(hitSize.x), std::fabs(hitSize.y));
        const float defaultHeight = std::fabs(hitSize.z);
        const float width = animation->worldWidth > 0.0f
          ? animation->worldWidth
          : std::max(0.05f, defaultWidth);
        const float height = animation->worldHeight > 0.0f
          ? animation->worldHeight
          : std::max(0.05f, defaultHeight);
        const float bottom = renderPosition.z + character->hitBox.minimum.z;

        entry.artworkReady = true;
        entry.animation = animation;
        entry.spriteFrame = character->animation.frame;
        entry.spriteMirror = character->animation.mirror || implicitMirror;
        entry.spriteCentre = {
          renderPosition.x,
          renderPosition.y,
          bottom + height * 0.5f
        };
        entry.spriteInverseWidth = 1.0f / width;
        entry.spriteInverseHeight = 1.0f / height;
      }
    }

    runtimeRenderEntries_.push_back(std::move(entry));
  }

  runtimeSampleScratch_.clear();
  if (runtimeSampleScratch_.capacity() < runtimeRenderEntries_.size()) {
    runtimeSampleScratch_.reserve(runtimeRenderEntries_.size());
  }
  runtimeRenderCachePrepared_ = true;
}

bool World::setLevelLight(
  const std::string& levelId,
  const Vec3& position,
  float ambient,
  float attenuation,
  float directScale
) {
  const std::size_t index = levelIndex(levelId);
  if (index >= levelLights_.size()) return false;
  LevelLight& light = levelLights_[index];
  light.configured = true;
  light.position = position;
  light.ambient = std::max(0.0f, std::min(1.0f, ambient));
  light.attenuation = std::max(0.0f, attenuation);
  light.directScale = std::max(0.0f, directScale);
  return true;
}

float World::runtimeLightVisibility(std::size_t index, const Vec3& point) const {
  if (index >= levelLights_.size() || !levelLights_[index].configured) return 1.0f;

  const LevelLight& light = levelLights_[index];
  const Vec3 toLight = light.position - point;
  const float distanceSquaredValue = dot(toLight, toLight);
  if (distanceSquaredValue < 1e-12f) return 1.0f;
  const float distance = std::sqrt(distanceSquaredValue);
  const Vec3 direction = toLight / distance;
  const float maximumDistance = distance - 0.006f;
  if (maximumDistance <= 0.0f) return 1.0f;

  const Ray shadowRay{point + direction * 0.003f, direction};
  return levels_[index]->rayOccluded(shadowRay, maximumDistance) ? 0.0f : 1.0f;
}

float World::runtimeLightVisibility(const std::string& levelId, const Vec3& point) const {
  return runtimeLightVisibility(levelIndex(levelId), point);
}

Vec3 World::shadeRuntimeSurface(
  std::size_t index,
  const Vec3& point,
  const Vec3& normal,
  const Vec3& colour
) const {
  if (index >= levelLights_.size() || !levelLights_[index].configured) return colour;

  const LevelLight& light = levelLights_[index];
  const Vec3 toLight = light.position - point;
  const float distanceSquaredValue = dot(toLight, toLight);
  if (distanceSquaredValue < 1e-12f) return colour;
  const float distance = std::sqrt(distanceSquaredValue);
  const Vec3 direction = toLight / distance;
  const float diffuse = std::max(0.0f, dot(normal, direction));
  const float attenuation = 1.0f / (1.0f + light.attenuation * distanceSquaredValue);
  const float maximumDistance = distance - 0.006f;
  const float visibility = maximumDistance > 0.0f &&
      levels_[index]->rayOccluded({point + direction * 0.003f, direction}, maximumDistance)
    ? 0.0f
    : 1.0f;
  const float factor = light.ambient + visibility * diffuse * attenuation * light.directScale;
  const Vec3 shaded = colour * factor;
  return {
    std::min(shaded.x, 1.0f),
    std::min(shaded.y, 1.0f),
    std::min(shaded.z, 1.0f)
  };
}

Vec3 World::shadeRuntimeSurface(
  const std::string& levelId,
  const Vec3& point,
  const Vec3& normal,
  const Vec3& colour
) const {
  return shadeRuntimeSurface(levelIndex(levelId), point, normal, colour);
}

float World::runtimeSpriteLightFactor(std::size_t index, const Vec3& point) const {
  if (index >= levelLights_.size() || !levelLights_[index].configured) return 1.0f;
  const LevelLight& light = levelLights_[index];
  const float visibility = runtimeLightVisibility(index, point);
  return light.ambient + visibility * (1.0f - light.ambient);
}

float World::runtimeSpriteLightFactor(const std::string& levelId, const Vec3& point) const {
  return runtimeSpriteLightFactor(levelIndex(levelId), point);
}

Vec3 World::sample(const Ray& ray, float backgroundY) const {
  SceneSurfaceHit environmentHit;
  const Vec3 environmentColour = activeLevel().sampleWithHit(ray, backgroundY, environmentHit);
  const float environmentHitDistance = environmentHit.found
    ? environmentHit.distance
    : std::numeric_limits<float>::max();

  if (!runtimeRenderCachePrepared_) prepareRenderFrame(ray.direction);

  bool runtimeFound = false;
  const Vec3 runtime = sampleRuntimeEntities(
    ray,
    environmentColour,
    environmentHitDistance,
    runtimeFound
  );
  return runtimeFound ? runtime : environmentColour;
}

Vec3 World::sampleRuntimeEntities(
  const Ray& ray,
  const Vec3& environmentColour,
  float environmentHitDistance,
  bool& found
) const {
  if (!runtimeRenderCachePrepared_) prepareRenderFrame(ray.direction);

  std::vector<RuntimeSample>& samples = runtimeSampleScratch_;
  samples.clear();

  for (const RuntimeRenderEntry& entry : runtimeRenderEntries_) {
    const Character* character = entry.character;
    if (!character) continue;

    RuntimeSample sample;

    if (entry.artworkReady && entry.animation && runtimeSpritePlaneValid_) {
      const float t = dot(entry.spriteCentre - ray.origin, runtimeSpritePlaneNormal_) *
        runtimeSpriteInverseDenominator_;
      if (t > 0.001f && t < environmentHitDistance) {
        const Vec3 point = ray.origin + ray.direction * t;
        const Vec3 delta = point - entry.spriteCentre;
        const float u = dot(delta, runtimeSpriteScreenRight_) * entry.spriteInverseWidth + 0.5f;
        const float v = 0.5f - delta.z * entry.spriteInverseHeight;

        if (u >= 0.0f && u < 1.0f && v >= 0.0f && v < 1.0f) {
          const SpritePixel pixel = spriteAtlases_.sample(
            *entry.animation,
            entry.spriteFrame,
            u,
            v,
            entry.spriteMirror
          );
          if (pixel.alpha > 0.01f) {
            sample.distance = t;
            sample.point = point;
            sample.colour = pixel.colour;
            sample.alpha = pixel.alpha;
            if (entry.selected && characterSystem_) {
              sample.colour = applyTint(sample.colour, characterSystem_->selectionStyle());
            }
            sample.colour = sample.colour * runtimeSpriteLightFactor(activeLevelIndex_, sample.point);
            samples.push_back(sample);
          }
        }
      }
      // Once artwork is resolved, transparent/missed sprite pixels must stay
      // transparent rather than falling back to the Character's debug box.
      continue;
    }

    ObjectRayHit hit;
    if (!entry.proxy.intersectRay(ray, 0.001f, environmentHitDistance, hit)) continue;
    sample.distance = hit.distance;
    sample.point = hit.worldPoint;
    sample.colour = labelPixel(*character, hit)
      ? Vec3(0.96f, 0.97f, 1.0f)
      : faceColour(hit.face);
    sample.alpha = 1.0f;
    if (entry.selected && characterSystem_) {
      sample.colour = applyTint(sample.colour, characterSystem_->selectionStyle());
    }
    sample.colour = shadeRuntimeSurface(
      activeLevelIndex_,
      sample.point,
      hit.worldNormal,
      sample.colour
    );
    samples.push_back(sample);
  }

  if (samples.empty()) {
    found = false;
    return Vec3();
  }

  if (samples.size() > 1) {
    std::sort(samples.begin(), samples.end(), [](const RuntimeSample& a, const RuntimeSample& b) {
      return a.distance > b.distance;
    });
  }

  Vec3 colour = environmentColour;
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
  return levelFor(levelId).objects();
}

bool World::isLevelResident(std::size_t index) const {
  return index < levels_.size() && levels_[index] && levels_[index]->isResident();
}

bool World::isLevelResident(const std::string& levelId) const {
  return isLevelResident(levelIndex(levelId));
}

std::size_t World::residentLevelCount() const {
  std::size_t count = 0;
  for (const std::unique_ptr<IWorldLevel>& level : levels_) {
    if (level && level->isResident()) ++count;
  }
  return count;
}

bool World::intersectsSolid(const HitBox& hitBox) const {
  return activeLevel().intersectsSolid(hitBox);
}

bool World::collidesWith(const Object& candidate) const {
  return collidesWith(candidate, nullptr);
}

bool World::collidesWith(const Object& candidate, const Object* ignored) const {
  const std::string targetLevelId = candidate.location.levelId.empty()
    ? activeLevelId()
    : candidate.location.levelId;
  const IWorldLevel& targetLevel = levelFor(targetLevelId);
  const std::vector<Object>& staticObjects = targetLevel.objects();

  for (std::size_t index = 0; index < staticObjects.size(); ++index) {
    const Object& object = staticObjects[index];
    const bool enabled = collisionPolicy_
      ? collisionPolicy_->shouldCollide(object, candidate)
      : object.collisionEnabledWith(candidate);
    if (enabled && targetLevel.overlapsStatic(index, candidate)) return true;
  }

  for (const Object* object : entities_.all()) {
    if (!object || object == ignored || object == &candidate) continue;
    const bool enabled = collisionPolicy_
      ? collisionPolicy_->shouldCollide(*object, candidate)
      : object->collisionEnabledWith(candidate);
    if (!enabled) continue;

    const Object* overlapObject = object;
    Object mappedProxy;
    if (
      !candidate.location.liminalObjectId.empty() &&
      candidate.location.liminalObjectId == object->location.liminalObjectId &&
      candidate.location.levelId != object->location.levelId
    ) {
      Vec3 mappedPosition;
      if (mapLiminalPosition(object->location, targetLevelId, mappedPosition)) {
        mappedProxy = renderProxy(*object, mappedPosition, targetLevelId);
        overlapObject = &mappedProxy;
      }
    }

    if (candidate.overlaps(*overlapObject)) return true;
  }
  return false;
}

bool World::containsPosition(const std::string& levelId, const Vec3& position) const {
  std::size_t index = levelIndex(levelId);
  if (index >= levelXYBounds_.size()) index = activeLevelIndex_;
  const LevelXYBounds& bounds = levelXYBounds_[index];
  return bounds.unrestricted || (
    position.x >= bounds.minimumX && position.x <= bounds.maximumX &&
    position.y >= bounds.minimumY && position.y <= bounds.maximumY
  );
}

bool World::pickWalkableSurface(const Ray& ray, SceneSurfaceHit& hit) const {
  if (!traceEnvironment(ray, hit)) return false;
  return hit.walkable;
}

bool World::walkableSurfaceAt(
  const std::string& levelId,
  float x,
  float y,
  SceneSurfaceHit& hit
) const {
  const std::size_t index = levelIndex(levelId);
  if (index >= levels_.size()) return false;
  return levels_[index]->walkableSurfaceAt(x, y, hit);
}

bool World::resolveWalkablePosition(
  const Object& object,
  const std::string& levelId,
  const Vec3& requested,
  float referenceZ,
  float maxStepUp,
  float maxDrop,
  Vec3& resolved
) const {
  if (!containsPosition(levelId, requested)) return false;

  SceneSurfaceHit support;
  if (!walkableSurfaceAt(levelId, requested.x, requested.y, support)) return false;

  const float originZ = support.point.z - object.hitBox.minimum.z;
  const float deltaZ = originZ - referenceZ;
  if (deltaZ > maxStepUp + 1e-4f || deltaZ < -maxDrop - 1e-4f) return false;

  Object probe = object;
  probe.location.levelId = levelId;
  probe.location.position = {requested.x, requested.y, originZ};
  probe.location.liminalObjectId = liminalObjectAt(levelId, probe.location.position);
  if (collidesWith(probe, &object)) return false;

  resolved = probe.location.position;
  return true;
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
  levels_[activeLevelIndex_]->setResident(false);
  activeLevelIndex_ = index;
  levels_[activeLevelIndex_]->setResident(true);
  runtimeRenderCachePrepared_ = false;
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
