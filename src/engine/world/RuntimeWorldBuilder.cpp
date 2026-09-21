#include "engine/world/RuntimeWorldBuilder.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace isoweb {
namespace engine {
namespace {

bool finite(float value) {
  return std::isfinite(value);
}

bool finiteVec(const Vec3& value) {
  return finite(value.x) && finite(value.y) && finite(value.z);
}

bool fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

bool validPortal(const RoomLayout& layout, const RoomPortal& portal) {
  return !portal.roomId.empty() && layout.room(portal.roomId) != nullptr &&
    finite(portal.offset) && finite(portal.width) && portal.width > 0.0f;
}

} // namespace

void RuntimeWorldBuilder::begin(
  std::size_t defaultLevelIndex,
  std::size_t lowerPreviewDepth,
  float lowerPreviewResolutionScale
) {
  building_ = true;
  defaultLevelIndex_ = defaultLevelIndex;
  lowerPreviewDepth_ = lowerPreviewDepth;
  lowerPreviewResolutionScale_ = lowerPreviewResolutionScale;
  levels_.clear();
  connectors_.clear();
}

void RuntimeWorldBuilder::cancel() {
  building_ = false;
  levels_.clear();
  connectors_.clear();
}

std::size_t RuntimeWorldBuilder::addLevel(
  const std::string& id,
  const Vec3& viewOrigin,
  const Vec3& lightPosition,
  const Vec3& floorDark,
  const Vec3& floorLight,
  const Vec3& wallColour,
  const Vec3& boundsFocus
) {
  if (!building_) return std::numeric_limits<std::size_t>::max();
  LevelBuild level;
  level.id = id;
  level.viewOrigin = viewOrigin;
  level.definition.lightPosition = lightPosition;
  level.definition.floorDark = floorDark;
  level.definition.floorLight = floorLight;
  level.definition.wallColour = wallColour;
  level.definition.boundsFocus = boundsFocus;
  level.definition.roomLayout.previewFloorDark = floorDark;
  level.definition.roomLayout.previewFloorLight = floorLight;
  level.definition.roomLayout.previewWall = wallColour;
  levels_.push_back(std::move(level));
  return levels_.size() - 1;
}

bool RuntimeWorldBuilder::addGround(
  std::size_t levelIndex,
  const RuntimeGroundRegion& ground
) {
  if (!building_ || !validLevelIndex(levelIndex)) return false;
  levels_[levelIndex].definition.ground.push_back(ground);
  return true;
}

bool RuntimeWorldBuilder::addRoom(std::size_t levelIndex, const Room& room) {
  if (!building_ || !validLevelIndex(levelIndex)) return false;
  levels_[levelIndex].definition.roomLayout.rooms.push_back(room);
  return true;
}

bool RuntimeWorldBuilder::addRoomConnection(
  std::size_t levelIndex,
  const RoomConnection& connection
) {
  if (!building_ || !validLevelIndex(levelIndex)) return false;
  levels_[levelIndex].definition.roomLayout.connections.push_back(connection);
  return true;
}

bool RuntimeWorldBuilder::addPrimitive(
  std::size_t levelIndex,
  const RuntimePrimitive& primitive
) {
  if (!building_ || !validLevelIndex(levelIndex)) return false;
  levels_[levelIndex].definition.objects.push_back(primitive);
  return true;
}

bool RuntimeWorldBuilder::addFloorHole(
  std::size_t levelIndex,
  const RuntimeFloorHole& hole
) {
  if (!building_ || !validLevelIndex(levelIndex)) return false;
  levels_[levelIndex].definition.floorHoles.push_back(hole);
  return true;
}

bool RuntimeWorldBuilder::addStaircase(
  std::size_t levelIndex,
  const RuntimeStaircase& staircase
) {
  if (!building_ || !validLevelIndex(levelIndex)) return false;
  levels_[levelIndex].definition.staircases.push_back(staircase);
  return true;
}

std::size_t RuntimeWorldBuilder::addConnector(const NavigationLink& connector) {
  if (!building_) return std::numeric_limits<std::size_t>::max();
  connectors_.push_back(connector);
  return connectors_.size() - 1;
}

bool RuntimeWorldBuilder::addConnectorForwardSample(
  std::size_t connectorIndex,
  const Vec3& sample
) {
  if (!building_ || connectorIndex >= connectors_.size()) return false;
  connectors_[connectorIndex].forwardTraversal.push_back(sample);
  return true;
}

bool RuntimeWorldBuilder::addConnectorReverseSample(
  std::size_t connectorIndex,
  const Vec3& sample
) {
  if (!building_ || connectorIndex >= connectors_.size()) return false;
  connectors_[connectorIndex].reverseTraversal.push_back(sample);
  return true;
}

bool RuntimeWorldBuilder::validate(std::string* error) const {
  if (!building_) return fail(error, "world build has not begun");
  if (levels_.empty()) return fail(error, "world must contain at least one level");
  if (defaultLevelIndex_ >= levels_.size()) return fail(error, "default level index is invalid");
  if (!finite(lowerPreviewResolutionScale_) || lowerPreviewResolutionScale_ <= 0.0f) {
    return fail(error, "lower preview resolution scale must be positive");
  }

  std::unordered_set<std::string> levelIds;
  for (const LevelBuild& level : levels_) {
    if (level.id.empty()) return fail(error, "level id must not be empty");
    if (!levelIds.insert(level.id).second) return fail(error, "level ids must be unique");
    if (!finiteVec(level.viewOrigin) || !finiteVec(level.definition.lightPosition) ||
        !finiteVec(level.definition.floorDark) || !finiteVec(level.definition.floorLight) ||
        !finiteVec(level.definition.wallColour) || !finiteVec(level.definition.boundsFocus)) {
      return fail(error, "level contains a non-finite vector");
    }

    for (const RuntimeGroundRegion& ground : level.definition.ground) {
      if (!finiteVec(ground.centre) || !finite(ground.width) || !finite(ground.depth) ||
          ground.width <= 0.0f || ground.depth <= 0.0f) {
        return fail(error, "ground region dimensions must be finite and positive");
      }
    }

    std::unordered_set<std::string> roomIds;
    for (const Room& room : level.definition.roomLayout.rooms) {
      if (room.id.empty() || !roomIds.insert(room.id).second) {
        return fail(error, "room ids must be non-empty and unique within a level");
      }
      if (!finiteVec(room.centre) || !finite(room.width) || !finite(room.depth) ||
          !finite(room.floorZ) || !finite(room.wallHeight) || !finite(room.wallThickness) ||
          room.width <= 0.0f || room.depth <= 0.0f ||
          room.wallHeight < 0.0f || room.wallThickness < 0.0f) {
        return fail(error, "room dimensions must be finite and valid");
      }
    }

    std::unordered_set<std::string> connectionIds;
    for (const RoomConnection& connection : level.definition.roomLayout.connections) {
      if (connection.id.empty() || !connectionIds.insert(connection.id).second) {
        return fail(error, "room connection ids must be non-empty and unique");
      }
      if (!validPortal(level.definition.roomLayout, connection.a) ||
          !validPortal(level.definition.roomLayout, connection.b)) {
        return fail(error, "room connection references an invalid portal");
      }
    }

    for (const RuntimePrimitive& primitive : level.definition.objects) {
      if (!finiteVec(primitive.position) || !finiteVec(primitive.colour) ||
          !finite(primitive.size) || !finite(primitive.height) || primitive.size <= 0.0f) {
        return fail(error, "primitive contains invalid dimensions");
      }
      if (
        primitive.kind != RuntimePrimitiveKind::Sphere &&
        primitive.kind != RuntimePrimitiveKind::Dodecahedron &&
        primitive.kind != RuntimePrimitiveKind::Icosahedron &&
        primitive.height <= 0.0f
      ) {
        return fail(error, "primitive height must be positive");
      }
    }

    for (const RuntimeFloorHole& hole : level.definition.floorHoles) {
      if (!finite(hole.minimumX) || !finite(hole.maximumX) ||
          !finite(hole.minimumY) || !finite(hole.maximumY) ||
          hole.minimumX >= hole.maximumX || hole.minimumY >= hole.maximumY) {
        return fail(error, "floor hole bounds are invalid");
      }
    }

    for (const RuntimeStaircase& staircase : level.definition.staircases) {
      if (!finite(staircase.centreX) || !finite(staircase.startY) ||
          !finite(staircase.endY) || !finite(staircase.startZ) ||
          !finite(staircase.endZ) || !finite(staircase.width) ||
          staircase.width <= 0.0f || staircase.startY == staircase.endY) {
        return fail(error, "staircase dimensions are invalid");
      }
    }
  }

  std::unordered_set<std::string> connectorIds;
  for (const NavigationLink& connector : connectors_) {
    if (connector.id.empty() || !connectorIds.insert(connector.id).second) {
      return fail(error, "connector ids must be non-empty and unique");
    }
    if (levelIds.count(connector.fromLevelId) == 0 ||
        levelIds.count(connector.toLevelId) == 0) {
      return fail(error, "connector references an unknown level");
    }
    if (!finiteVec(connector.fromPosition) || !finiteVec(connector.toPosition)) {
      return fail(error, "connector endpoint is not finite");
    }
    for (const Vec3& sample : connector.forwardTraversal) {
      if (!finiteVec(sample)) return fail(error, "connector forward sample is not finite");
    }
    for (const Vec3& sample : connector.reverseTraversal) {
      if (!finiteVec(sample)) return fail(error, "connector reverse sample is not finite");
    }
  }

  return true;
}

bool RuntimeWorldBuilder::commit(World& world, std::string* error) {
  if (!validate(error)) return false;

  std::vector<std::unique_ptr<IWorldLevel>> runtimeLevels;
  runtimeLevels.reserve(levels_.size());
  for (const LevelBuild& level : levels_) {
    runtimeLevels.push_back(makeRuntimeWorldLevel(level.definition));
  }

  if (!world.replaceLevels(std::move(runtimeLevels), defaultLevelIndex_)) {
    return fail(error, "runtime rejected replacement levels");
  }

  // Rename through collision-free temporary IDs so authored IDs may legally be
  // numeric strings that overlap the constructor's initial index-based IDs.
  for (std::size_t index = 0; index < levels_.size(); ++index) {
    if (!world.setLevelId(index, "__isoweb_build_" + std::to_string(index))) {
      return fail(error, "could not assign temporary level id");
    }
  }
  for (std::size_t index = 0; index < levels_.size(); ++index) {
    if (!world.setLevelId(index, levels_[index].id)) {
      return fail(error, "could not assign authored level id");
    }
    if (!world.setLevelViewOrigin(levels_[index].id, levels_[index].viewOrigin)) {
      return fail(error, "could not assign level view origin");
    }
    if (!world.setLevelLight(
      levels_[index].id,
      levels_[index].definition.lightPosition
    )) {
      return fail(error, "could not assign level light");
    }
  }

  world.setLowerLevelPreviewDepth(lowerPreviewDepth_);
  world.setLowerPreviewResolutionScale(lowerPreviewResolutionScale_);
  world.setNavigationLinks(connectors_);
  building_ = false;
  return true;
}

} // namespace engine
} // namespace isoweb
