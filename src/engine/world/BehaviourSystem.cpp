#include "engine/world/BehaviourSystem.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
namespace isoweb { namespace engine {
namespace {
constexpr float PI=3.14159265358979323846f;
float overlap(float a,float b,float c,float d){return std::min(b,d)-std::max(a,c);}
float cycleHeight(float phase,float up,float down){
 const float c=phase-std::floor(phase);
 if(c<.34f)return up;
 if(c<.49f){float t=(c-.34f)/.15f,e=t*t*(3-2*t);return up*(1-e)+down*e;}
 if(c<.66f)return down;
 float t=(c-.66f)/.34f,e=t*t*(3-2*t);return down*(1-e)+up*e;
}
}
void BehaviourSystem::clearDefinitions(){setEnabled(false);entities_.clear();gates_.clear();verticals_.clear();rotations_.clear();hazards_.clear();}
void BehaviourSystem::addEntity(const DynamicEntityDefinition& d){entities_.push_back(d);if(enabled_) { setEnabled(false); setEnabled(true); }}
bool BehaviourSystem::addEntityCollisionTag(const std::string& id,const std::string& tag){for(auto& d:entities_)if(d.id==id){d.collisionTags.push_back(tag);return true;}return false;}
void BehaviourSystem::addGate(const OscillatingGateDefinition& d){gates_.push_back(d);}
void BehaviourSystem::addVerticalCycle(const VerticalCycleDefinition& d){verticals_.push_back(d);}
void BehaviourSystem::addRotation(const RotationDefinition& d){rotations_.push_back(d);}
void BehaviourSystem::addHazard(const HazardDefinition& d){hazards_.push_back(d);}
void BehaviourSystem::setEnabled(bool e){if(e==enabled_)return;remove();enabled_=e;for(auto&g:gates_)g.phase=0;for(auto&v:verticals_)v.phase=0;for(auto&r:rotations_)r.phase=0;if(enabled_)spawn();}
void BehaviourSystem::spawn(){
 for(const auto& d:entities_){
  std::unique_ptr<Character> p(new Character());p->id=d.id;p->location.worldId=d.worldId;p->location.timelineId=d.timelineId;p->location.levelId=d.levelId;p->location.position=d.position;p->forward=d.forward;p->hitBox=d.hitBox;p->solid=true;p->npc=true;p->controllable=false;p->movementSpeedMultiplier=0;p->surfaceTextureMode=d.textureMode;p->textureWorldUnitsPerTile=d.textureWorldUnitsPerTile;p->collisionTags=d.collisionTags;
  world_.entities().add(std::move(p));spawnedIds_.push_back(d.id);
 }
 tick(0);
}
void BehaviourSystem::remove(){for(const auto&id:spawnedIds_)world_.entities().remove(id);spawnedIds_.clear();}
Character* BehaviourSystem::part(const std::string&id){return dynamic_cast<Character*>(world_.entities().find(id));}
const Character* BehaviourSystem::part(const std::string&id)const{return dynamic_cast<const Character*>(world_.entities().find(id));}
bool BehaviourSystem::isBehaviourEntity(const Character& c)const{return std::find(spawnedIds_.begin(),spawnedIds_.end(),c.id)!=spawnedIds_.end();}
bool BehaviourSystem::touchesFace(const Object&o,const Object&x,HazardFace f,float tol){
 if(!o.location.sharesSpaceWith(x.location))return false;
 Vec3 mn(std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()),mx=-mn;
 for(int a=0;a<2;a++)for(int b=0;b<2;b++)for(int c=0;c<2;c++){Vec3 q(a?x.hitBox.maximum.x:x.hitBox.minimum.x,b?x.hitBox.maximum.y:x.hitBox.minimum.y,c?x.hitBox.maximum.z:x.hitBox.minimum.z);q=o.worldToLocal(x.localToWorld(q));mn.x=std::min(mn.x,q.x);mn.y=std::min(mn.y,q.y);mn.z=std::min(mn.z,q.z);mx.x=std::max(mx.x,q.x);mx.y=std::max(mx.y,q.y);mx.z=std::max(mx.z,q.z);}
 const HitBox& h=o.hitBox;Vec3 cc=(mn+mx)*.5f,oc=h.centre();float ox=overlap(mn.x-tol,mx.x+tol,h.minimum.x,h.maximum.x),oy=overlap(mn.y-tol,mx.y+tol,h.minimum.y,h.maximum.y),oz=overlap(mn.z-tol,mx.z+tol,h.minimum.z,h.maximum.z);
 if(f==HazardFace::Any)return touchesFace(o,x,HazardFace::Left,tol)||touchesFace(o,x,HazardFace::Right,tol)||touchesFace(o,x,HazardFace::Back,tol)||touchesFace(o,x,HazardFace::Front,tol)||touchesFace(o,x,HazardFace::Bottom,tol)||touchesFace(o,x,HazardFace::Top,tol);
 if(f==HazardFace::Left)return cc.x<=oc.x&&oy>0&&oz>0&&mx.x>=h.minimum.x-tol&&mn.x<=h.minimum.x+tol;
 if(f==HazardFace::Right)return cc.x>=oc.x&&oy>0&&oz>0&&mn.x<=h.maximum.x+tol&&mx.x>=h.maximum.x-tol;
 if(f==HazardFace::Back)return cc.y<=oc.y&&ox>0&&oz>0&&mx.y>=h.minimum.y-tol&&mn.y<=h.minimum.y+tol;
 if(f==HazardFace::Front)return cc.y>=oc.y&&ox>0&&oz>0&&mn.y<=h.maximum.y+tol&&mx.y>=h.maximum.y-tol;
 if(f==HazardFace::Bottom)return cc.z<=oc.z&&ox>0&&oy>0&&mx.z>=h.minimum.z-tol&&mn.z<=h.minimum.z+tol;
 return cc.z>=oc.z&&ox>0&&oy>0&&mn.z<=h.maximum.z+tol&&mx.z>=h.maximum.z-tol;
}
std::vector<std::string> BehaviourSystem::tick(float dt){
 if(!enabled_)return{};dt=std::max(0.f,dt);
 for(auto&g:gates_){g.phase=std::fmod(g.phase+dt*g.angularSpeed,PI*2);float centre=std::sin(g.phase)*g.sweep,half=g.gap*.5f;auto set=[&](Character*p,float a,float b){if(!p)return;float cx=(a+b)*.5f,h=std::max(.01f,(b-a)*.5f);p->location.position={cx,g.base.y,g.base.z};p->hitBox.minimum={-h,-g.halfThickness,0};p->hitBox.maximum={h,g.halfThickness,g.height};};set(part(g.leftId),-g.halfSpan,centre-half);set(part(g.rightId),centre+half,g.halfSpan);}
 for(auto&v:verticals_){Character*p=part(v.entityId);if(!p)continue;float old=v.phase;Vec3 pos=p->location.position;v.phase+=dt/v.period;p->location.position=v.base+Vec3(0,0,cycleHeight(v.phase,v.upZ,v.downZ));if(v.blockOnSafeContact){for(const Character*c:world_.entities().characters())if(c&&!isBehaviourEntity(*c)&&p->overlaps(*c)&&!touchesFace(*p,*c,v.lethalFace,v.contactTolerance)){v.phase=old;p->location.position=pos;break;}}}
 for(auto&r:rotations_){r.phase=std::fmod(r.phase+dt*r.angularSpeed,PI*2);Vec3 f(std::sin(r.phase),std::cos(r.phase),0);for(size_t i=0;i<r.entityIds.size();++i)if(Character*p=part(r.entityIds[i])){const float m=i<r.directionMultipliers.size()?r.directionMultipliers[i]:1.f;p->forward=f*m;}}
 std::set<std::string> hit;for(const auto&h:hazards_){const Character*p=part(h.entityId);if(!p)continue;for(const Character*c:world_.entities().characters())if(c&&!isBehaviourEntity(*c)&&touchesFace(*p,*c,h.face,h.tolerance))hit.insert(c->id);}return {hit.begin(),hit.end()};
}
}}
