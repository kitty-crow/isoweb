#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "engine/world/BehaviourSystem.hpp"
namespace isoweb { namespace demo {
class DemoObstacleSystem {
public:
 explicit DemoObstacleSystem(engine::World& world);
 bool enabled()const{return behaviours_.enabled();}
 void setEnabled(bool enabled){behaviours_.setEnabled(enabled);}
 std::vector<std::string> tick(float dt){return behaviours_.tick(dt);}
 bool isObstacleCharacter(const engine::Character& c)const{return behaviours_.isBehaviourEntity(c);}
 std::size_t partCount()const{return behaviours_.partCount();}
private: engine::BehaviourSystem behaviours_;
};
}}
