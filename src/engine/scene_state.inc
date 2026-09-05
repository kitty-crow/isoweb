int frameWidth=512, frameHeight=288, allocatedFrameWidth=0, allocatedFrameHeight=0;
constexpr int ROTATE_ARROW_WIDTH=56, ROTATE_ARROW_HEIGHT=44, RESET_DISK_SIZE=38;
constexpr int ROTATE_LEFT_X=18, ROTATE_ROW_GAP=8, ZOOM_CONTROL_SIZE=32;
constexpr int ZOOM_LEFT=18, ZOOM_TOP=18, ZOOM_GAP=6, CONTROL_BOTTOM=18;
constexpr int PAN_ARROW_SIZE=38, PAN_X_STEP=48, PAN_Y_STEP=36, PAN_PAD_RIGHT=18, PAN_PAD_BOTTOM=16;
constexpr int LEVEL_RIGHT=18, LEVEL_TOP=18, LEVEL_GAP=6;
constexpr float EPSILON=0.0015f, FAR_DISTANCE=1000.0f, PI=3.14159265358979323846f;
constexpr float GROUND_LIMIT=4.40f, PAN_LIMIT=GROUND_LIMIT-1.15f, BASE_VIEW_HEIGHT=6.15f, MIN_VIEW_WIDTH=5.50f;
constexpr int DEFAULT_LEVEL_INDEX=1;

struct Vec3 { float x=0,y=0,z=0; Vec3()=default; Vec3(float x,float y,float z):x(x),y(y),z(z){} Vec3 operator+(const Vec3&o)const{return{x+o.x,y+o.y,z+o.z};} Vec3 operator-(const Vec3&o)const{return{x-o.x,y-o.y,z-o.z};} Vec3 operator*(float s)const{return{x*s,y*s,z*s};} Vec3 operator/(float s)const{return{x/s,y/s,z/s};} };
float dot(const Vec3&a,const Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(const Vec3&a,const Vec3&b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float length(const Vec3&v){return std::sqrt(dot(v,v));}
Vec3 normalise(const Vec3&v){float m=length(v);return m>0?v/m:Vec3();}
Vec3 rotateZ(const Vec3&v,float r){float c=std::cos(r),s=std::sin(r);return{v.x*c-v.y*s,v.x*s+v.y*c,v.z};}
struct Ray{Vec3 origin,direction;};

enum class ShapeKind{Cube,Sphere,Cone,Pyramid,Dodecahedron,Icosahedron};
struct RenderObject{ShapeKind kind;Vec3 position;float size;float height;Vec3 colour;};
struct LevelDefinition{std::vector<RenderObject> objects;Vec3 lightPosition;Vec3 floorDark;Vec3 floorLight;};
struct Hit{bool found=false;float t=FAR_DISTANCE;Vec3 point,normal,colour;bool ground=false;};

std::vector<LevelDefinition> makeDemoLevels(){
  std::vector<LevelDefinition> out;
  LevelDefinition lower;
  lower.lightPosition={4.20f,-3.20f,5.60f}; lower.floorDark={.34f,.34f,.36f}; lower.floorLight={.40f,.40f,.42f};
  lower.objects.push_back({ShapeKind::Cone,{-1.30f,-0.80f,.82f},.86f,1.64f,{.62f,.25f,.82f}});
  lower.objects.push_back({ShapeKind::Pyramid,{1.20f,.85f,.83f},.92f,1.66f,{.96f,.78f,.16f}}); out.push_back(lower);
  LevelDefinition middle;
  middle.lightPosition={-3.60f,-4.20f,6.50f}; middle.floorDark={.567f,.6048f,.630f}; middle.floorLight={.621f,.6624f,.690f};
  middle.objects.push_back({ShapeKind::Cube,{-1.05f,.65f,.775f},.80f,1.55f,{.18f,.48f,.88f}});
  middle.objects.push_back({ShapeKind::Sphere,{1.05f,-.25f,.90f},.90f,1.80f,{.95f,.43f,.12f}}); out.push_back(middle);
  LevelDefinition upper;
  upper.lightPosition={3.80f,4.40f,7.20f}; upper.floorDark={.74f,.74f,.76f}; upper.floorLight={.82f,.82f,.84f};
  upper.objects.push_back({ShapeKind::Dodecahedron,{-1.35f,.95f,1.02f},.72f,0,{.18f,.50f,.94f}});
  upper.objects.push_back({ShapeKind::Icosahedron,{1.30f,-.95f,1.08f},.78f,0,{.90f,.16f,.14f}}); out.push_back(upper);
  return out;
}
std::vector<LevelDefinition> levels=makeDemoLevels();
const Vec3 BASE_FOCUS(0,.15f,.55f);
int selectedLevel=DEFAULT_LEVEL_INDEX, cameraYawStep=0, zoomPreset=3;
float cameraPanX=0,cameraPanY=0; bool detailedZoomMode=false;
const LevelDefinition& activeLevel(){return levels[static_cast<std::size_t>(selectedLevel)];}

float yawRadians(){return -cameraYawStep*PI*.25f;}
Vec3 cameraForward(){return rotateZ(normalise({1,1,-1}),yawRadians());}
Vec3 cameraGroundRight(){return normalise(cross(cameraForward(),{0,0,1}));}
Vec3 cameraGroundDown(){Vec3 f=cameraForward();return normalise({f.x,f.y,0});}
bool wholeZoom(){return zoomPreset==0;}
float zoomScale(){switch(zoomPreset){case 5:return 4;case 4:return 2;case 3:return 1;case 2:return .5f;case 1:return .25f;default:return .25f;}}
Vec3 objectExtent(const RenderObject&o){
  if(o.kind==ShapeKind::Cube)return{o.size,o.size,o.height*.5f};
  if(o.kind==ShapeKind::Sphere)return{o.size,o.size,o.size};
  if(o.kind==ShapeKind::Cone||o.kind==ShapeKind::Pyramid)return{o.size,o.size,o.height*.5f};
  float r=o.size*1.55f;return{r,r,r};
}
float wholeViewHeight(float aspect){
  Vec3 f=cameraForward(),r=normalise(cross(f,{0,0,1})),u=normalise(cross(r,f));float minR=1e9f,maxR=-1e9f,minU=1e9f,maxU=-1e9f;
  auto add=[&](Vec3 p){Vec3 q=p-BASE_FOCUS;float pr=dot(q,r),pu=dot(q,u);minR=std::min(minR,pr);maxR=std::max(maxR,pr);minU=std::min(minU,pu);maxU=std::max(maxU,pu);};
  add({-GROUND_LIMIT,-GROUND_LIMIT,0});add({GROUND_LIMIT,-GROUND_LIMIT,0});add({-GROUND_LIMIT,GROUND_LIMIT,0});add({GROUND_LIMIT,GROUND_LIMIT,0});
  for(const auto&o:activeLevel().objects){Vec3 e=objectExtent(o);for(int sx=-1;sx<=1;sx+=2)for(int sy=-1;sy<=1;sy+=2)for(int sz=-1;sz<=1;sz+=2)add(o.position+Vec3(e.x*sx,e.y*sy,e.z*sz));}
  float w=(maxR-minR)*1.04f,h=(maxU-minU)*1.04f;return std::max(h,w/aspect);
}
float baseViewHeight(float aspect){return std::max(BASE_VIEW_HEIGHT,MIN_VIEW_WIDTH/aspect);}
float wholeZoomScale(){float a=float(frameWidth)/frameHeight;return baseViewHeight(a)/wholeViewHeight(a);}
float viewHeight(){float a=float(frameWidth)/frameHeight;return wholeZoom()?wholeViewHeight(a):baseViewHeight(a)/zoomScale();}
bool canPan(){float a=float(frameWidth)/frameHeight;return viewHeight()+.0001f<wholeViewHeight(a);}
int detailedPresetAt(int p){if(wholeZoomScale()>.25f){static const int o[6]={1,0,2,3,4,5};return o[p];}return p;}
int presetAt(int p){return detailedZoomMode?detailedPresetAt(p):p+2;}
int sequenceLength(){return detailedZoomMode?6:3;}
int sequencePosition(){for(int p=0;p<sequenceLength();++p)if(presetAt(p)==zoomPreset)return p;return detailedZoomMode?3:1;}
bool canZoomIn(){return sequencePosition()<sequenceLength()-1;}
bool canZoomOut(){return sequencePosition()>0;}
void stepZoom(int d){int p=std::max(0,std::min(sequenceLength()-1,sequencePosition()+d));zoomPreset=presetAt(p);}
void pan(float right,float down){if(!canPan())return;Vec3 d=cameraGroundRight()*right+cameraGroundDown()*down;cameraPanX=std::max(-PAN_LIMIT,std::min(PAN_LIMIT,cameraPanX+d.x));cameraPanY=std::max(-PAN_LIMIT,std::min(PAN_LIMIT,cameraPanY+d.y));}
