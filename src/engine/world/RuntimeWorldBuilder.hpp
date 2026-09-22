#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "engine/world/LiminalObject.hpp"
#include "engine/world/RuntimeWorld.hpp"

namespace isoweb {
namespace engine {

class RuntimeWorldBuilder {
public:
  struct LevelBuild {
    std::string id;
    Vec3 viewOrigin;
    RuntimeLevelDefinition definition;
  };

  void begin(
    std::size_t defaultLevelIndex,
    std::size_t lowerPreviewDepth,
    float lowerPreviewResolutionScale
  );
  void cancel();

  std::size_t addLevel(
    const std::string& id,
    const Vec3& viewOrigin,
    const Vec3& lightPosition,
    const Vec3& floorDark,
    const Vec3& floorLight,
    const Vec3& wallColour,
    const Vec3& boundsFocus
  );

  bool addGround(std::size_t levelIndex, const RuntimeGroundRegion& ground);
  bool addRoom(std::size_t levelIndex, const Room& room);
  bool addRoomConnection(std::size_t levelIndex, const RoomConnection& connection);
  bool addPrimitive(std::size_t levelIndex, const RuntimePrimitive& primitive);
  bool addFloorHole(std::size_t levelIndex, const RuntimeFloorHole& hole);
  bool addStaircase(std::size_t levelIndex, const RuntimeStaircase& staircase);

  std::size_t addConnector(const NavigationLink& connector);
  bool addConnectorForwardSample(std::size_t connectorIndex, const Vec3& sample);
  bool addConnectorReverseSample(std::size_t connectorIndex, const Vec3& sample);

  bool validate(std::string* error = nullptr) const;
  bool commit(World& world, std::string* error = nullptr);

  bool building() const { return building_; }
  std::size_t levelCount() const { return levels_.size(); }

private:
  bool validLevelIndex(std::size_t index) const { return index < levels_.size(); }

  bool building_ = false;
  std::size_t defaultLevelIndex_ = 0;
  std::size_t lowerPreviewDepth_ = 0;
  float lowerPreviewResolutionScale_ = 0.25f;
  std::vector<LevelBuild> levels_;
  std::vector<NavigationLink> connectors_;
};

} // namespace engine
} // namespace isoweb
