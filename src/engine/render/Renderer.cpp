#include "engine/render/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace isoweb {
namespace engine {
namespace {

constexpr float RAY_EPSILON = 0.0015f;
constexpr float FAR_DISTANCE = 1000.0f;

const unsigned char* glyphRows(char glyph) {
  static const unsigned char S[7] = {31, 16, 16, 31, 1, 1, 31};
  static const unsigned char R[7] = {30, 17, 17, 30, 20, 18, 17};
  static const unsigned char L[7] = {16, 16, 16, 16, 16, 16, 31};
  static const unsigned char B[7] = {30, 17, 17, 30, 17, 17, 30};
  static const unsigned char T[7] = {31, 4, 4, 4, 4, 4, 4};
  static const unsigned char t[7] = {4, 4, 31, 4, 4, 5, 2};

  switch (glyph) {
    case 'S': return S;
    case 'R': return R;
    case 'L': return L;
    case 'B': return B;
    case 'T': return T;
    case 't': return t;
    default: return nullptr;
  }
}

bool glyphPixel(char glyph, int x, int y) {
  if (x < 0 || x >= 5 || y < 0 || y >= 7) return false;
  const unsigned char* rows = glyphRows(glyph);
  if (!rows) return false;
  return (rows[y] & (1u << (4 - x))) != 0;
}

std::string faceLabel(ObjectFace face) {
  switch (face) {
    case ObjectFace::Front: return "S";
    case ObjectFace::Back: return "B";
    case ObjectFace::Left: return "L";
    case ObjectFace::Right: return "R";
    case ObjectFace::Top: return "T";
    case ObjectFace::Bottom: return "Bt";
  }
  return "";
}

void faceUv(
  const Character& character,
  const ObjectRayHit& hit,
  float& u,
  float& v
) {
  const HitBox& box = character.hitBox;
  const Vec3 size = box.size();

  const auto normalised = [](float value, float minimum, float span) {
    return std::fabs(span) > 1e-7f ? (value - minimum) / span : 0.5f;
  };

  switch (hit.face) {
    case ObjectFace::Front:
      u = normalised(hit.localPoint.x, box.minimum.x, size.x);
      v = normalised(hit.localPoint.z, box.minimum.z, size.z);
      break;
    case ObjectFace::Back:
      u = 1.0f - normalised(hit.localPoint.x, box.minimum.x, size.x);
      v = normalised(hit.localPoint.z, box.minimum.z, size.z);
      break;
    case ObjectFace::Left:
      u = normalised(hit.localPoint.y, box.minimum.y, size.y);
      v = normalised(hit.localPoint.z, box.minimum.z, size.z);
      break;
    case ObjectFace::Right:
      u = 1.0f - normalised(hit.localPoint.y, box.minimum.y, size.y);
      v = normalised(hit.localPoint.z, box.minimum.z, size.z);
      break;
    case ObjectFace::Top:
      u = normalised(hit.localPoint.x, box.minimum.x, size.x);
      v = normalised(hit.localPoint.y, box.minimum.y, size.y);
      break;
    case ObjectFace::Bottom:
      u = normalised(hit.localPoint.x, box.minimum.x, size.x);
      v = 1.0f - normalised(hit.localPoint.y, box.minimum.y, size.y);
      break;
  }

  u = std::max(0.0f, std::min(1.0f, u));
  v = std::max(0.0f, std::min(1.0f, v));
}

bool labelPixel(ObjectFace face, float u, float v) {
  const std::string label = faceLabel(face);
  if (label.empty()) return false;

  const int glyphCount = static_cast<int>(label.size());
  const int totalColumns = glyphCount == 1 ? 5 : glyphCount * 5 + (glyphCount - 1);
  const float width = glyphCount == 1 ? 0.62f : 0.82f;
  const float height = 0.70f;
  const float left = (1.0f - width) * 0.5f;
  const float bottom = (1.0f - height) * 0.5f;

  if (u < left || u >= left + width || v < bottom || v >= bottom + height) return false;

  const float localU = (u - left) / width;
  const float localV = (v - bottom) / height;
  const int column = std::min(totalColumns - 1, static_cast<int>(localU * totalColumns));
  const int row = std::min(6, static_cast<int>((1.0f - localV) * 7.0f));

  if (glyphCount == 1) return glyphPixel(label[0], column, row);

  const int glyphWidthWithGap = 6;
  const int glyphIndex = column / glyphWidthWithGap;
  const int glyphColumn = column % glyphWidthWithGap;
  if (glyphIndex < 0 || glyphIndex >= glyphCount || glyphColumn >= 5) return false;
  return glyphPixel(label[glyphIndex], glyphColumn, row);
}

Vec3 faceBaseColour(ObjectFace face) {
  switch (face) {
    case ObjectFace::Front: return {0.88f, 0.88f, 0.90f};
    case ObjectFace::Back: return {0.70f, 0.72f, 0.76f};
    case ObjectFace::Left: return {0.80f, 0.83f, 0.87f};
    case ObjectFace::Right: return {0.80f, 0.83f, 0.87f};
    case ObjectFace::Top: return {0.94f, 0.94f, 0.96f};
    case ObjectFace::Bottom: return {0.56f, 0.58f, 0.62f};
  }
  return {0.8f, 0.8f, 0.8f};
}

Vec3 characterDebugColour(const Character& character, const ObjectRayHit& hit) {
  float u = 0.5f;
  float v = 0.5f;
  faceUv(character, hit, u, v);

  const bool edge = u < 0.035f || u > 0.965f || v < 0.035f || v > 0.965f;
  if (edge || labelPixel(hit.face, u, v)) {
    return character.selected
      ? Vec3(0.02f, 0.05f, 0.12f)
      : Vec3(0.035f, 0.04f, 0.05f);
  }

  Vec3 colour = faceBaseColour(hit.face);
  const Vec3 lightDirection = normalise(Vec3(-0.35f, -0.45f, 1.0f));
  const float diffuse = std::max(0.0f, dot(hit.normal, lightDirection));
  colour = colour * (0.58f + diffuse * 0.42f);

  if (character.selected) {
    colour = colour * 0.38f + character.selectionTint * 0.62f;
  }
  return colour;
}

} // namespace

Renderer::Renderer(const IWorld& world, Camera& camera, ControlSprites& controls)
    : world_(world), camera_(camera), controls_(controls) {}

void Renderer::resize(int width, int height) {
  frameWidth_ = std::max(160, std::min(1600, width));
  frameHeight_ = std::max(160, std::min(1600, height));
}

bool Renderer::canPan() const {
  return camera_.canPan(frameWidth_, frameHeight_, world_.bounds());
}

CameraControlState Renderer::cameraControlState() const {
  return camera_.controlState(frameWidth_, frameHeight_, world_.bounds());
}

float Renderer::viewHeight() const {
  return camera_.viewHeight(frameWidth_, frameHeight_, world_.bounds());
}

float Renderer::wholeZoomScale() const {
  return camera_.wholeZoomScale(frameWidth_, frameHeight_, world_.bounds());
}

std::uint8_t Renderer::toByte(float value) {
  value = std::pow(std::max(0.0f, std::min(1.0f, value)), 1.0f / 2.2f);
  return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
}

void Renderer::ensureFrame() {
  if (!dsr::image_exists(frame_) || allocatedFrameWidth_ != frameWidth_ || allocatedFrameHeight_ != frameHeight_) {
    frame_ = dsr::image_create_RgbaU8(frameWidth_, frameHeight_, false);
    allocatedFrameWidth_ = frameWidth_;
    allocatedFrameHeight_ = frameHeight_;
  }

  const std::size_t required = static_cast<std::size_t>(frameWidth_) * frameHeight_ * 4;
  if (rgba_.size() != required) rgba_.resize(required);
}

Ray Renderer::makeRay(float px, float py) const {
  const Vec3 forward = camera_.forward();
  const Vec3 right = normalise(cross(forward, {0.0f, 0.0f, 1.0f}));
  const Vec3 up = normalise(cross(right, forward));
  const float aspect = static_cast<float>(frameWidth_) / frameHeight_;
  const float height = viewHeight();
  const float width = height * aspect;
  const float screenX = (px / frameWidth_ - 0.5f) * width;
  const float screenY = (0.5f - py / frameHeight_) * height;

  const WorldBounds& bounds = world_.bounds();
  const Vec3 focus = canPan()
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;

  return {focus - forward * 9.0f + right * screenX + up * screenY, forward};
}

bool Renderer::characterOnActiveLevel(const Character& character) const {
  return character.location.levelId.empty() || character.location.levelId == world_.activeLevelId();
}

float Renderer::approximateSceneDepth(const Ray& ray) const {
  float nearest = FAR_DISTANCE;

  for (const Object& object : world_.objects()) {
    ObjectRayHit hit;
    if (object.rayIntersection(ray, RAY_EPSILON, nearest, hit)) nearest = hit.t;
  }

  if (std::fabs(ray.direction.z) > 1e-7f) {
    const float groundT = -ray.origin.z / ray.direction.z;
    if (groundT >= RAY_EPSILON && groundT < nearest) nearest = groundT;
  }
  return nearest;
}

bool Renderer::screenToGround(float pixelX, float pixelY, Vec3& point) const {
  const Ray ray = makeRay(pixelX, pixelY);
  if (std::fabs(ray.direction.z) < 1e-7f) return false;
  const float t = -ray.origin.z / ray.direction.z;
  if (t < RAY_EPSILON) return false;
  point = ray.origin + ray.direction * t;
  return true;
}

int Renderer::pickCharacter(float pixelX, float pixelY) const {
  const Ray ray = makeRay(pixelX, pixelY);
  float nearest = approximateSceneDepth(ray);
  int picked = -1;

  const std::vector<Character>& characters = world_.characters();
  for (std::size_t index = 0; index < characters.size(); ++index) {
    const Character& character = characters[index];
    if (!characterOnActiveLevel(character)) continue;

    ObjectRayHit hit;
    if (character.rayIntersection(ray, RAY_EPSILON, nearest, hit)) {
      nearest = hit.t;
      picked = static_cast<int>(index);
    }
  }
  return picked;
}

void Renderer::render() {
  ensureFrame();
  const float offsets[2] = {0.25f, 0.75f};

  for (int y = 0; y < frameHeight_; ++y) {
    for (int x = 0; x < frameWidth_; ++x) {
      Vec3 colour;
      for (int sampleY = 0; sampleY < 2; ++sampleY) {
        for (int sampleX = 0; sampleX < 2; ++sampleX) {
          const float px = x + offsets[sampleX];
          const float py = y + offsets[sampleY];
          const Ray ray = makeRay(px, py);
          Vec3 sampleColour = world_.sample(ray, py / frameHeight_);

          float nearest = approximateSceneDepth(ray);
          const Character* visibleCharacter = nullptr;
          ObjectRayHit visibleHit;

          for (const Character& character : world_.characters()) {
            if (!characterOnActiveLevel(character) || character.hasArtwork()) continue;

            ObjectRayHit hit;
            if (character.rayIntersection(ray, RAY_EPSILON, nearest, hit)) {
              nearest = hit.t;
              visibleCharacter = &character;
              visibleHit = hit;
            }
          }

          if (visibleCharacter) {
            sampleColour = characterDebugColour(*visibleCharacter, visibleHit);
          }
          colour = colour + sampleColour;
        }
      }
      colour = colour * 0.25f;
      dsr::image_writePixel(
        frame_,
        x,
        y,
        {toByte(colour.x), toByte(colour.y), toByte(colour.z), 255}
      );
    }
  }

  const CameraControlState cameraState = cameraControlState();
  LevelControlState levelState;
  levelState.canMoveUp = world_.activeLevelIndex() + 1 < world_.levelCount();
  levelState.canMoveDown = world_.activeLevelIndex() > 0;
  levelState.atDefault = world_.activeLevelIndex() == world_.defaultLevelIndex();
  controls_.draw(frame_, frameWidth_, frameHeight_, cameraState, levelState);

  for (int y = 0; y < frameHeight_; ++y) {
    for (int x = 0; x < frameWidth_; ++x) {
      const auto colour = dsr::image_readPixel_border(frame_, x, y);
      const std::size_t index = static_cast<std::size_t>((y * frameWidth_ + x) * 4);
      rgba_[index] = colour.red;
      rgba_[index + 1] = colour.green;
      rgba_[index + 2] = colour.blue;
      rgba_[index + 3] = 255;
    }
  }
}

} // namespace engine
} // namespace isoweb
