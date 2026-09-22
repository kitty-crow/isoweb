#include "demo/DemoObstacles.hpp"
namespace isoweb { namespace demo {
DemoObstacleSystem::DemoObstacleSystem(engine::World& world):behaviours_(world){
 using namespace engine;
 auto box=[](Vec3 a,Vec3 b){HitBox h;h.minimum=a;h.maximum=b;return h;};
 auto add=[&](const char*id,const char*level,Vec3 pos,Vec3 f,HitBox hit,SurfaceTextureMode mode=SurfaceTextureMode::TileLocal){
  DynamicEntityDefinition d;d.id=id;d.worldId="demo";d.timelineId="default";d.levelId=level;d.position=pos;d.forward=f;d.hitBox=hit;d.textureMode=mode;d.textureWorldUnitsPerTile=.5f;d.collisionTags.push_back("world-behaviour");behaviours_.addEntity(d);
 };
 add("demo-obstacle-barrier-left","middle",{0,1.9f,0},{0,1,0},box({-.1f,-.11f,0},{.1f,.11f,1.3f}),SurfaceTextureMode::TileWorld);
 add("demo-obstacle-barrier-right","middle",{0,1.9f,0},{0,1,0},box({-.1f,-.11f,0},{.1f,.11f,1.3f}),SurfaceTextureMode::TileWorld);
 add("demo-obstacle-guillotine","upper",{0,2.15f,2.25f},{0,1,0},box({-4.48f,-.12f,-.12f},{4.48f,.12f,.12f}));
 add("demo-obstacle-blade-a","lower",{2.15f,-3.82f,0},{0,1,0},box({-.11f,.16f,.08f},{.11f,1.08f,.28f}));
 add("demo-obstacle-blade-b","lower",{2.15f,-3.82f,0},{0,-1,0},box({-.11f,.16f,.08f},{.11f,1.08f,.28f}));
 OscillatingGateDefinition gate;gate.leftId="demo-obstacle-barrier-left";gate.rightId="demo-obstacle-barrier-right";gate.base={0,1.9f,0};gate.halfSpan=4.48f;gate.gap=2.35f;gate.sweep=2.15f;gate.angularSpeed=.24f;gate.halfThickness=.11f;gate.height=1.3f;behaviours_.addGate(gate);
 VerticalCycleDefinition guillotine;guillotine.entityId="demo-obstacle-guillotine";guillotine.base={0,2.15f,0};guillotine.upZ=2.25f;guillotine.downZ=.28f;guillotine.period=2.6f;guillotine.blockOnSafeContact=true;guillotine.lethalFace=HazardFace::Bottom;guillotine.contactTolerance=.028f;behaviours_.addVerticalCycle(guillotine);
 RotationDefinition blades;blades.entityIds={"demo-obstacle-blade-a","demo-obstacle-blade-b"};blades.directionMultipliers={1.0f,-1.0f};blades.angularSpeed=2.35f;behaviours_.addRotation(blades);
 HazardDefinition x;x.tolerance=.028f;x.entityId="demo-obstacle-barrier-left";x.face=HazardFace::Right;behaviours_.addHazard(x);x.entityId="demo-obstacle-barrier-right";x.face=HazardFace::Left;behaviours_.addHazard(x);x.entityId="demo-obstacle-guillotine";x.face=HazardFace::Bottom;behaviours_.addHazard(x);x.entityId="demo-obstacle-blade-a";x.face=HazardFace::Any;behaviours_.addHazard(x);x.entityId="demo-obstacle-blade-b";behaviours_.addHazard(x);
}
}}
