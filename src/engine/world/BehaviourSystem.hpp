#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "engine/world/Character.hpp"
#include "engine/world/World.hpp"
namespace isoweb { namespace engine {
enum class HazardFace { Any, Left, Right, Back, Front, Bottom, Top };
struct DynamicEntityDefinition {
  std::string id, worldId, timelineId, levelId;
  Vec3 position, forward = {0,1,0};
  HitBox hitBox;
  SurfaceTextureMode textureMode = SurfaceTextureMode::TileLocal;
  float textureWorldUnitsPerTile = 1.0f;
  std::vector<std::string> collisionTags;
};
struct OscillatingGateDefinition {
  std::string leftId, rightId;
  Vec3 base;
  float halfSpan=0, gap=0, sweep=0, angularSpeed=0, halfThickness=0, height=0, phase=0;
};
struct VerticalCycleDefinition {
  std::string entityId;
  Vec3 base;
  float upZ=0, downZ=0, period=1, phase=0;
  bool blockOnSafeContact=false;
  HazardFace lethalFace=HazardFace::Bottom;
  float contactTolerance=0.028f;
};
struct RotationDefinition { std::vector<std::string> entityIds; float angularSpeed=0, phase=0; };
struct HazardDefinition { std::string entityId; HazardFace face=HazardFace::Any; float tolerance=0.028f; };
class BehaviourSystem {
public:
 explicit BehaviourSystem(World& world):world_(world){}
 bool enabled() const{return enabled_;}
 void setEnabled(bool);
 void clearDefinitions();
 void addEntity(const DynamicEntityDefinition&);
 void addGate(const OscillatingGateDefinition&);
 void addVerticalCycle(const VerticalCycleDefinition&);
 void addRotation(const RotationDefinition&);
 void addHazard(const HazardDefinition&);
 std::vector<std::string> tick(float);
 bool isBehaviourEntity(const Character&) const;
 std::size_t partCount() const{return spawnedIds_.size();}
private:
 World& world_; bool enabled_=false;
 std::vector<DynamicEntityDefinition> entities_;
 std::vector<OscillatingGateDefinition> gates_;
 std::vector<VerticalCycleDefinition> verticals_;
 std::vector<RotationDefinition> rotations_;
 std::vector<HazardDefinition> hazards_;
 std::vector<std::string> spawnedIds_;
 void spawn(); void remove();
 Character* part(const std::string&); const Character* part(const std::string&) const;
 static bool touchesFace(const Object&,const Object&,HazardFace,float);
};
}}
