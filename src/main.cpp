#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <emscripten/emscripten.h>
#include "DFPSR/api/drawAPI.h"
#include "DFPSR/api/imageAPI.h"

namespace {
int frameWidth=512, frameHeight=288, allocatedFrameWidth=0, allocatedFrameHeight=0;
constexpr int ROTATE_ARROW_WIDTH=56, ROTATE_ARROW_HEIGHT=44, RESET_DISK_SIZE=38;
constexpr int ROTATE_LEFT_X=18, ROTATE_ROW_GAP=8, ZOOM_CONTROL_SIZE=32;
constexpr int ZOOM_LEFT=18, ZOOM_TOP=18, ZOOM_GAP=6, CONTROL_BOTTOM=18;
constexpr int PAN_ARROW_SIZE=38, PAN_X_STEP=48, PAN_Y_STEP=36, PAN_PAD_RIGHT=18, PAN_PAD_BOTTOM=16;
constexpr float EPSILON=0.0015f, FAR_DISTANCE=1000.0f, PI=3.14159265358979323846f;
constexpr float GROUND_LIMIT=4.40f, PAN_LIMIT=GROUND_LIMIT-1.15f, BASE_VIEW_HEIGHT=6.15f, MIN_VIEW_WIDTH=5.50f;

struct Vec3 { float x=0,y=0,z=0; Vec3()=default; Vec3(float x,float y,float z):x(x),y(y),z(z){} Vec3 operator+(const Vec3&o)const{return{x+o.x,y+o.y,z+o.z};} Vec3 operator-(const Vec3&o)const{return{x-o.x,y-o.y,z-o.z};} Vec3 operator*(float s)const{return{x*s,y*s,z*s};} Vec3 operator/(float s)const{return{x/s,y/s,z/s};} };
float dot(const Vec3&a,const Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(const Vec3&a,const Vec3&b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float length(const Vec3&v){return std::sqrt(dot(v,v));}
Vec3 normalise(const Vec3&v){float m=length(v);return m>0?v/m:Vec3();}
Vec3 rotateZ(const Vec3&v,float r){float c=std::cos(r),s=std::sin(r);return{v.x*c-v.y*s,v.x*s+v.y*c,v.z};}
struct Ray{Vec3 origin,direction;};
enum class Surface{None,Ground,Cube,Sphere};
struct Hit{bool found=false;float t=FAR_DISTANCE;Vec3 point,normal;Surface surface=Surface::None;};

struct LevelDefinition {
  Vec3 cubeMin;
  Vec3 cubeMax;
  Vec3 sphereCentre;
  float sphereRadius;
  Vec3 lightPosition;
  Vec3 floorDark;
  Vec3 floorLight;
};

const std::vector<LevelDefinition> levels = {{
  {-1.85f,-0.15f,0.0f},
  {-0.25f,1.45f,1.55f},
  {1.05f,-0.25f,0.90f},
  0.90f,
  {-3.60f,-4.20f,6.50f},
  {0.567f,0.6048f,0.630f},
  {0.621f,0.6624f,0.690f}
}};
const Vec3 BASE_FOCUS(0,0.15f,0.55f);
const LevelDefinition& activeLevel(){return levels.front();}
int cameraYawStep=0, zoomPreset=3; float cameraPanX=0,cameraPanY=0; bool detailedZoomMode=false;

float yawRadians(){return -cameraYawStep*PI*0.25f;}
Vec3 cameraForward(){return rotateZ(normalise({1,1,-1}),yawRadians());}
Vec3 cameraGroundRight(){return normalise(cross(cameraForward(),{0,0,1}));}
Vec3 cameraGroundDown(){Vec3 f=cameraForward();return normalise({f.x,f.y,0});}
bool wholeZoom(){return zoomPreset==0;}
float zoomScale(){switch(zoomPreset){case 5:return 4;case 4:return 2;case 3:return 1;case 2:return .5f;case 1:return .25f;default:return .25f;}}

float wholeViewHeight(float aspect){
  const LevelDefinition& level=activeLevel();
  Vec3 f=cameraForward(),r=normalise(cross(f,{0,0,1})),u=normalise(cross(r,f));
  float minR=1e9f,maxR=-1e9f,minU=1e9f,maxU=-1e9f;
  auto add=[&](Vec3 p){Vec3 q=p-BASE_FOCUS;float pr=dot(q,r),pu=dot(q,u);minR=std::min(minR,pr);maxR=std::max(maxR,pr);minU=std::min(minU,pu);maxU=std::max(maxU,pu);};
  add({-GROUND_LIMIT,-GROUND_LIMIT,0});add({GROUND_LIMIT,-GROUND_LIMIT,0});add({-GROUND_LIMIT,GROUND_LIMIT,0});add({GROUND_LIMIT,GROUND_LIMIT,0});
  for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z)add({x?level.cubeMax.x:level.cubeMin.x,y?level.cubeMax.y:level.cubeMin.y,z?level.cubeMax.z:level.cubeMin.z});
  add(level.sphereCentre+Vec3(level.sphereRadius,0,0));add(level.sphereCentre+Vec3(-level.sphereRadius,0,0));add(level.sphereCentre+Vec3(0,level.sphereRadius,0));add(level.sphereCentre+Vec3(0,-level.sphereRadius,0));add(level.sphereCentre+Vec3(0,0,level.sphereRadius));add(level.sphereCentre+Vec3(0,0,-level.sphereRadius));
  float halfW=std::max(std::fabs(minR),std::fabs(maxR)),halfH=std::max(std::fabs(minU),std::fabs(maxU));
  return std::max(halfH*2.04f,(halfW*2.04f)/aspect);
}
float baseViewHeight(float aspect){return std::max(BASE_VIEW_HEIGHT,MIN_VIEW_WIDTH/aspect);}
float wholeZoomScale(){float a=float(frameWidth)/frameHeight;return baseViewHeight(a)/wholeViewHeight(a);}
float viewHeight(){float a=float(frameWidth)/frameHeight;return wholeZoom()?wholeViewHeight(a):baseViewHeight(a)/zoomScale();}
bool canPan(){float a=float(frameWidth)/frameHeight;return viewHeight()+0.0001f<wholeViewHeight(a);}
int detailedPresetAt(int p){if(wholeZoomScale()>.25f){static const int o[6]={1,0,2,3,4,5};return o[p];}return p;}
int presetAt(int p){return detailedZoomMode?detailedPresetAt(p):p+2;}
int sequenceLength(){return detailedZoomMode?6:3;}
int sequencePosition(){for(int p=0;p<sequenceLength();++p)if(presetAt(p)==zoomPreset)return p;return detailedZoomMode?3:1;}
void stepZoom(int d){int p=std::max(0,std::min(sequenceLength()-1,sequencePosition()+d));zoomPreset=presetAt(p);}
void pan(float right,float down){if(!canPan())return;Vec3 d=cameraGroundRight()*right+cameraGroundDown()*down;cameraPanX=std::max(-PAN_LIMIT,std::min(PAN_LIMIT,cameraPanX+d.x));cameraPanY=std::max(-PAN_LIMIT,std::min(PAN_LIMIT,cameraPanY+d.y));}

bool intersectSphere(const Ray&r,float mn,float mx,Hit&h){const auto&level=activeLevel();Vec3 oc=r.origin-level.sphereCentre;float a=dot(r.direction,r.direction),hb=dot(oc,r.direction),c=dot(oc,oc)-level.sphereRadius*level.sphereRadius,disc=hb*hb-a*c;if(disc<0)return false;float q=std::sqrt(disc),t=(-hb-q)/a;if(t<mn||t>mx){t=(-hb+q)/a;if(t<mn||t>mx)return false;}h.found=true;h.t=t;h.point=r.origin+r.direction*t;h.normal=normalise(h.point-level.sphereCentre);h.surface=Surface::Sphere;return true;}
bool intersectCube(const Ray&r,float mn,float mx,Hit&h){const auto&level=activeLevel();float nearT=mn,farT=mx,o[3]={r.origin.x,r.origin.y,r.origin.z},d[3]={r.direction.x,r.direction.y,r.direction.z},lo[3]={level.cubeMin.x,level.cubeMin.y,level.cubeMin.z},hi[3]={level.cubeMax.x,level.cubeMax.y,level.cubeMax.z};for(int a=0;a<3;++a){if(std::fabs(d[a])<1e-7f){if(o[a]<lo[a]||o[a]>hi[a])return false;continue;}float inv=1/d[a],t0=(lo[a]-o[a])*inv,t1=(hi[a]-o[a])*inv;if(t0>t1)std::swap(t0,t1);nearT=std::max(nearT,t0);farT=std::min(farT,t1);if(farT<nearT)return false;}h.found=true;h.t=nearT;h.point=r.origin+r.direction*nearT;h.surface=Surface::Cube;float best=std::fabs(h.point.x-level.cubeMin.x);h.normal={-1,0,0};auto choose=[&](float q,Vec3 n){if(q<best){best=q;h.normal=n;}};choose(std::fabs(h.point.x-level.cubeMax.x),{1,0,0});choose(std::fabs(h.point.y-level.cubeMin.y),{0,-1,0});choose(std::fabs(h.point.y-level.cubeMax.y),{0,1,0});choose(std::fabs(h.point.z-level.cubeMin.z),{0,0,-1});choose(std::fabs(h.point.z-level.cubeMax.z),{0,0,1});return true;}
bool intersectGround(const Ray&r,float mn,float mx,Hit&h){if(std::fabs(r.direction.z)<1e-7f)return false;float t=-r.origin.z/r.direction.z;if(t<mn||t>mx)return false;Vec3 p=r.origin+r.direction*t;if(std::fabs(p.x)>GROUND_LIMIT||std::fabs(p.y)>GROUND_LIMIT)return false;h.found=true;h.t=t;h.point=p;h.normal={0,0,1};h.surface=Surface::Ground;return true;}
Hit traceClosest(const Ray&r,float mn,float mx){Hit out,h;if(intersectCube(r,mn,mx,h)){out=h;mx=h.t;}h=Hit();if(intersectSphere(r,mn,mx,h)){out=h;mx=h.t;}h=Hit();if(intersectGround(r,mn,mx,h))out=h;return out;}
bool occluded(Vec3 p,Vec3 n){const auto&level=activeLevel();Vec3 to=level.lightPosition-p;float dist=length(to);return traceClosest({p+n*EPSILON,to/dist},EPSILON,dist-EPSILON).found;}
Vec3 material(const Hit&h){const auto&level=activeLevel();if(h.surface==Surface::Cube)return{.18f,.48f,.88f};if(h.surface==Surface::Sphere)return{.95f,.43f,.12f};int x=int(std::floor(h.point.x+20)),y=int(std::floor(h.point.y+20));return((x+y)&1)?level.floorDark:level.floorLight;}
Vec3 shade(const Hit&h){const auto&level=activeLevel();Vec3 base=material(h),to=level.lightPosition-h.point;float dist=length(to),diff=std::max(0.0f,dot(h.normal,to/dist)),att=1/(1+.018f*dist*dist),vis=occluded(h.point,h.normal)?0:1;Vec3 c=base*(.19f+vis*diff*att*1.18f);if(h.surface==Surface::Ground){float ex=std::fabs(h.point.x-std::round(h.point.x)),ey=std::fabs(h.point.y-std::round(h.point.y));if(std::min(ex,ey)<.022f)c=c*.78f;}return{std::min(c.x,1.0f),std::min(c.y,1.0f),std::min(c.z,1.0f)};}
Vec3 background(float y){float t=std::max(0.0f,std::min(1.0f,y));return Vec3(.075f,.12f,.18f)*(1-t)+Vec3(.20f,.28f,.34f)*t;}
Vec3 trace(float px,float py){Vec3 f=cameraForward(),r=normalise(cross(f,{0,0,1})),u=normalise(cross(r,f));float a=float(frameWidth)/frameHeight,vh=viewHeight(),vw=vh*a,sx=(px/frameWidth-.5f)*vw,sy=(.5f-py/frameHeight)*vh;Vec3 focus=canPan()?BASE_FOCUS+Vec3(cameraPanX,cameraPanY,0):BASE_FOCUS;Hit h=traceClosest({focus-f*9+r*sx+u*sy,f},EPSILON,FAR_DISTANCE);return h.found?shade(h):background(py/frameHeight);}
std::uint8_t byte(float v){v=std::pow(std::max(0.0f,std::min(1.0f,v)),1/2.2f);return std::uint8_t(v*255+.5f);}

float segDist(float px,float py,float ax,float ay,float bx,float by){float x=bx-ax,y=by-ay,q=(x*x+y*y),t=q>0?std::max(0.0f,std::min(1.0f,((px-ax)*x+(py-ay)*y)/q)):0,dx=px-(ax+x*t),dy=py-(ay+y*t);return std::sqrt(dx*dx+dy*dy);}
float curvedArrow(float px,float py){constexpr int N=28;float cx=27.5f,cy=25,rx=19,ry=12,start=PI*.89f,end=PI*.10f,m=1000,px0=cx+rx*std::cos(start),py0=cy-ry*std::sin(start);for(int i=1;i<=N;++i){float a=start+(end-start)*float(i)/N,x=cx+rx*std::cos(a),y=cy-ry*std::sin(a);m=std::min(m,segDist(px,py,px0,py0,x,y));px0=x;py0=y;}float tx=std::sin(end)*rx,ty=std::cos(end)*ry,l=std::sqrt(tx*tx+ty*ty),dx=tx/l,dy=ty/l,nx=-dy,ny=dx,bx=px0-dx*9,by=py0-dy*9;m=std::min(m,segDist(px,py,px0,py0,bx+nx*5.5f,by+ny*5.5f));return std::min(m,segDist(px,py,px0,py0,bx-nx*5.5f,by-ny*5.5f));}
float arrow(float px,float py,float dx,float dy){float c=PAN_ARROW_SIZE*.5f,nx=-dy,ny=dx,tx=c-dx*10,ty=c-dy*10,bx=c+dx*5,by=c+dy*5,tipx=c+dx*13,tipy=c+dy*13,m=segDist(px,py,tx,ty,bx,by);m=std::min(m,segDist(px,py,tipx,tipy,bx+nx*6,by+ny*6));return std::min(m,segDist(px,py,tipx,tipy,bx-nx*6,by-ny*6));}
float zoomGlyph(float px,float py,bool plus){float c=ZOOM_CONTROL_SIZE*.5f,a=8.5f,m=segDist(px,py,c-a,c,c+a,c);return plus?std::min(m,segDist(px,py,c,c-a,c,c+a)):m;}

dsr::OrderedImageRgbaU8 frame,cwSprite,ccwSprite,upSprite,downSprite,leftSprite,rightSprite,upDisabled,downDisabled,leftDisabled,rightDisabled,resetSprite,resetDisabled,plusSprite,minusSprite;
std::vector<std::uint8_t> rgba;
void spritePixel(dsr::OrderedImageRgbaU8&s,int x,int y,float d,float strength=1){float outer=std::max(0.0f,std::min(1.0f,3.6f-d));if(outer<=0)return;float core=std::max(0.0f,std::min(1.0f,2.25f-d));int alpha=int(outer*235*strength+.5f),shade=int(64+core*186*strength+.5f);dsr::image_writePixel(s,x,y,dsr::ColorRgbaI32(shade,shade,shade,alpha));}
void buildRotate(dsr::OrderedImageRgbaU8&s,bool mirror){s=dsr::image_create_RgbaU8(ROTATE_ARROW_WIDTH,ROTATE_ARROW_HEIGHT,true);dsr::image_fill(s,{0,0,0,0});for(int y=0;y<ROTATE_ARROW_HEIGHT;++y)for(int x=0;x<ROTATE_ARROW_WIDTH;++x){float sx=mirror?ROTATE_ARROW_WIDTH-1-x+.5f:x+.5f;spritePixel(s,x,y,curvedArrow(sx,y+.5f));}}
void buildPan(dsr::OrderedImageRgbaU8&s,float dx,float dy,bool disabled=false){s=dsr::image_create_RgbaU8(PAN_ARROW_SIZE,PAN_ARROW_SIZE,true);dsr::image_fill(s,{0,0,0,0});for(int y=0;y<PAN_ARROW_SIZE;++y)for(int x=0;x<PAN_ARROW_SIZE;++x)spritePixel(s,x,y,arrow(x+.5f,y+.5f,dx,dy),disabled?.28f:1);}
void buildReset(dsr::OrderedImageRgbaU8&s,bool disabled=false){s=dsr::image_create_RgbaU8(RESET_DISK_SIZE,RESET_DISK_SIZE,true);dsr::image_fill(s,{0,0,0,0});float c=RESET_DISK_SIZE*.5f,k=disabled?.28f:1;for(int y=0;y<RESET_DISK_SIZE;++y)for(int x=0;x<RESET_DISK_SIZE;++x){float dx=x+.5f-c,dy=y+.5f-c,r=std::sqrt(dx*dx+dy*dy);if(r<=12.5f){float e=std::max(0.0f,std::min(1.0f,13.5f-r));int q=int((r<9?198:232)*k),a=int(e*225*k+.5f);dsr::image_writePixel(s,x,y,{q,q,q,a});}}}
void buildZoom(dsr::OrderedImageRgbaU8&s,bool plus){s=dsr::image_create_RgbaU8(ZOOM_CONTROL_SIZE,ZOOM_CONTROL_SIZE,true);dsr::image_fill(s,{0,0,0,0});for(int y=0;y<ZOOM_CONTROL_SIZE;++y)for(int x=0;x<ZOOM_CONTROL_SIZE;++x)spritePixel(s,x,y,zoomGlyph(x+.5f,y+.5f,plus));}
void ensure(){if(!dsr::image_exists(frame)||allocatedFrameWidth!=frameWidth||allocatedFrameHeight!=frameHeight){frame=dsr::image_create_RgbaU8(frameWidth,frameHeight,false);allocatedFrameWidth=frameWidth;allocatedFrameHeight=frameHeight;}if(!dsr::image_exists(cwSprite)){buildRotate(cwSprite,false);buildRotate(ccwSprite,true);}if(!dsr::image_exists(upSprite)){buildPan(upSprite,0,-1);buildPan(downSprite,0,1);buildPan(leftSprite,-1,0);buildPan(rightSprite,1,0);buildPan(upDisabled,0,-1,true);buildPan(downDisabled,0,1,true);buildPan(leftDisabled,-1,0,true);buildPan(rightDisabled,1,0,true);}if(!dsr::image_exists(resetSprite)){buildReset(resetSprite);buildReset(resetDisabled,true);}if(!dsr::image_exists(plusSprite)){buildZoom(plusSprite,true);buildZoom(minusSprite,false);}std::size_t n=std::size_t(frameWidth)*frameHeight*4;if(rgba.size()!=n)rgba.resize(n);}
void controls(){int zx=ZOOM_LEFT+(RESET_DISK_SIZE-ZOOM_CONTROL_SIZE)/2,zr=ZOOM_LEFT,zin=ZOOM_TOP,zreset=zin+ZOOM_CONTROL_SIZE+ZOOM_GAP,zout=zreset+RESET_DISK_SIZE+ZOOM_GAP;dsr::draw_alphaFilter(frame,plusSprite,zx,zin);dsr::draw_alphaFilter(frame,resetSprite,zr,zreset);dsr::draw_alphaFilter(frame,minusSprite,zx,zout);int yawTop=frameHeight-CONTROL_BOTTOM-ROTATE_ARROW_HEIGHT,ccx=ROTATE_LEFT_X,ry=ccx+ROTATE_ARROW_WIDTH+ROTATE_ROW_GAP,cwx=ry+RESET_DISK_SIZE+ROTATE_ROW_GAP,rt=yawTop+(ROTATE_ARROW_HEIGHT-RESET_DISK_SIZE)/2;dsr::draw_alphaFilter(frame,ccwSprite,ccx,yawTop);dsr::draw_alphaFilter(frame,resetSprite,ry,rt);dsr::draw_alphaFilter(frame,cwSprite,cwx,yawTop);int cx=frameWidth-PAN_PAD_RIGHT-PAN_ARROW_SIZE-PAN_X_STEP,cy=frameHeight-PAN_PAD_BOTTOM-PAN_ARROW_SIZE-PAN_Y_STEP;bool on=canPan();auto&L=on?leftSprite:leftDisabled;auto&R=on?rightSprite:rightDisabled;auto&U=on?upSprite:upDisabled;auto&D=on?downSprite:downDisabled;auto&C=on?resetSprite:resetDisabled;dsr::draw_alphaFilter(frame,L,cx-PAN_X_STEP,cy);dsr::draw_alphaFilter(frame,C,cx,cy);dsr::draw_alphaFilter(frame,R,cx+PAN_X_STEP,cy);dsr::draw_alphaFilter(frame,U,cx,cy-PAN_Y_STEP);dsr::draw_alphaFilter(frame,D,cx,cy+PAN_Y_STEP);}
void render(){ensure();const float o[2]={.25f,.75f};for(int y=0;y<frameHeight;++y)for(int x=0;x<frameWidth;++x){Vec3 c;for(int sy=0;sy<2;++sy)for(int sx=0;sx<2;++sx)c=c+trace(x+o[sx],y+o[sy]);c=c*.25f;dsr::image_writePixel(frame,x,y,{byte(c.x),byte(c.y),byte(c.z),255});}controls();for(int y=0;y<frameHeight;++y)for(int x=0;x<frameWidth;++x){auto c=dsr::image_readPixel_border(frame,x,y);std::size_t i=std::size_t((y*frameWidth+x)*4);rgba[i]=c.red;rgba[i+1]=c.green;rgba[i+2]=c.blue;rgba[i+3]=255;}}
void present(){
  std::uintptr_t p=reinterpret_cast<std::uintptr_t>(rgba.data());
  bool panOn=canPan(); float vh=viewHeight(), wholeScale=wholeZoomScale();
  EM_ASM({
    const p=$0;
    const w=$1;
    const h=$2;
    const yaw=$3;
    const panX=$4;
    const panY=$5;
    const z=$6;
    const detailed=!!$7;
    const canPan=!!$8;
    const viewH=$9;
    const wholeScale=$10;
    const canvas=document.getElementById('canvas');
    if(!canvas)return;
    if(canvas.width!==w)canvas.width=w;
    if(canvas.height!==h)canvas.height=h;
    const ctx=canvas.getContext('2d',{alpha:false});
    const img=ctx.createImageData(w,h);
    img.data.set(HEAPU8.subarray(p,p+w*h*4));
    ctx.putImageData(img,0,0);
    window.isowebViewHeightWorld=viewH;
    window.isowebCameraCanPan=canPan;
    window.isowebWholeZoomScale=wholeScale;
    const u=document.getElementById('pan-up');
    const dn=document.getElementById('pan-down');
    const l=document.getElementById('pan-left');
    const r=document.getElementById('pan-right');
    const c=document.getElementById('reset-camera');
    if(u)u.disabled=!canPan;
    if(dn)dn.disabled=!canPan;
    if(l)l.disabled=!canPan;
    if(r)r.disabled=!canPan;
    if(c)c.disabled=!canPan;
    document.documentElement.classList.add('wasm-ready');
    const loading=document.getElementById('loading');
    if(loading)loading.hidden=true;
    let label='1x';
    if(z===0)label='whole';
    else if(z===1)label='0.25x';
    else if(z===2)label='0.5x';
    else if(z===4)label='2x';
    else if(z===5)label='4x';
    const status=document.getElementById('view-status');
    if(status)status.textContent='Camera '+yaw+' degrees around Z; pan X '+panX.toFixed(2)+'; Y '+panY.toFixed(2)+'; zoom '+label+(detailed?' detailed':' regular')+(canPan?'':'; panning disabled');
  },int(p),frameWidth,frameHeight,cameraYawStep*45,cameraPanX,cameraPanY,zoomPreset,detailedZoomMode?1:0,panOn?1:0,vh,wholeScale);
}
void redraw(){render();present();}
}

extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_render(){redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_resize(int w,int h){frameWidth=std::max(160,std::min(1600,w));frameHeight=std::max(160,std::min(1600,h));redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_clockwise(){cameraYawStep=(cameraYawStep+1)&7;redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_rotate_counterclockwise(){cameraYawStep=(cameraYawStep+7)&7;redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_yaw(){cameraYawStep=0;redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_in(){stepZoom(1);redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_zoom_out(){stepZoom(-1);redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_zoom(){zoomPreset=3;redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_set_detailed_mode(int enabled){detailedZoomMode=enabled!=0;if(!detailedZoomMode){if(zoomPreset<2)zoomPreset=2;if(zoomPreset>4)zoomPreset=4;}redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_pan(float right,float down){pan(right,down);redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_camera(){cameraPanX=0;cameraPanY=0;redraw();}