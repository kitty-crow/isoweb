float curvedArrow(float px,float py){constexpr int N=28;float cx=27.5f,cy=25,rx=19,ry=12,start=PI*.89f,end=PI*.10f,m=1000,px0=cx+rx*std::cos(start),py0=cy-ry*std::sin(start);for(int i=1;i<=N;++i){float a=start+(end-start)*float(i)/N,x=cx+rx*std::cos(a),y=cy-ry*std::sin(a);m=std::min(m,segDist(px,py,px0,py0,x,y));px0=x;py0=y;}float tx=std::sin(end)*rx,ty=std::cos(end)*ry,l=std::sqrt(tx*tx+ty*ty),dx=tx/l,dy=ty/l,nx=-dy,ny=dx,bx=px0-dx*9,by=py0-dy*9;m=std::min(m,segDist(px,py,px0,py0,bx+nx*5.5f,by+ny*5.5f));return std::min(m,segDist(px,py,px0,py0,bx-nx*5.5f,by-ny*5.5f));}
float arrow(float px,float py,float dx,float dy){float c=PAN_ARROW_SIZE*.5f,nx=-dy,ny=dx,tx=c-dx*10,ty=c-dy*10,bx=c+dx*5,by=c+dy*5,tipx=c+dx*13,tipy=c+dy*13,m=segDist(px,py,tx,ty,bx,by);m=std::min(m,segDist(px,py,tipx,tipy,bx+nx*6,by+ny*6));return std::min(m,segDist(px,py,tipx,tipy,bx-nx*6,by-ny*6));}
float zoomGlyph(float px,float py,bool plus){float c=ZOOM_CONTROL_SIZE*.5f,a=8.5f,m=segDist(px,py,c-a,c,c+a,c);return plus?std::min(m,segDist(px,py,c,c-a,c,c+a)):m;}
float zGlyph(float px,float py){float c=RESET_DISK_SIZE*.5f,a=5.5f,m=segDist(px,py,c-a,c-a,c+a,c-a);m=std::min(m,segDist(px,py,c+a,c-a,c-a,c+a));return std::min(m,segDist(px,py,c-a,c+a,c+a,c+a));}

dsr::OrderedImageRgbaU8 frame,cwSprite,ccwSprite,upSprite,downSprite,leftSprite,rightSprite,upDisabled,downDisabled,leftDisabled,rightDisabled,resetSprite,resetDisabled,plusSprite,minusSprite,zResetSprite,zResetDisabled;
std::vector<std::uint8_t> rgba;
void spritePixel(dsr::OrderedImageRgbaU8&s,int x,int y,float d,float strength=1){float outer=std::max(0.0f,std::min(1.0f,3.6f-d));if(outer<=0)return;float core=std::max(0.0f,std::min(1.0f,2.25f-d));int alpha=int(outer*235*strength+.5f),shade=int((64+core*186)*strength+.5f);dsr::image_writePixel(s,x,y,{shade,shade,shade,alpha});}
void buildRotate(dsr::OrderedImageRgbaU8&s,bool mirror){s=dsr::image_create_RgbaU8(ROTATE_ARROW_WIDTH,ROTATE_ARROW_HEIGHT,true);dsr::image_fill(s,{0,0,0,0});for(int y=0;y<ROTATE_ARROW_HEIGHT;++y)for(int x=0;x<ROTATE_ARROW_WIDTH;++x){float sx=mirror?ROTATE_ARROW_WIDTH-1-x+.5f:x+.5f;spritePixel(s,x,y,curvedArrow(sx,y+.5f));}}
void buildPan(dsr::OrderedImageRgbaU8&s,float dx,float dy,bool disabled=false){s=dsr::image_create_RgbaU8(PAN_ARROW_SIZE,PAN_ARROW_SIZE,true);dsr::image_fill(s,{0,0,0,0});for(int y=0;y<PAN_ARROW_SIZE;++y)for(int x=0;x<PAN_ARROW_SIZE;++x)spritePixel(s,x,y,arrow(x+.5f,y+.5f,dx,dy),disabled?.28f:1);}
void buildReset(dsr::OrderedImageRgbaU8&s,bool disabled=false){s=dsr::image_create_RgbaU8(RESET_DISK_SIZE,RESET_DISK_SIZE,true);dsr::image_fill(s,{0,0,0,0});float c=RESET_DISK_SIZE*.5f,k=disabled?.28f:1;for(int y=0;y<RESET_DISK_SIZE;++y)for(int x=0;x<RESET_DISK_SIZE;++x){float dx=x+.5f-c,dy=y+.5f-c,r=std::sqrt(dx*dx+dy*dy);if(r<=12.5f){float e=std::max(0.0f,std::min(1.0f,13.5f-r));int q=int((r<9?198:232)*k),a=int(e*225*k+.5f);dsr::image_writePixel(s,x,y,{q,q,q,a});}}}
void buildZReset(dsr::OrderedImageRgbaU8&s,bool disabled=false){buildReset(s,disabled);float k=disabled?.28f:1;for(int y=0;y<RESET_DISK_SIZE;++y)for(int x=0;x<RESET_DISK_SIZE;++x)if(zGlyph(x+.5f,y+.5f)<1.25f){int q=int(28*k+.5f),a=int(230*k+.5f);dsr::image_writePixel(s,x,y,{q,q,q,a});}}
void buildZoom(dsr::OrderedImageRgbaU8&s,bool plus){s=dsr::image_create_RgbaU8(ZOOM_CONTROL_SIZE,ZOOM_CONTROL_SIZE,true);dsr::image_fill(s,{0,0,0,0});for(int y=0;y<ZOOM_CONTROL_SIZE;++y)for(int x=0;x<ZOOM_CONTROL_SIZE;++x)spritePixel(s,x,y,zoomGlyph(x+.5f,y+.5f,plus));}
void ensure(){if(!dsr::image_exists(frame)||allocatedFrameWidth!=frameWidth||allocatedFrameHeight!=frameHeight){frame=dsr::image_create_RgbaU8(frameWidth,frameHeight,false);allocatedFrameWidth=frameWidth;allocatedFrameHeight=frameHeight;}if(!dsr::image_exists(cwSprite)){buildRotate(cwSprite,false);buildRotate(ccwSprite,true);}if(!dsr::image_exists(upSprite)){buildPan(upSprite,0,-1);buildPan(downSprite,0,1);buildPan(leftSprite,-1,0);buildPan(rightSprite,1,0);buildPan(upDisabled,0,-1,true);buildPan(downDisabled,0,1,true);buildPan(leftDisabled,-1,0,true);buildPan(rightDisabled,1,0,true);}if(!dsr::image_exists(resetSprite)){buildReset(resetSprite);buildReset(resetDisabled,true);buildZReset(zResetSprite);buildZReset(zResetDisabled,true);}if(!dsr::image_exists(plusSprite)){buildZoom(plusSprite,true);buildZoom(minusSprite,false);}std::size_t n=std::size_t(frameWidth)*frameHeight*4;if(rgba.size()!=n)rgba.resize(n);}
void controls(){
  int zx=ZOOM_LEFT+(RESET_DISK_SIZE-ZOOM_CONTROL_SIZE)/2,zr=ZOOM_LEFT,zin=ZOOM_TOP,zreset=zin+ZOOM_CONTROL_SIZE+ZOOM_GAP,zout=zreset+RESET_DISK_SIZE+ZOOM_GAP;dsr::draw_alphaFilter(frame,plusSprite,zx,zin);dsr::draw_alphaFilter(frame,resetSprite,zr,zreset);dsr::draw_alphaFilter(frame,minusSprite,zx,zout);
  int lx=frameWidth-LEVEL_RIGHT-PAN_ARROW_SIZE,lup=LEVEL_TOP,lr=lup+PAN_ARROW_SIZE+LEVEL_GAP,ldown=lr+RESET_DISK_SIZE+LEVEL_GAP;bool canUp=selectedLevel+1<int(levels.size()),canDown=selectedLevel>0,canReset=selectedLevel!=DEFAULT_LEVEL_INDEX&&DEFAULT_LEVEL_INDEX<int(levels.size());dsr::draw_alphaFilter(frame,canUp?upSprite:upDisabled,lx,lup);dsr::draw_alphaFilter(frame,canReset?zResetSprite:zResetDisabled,lx,lr);dsr::draw_alphaFilter(frame,canDown?downSprite:downDisabled,lx,ldown);
  int yawTop=frameHeight-CONTROL_BOTTOM-ROTATE_ARROW_HEIGHT,ccx=ROTATE_LEFT_X,ry=ccx+ROTATE_ARROW_WIDTH+ROTATE_ROW_GAP,cwx=ry+RESET_DISK_SIZE+ROTATE_ROW_GAP,rt=yawTop+(ROTATE_ARROW_HEIGHT-RESET_DISK_SIZE)/2;dsr::draw_alphaFilter(frame,ccwSprite,ccx,yawTop);dsr::draw_alphaFilter(frame,resetSprite,ry,rt);dsr::draw_alphaFilter(frame,cwSprite,cwx,yawTop);
  int cx=frameWidth-PAN_PAD_RIGHT-PAN_ARROW_SIZE-PAN_X_STEP,cy=frameHeight-PAN_PAD_BOTTOM-PAN_ARROW_SIZE-PAN_Y_STEP;bool on=canPan();auto&L=on?leftSprite:leftDisabled;auto&R=on?rightSprite:rightDisabled;auto&U=on?upSprite:upDisabled;auto&D=on?downSprite:downDisabled;auto&C=on?resetSprite:resetDisabled;dsr::draw_alphaFilter(frame,L,cx-PAN_X_STEP,cy);dsr::draw_alphaFilter(frame,C,cx,cy);dsr::draw_alphaFilter(frame,R,cx+PAN_X_STEP,cy);dsr::draw_alphaFilter(frame,U,cx,cy-PAN_Y_STEP);dsr::draw_alphaFilter(frame,D,cx,cy+PAN_Y_STEP);
}
void render(){ensure();const float o[2]={.25f,.75f};for(int y=0;y<frameHeight;++y)for(int x=0;x<frameWidth;++x){Vec3 c;for(int sy=0;sy<2;++sy)for(int sx=0;sx<2;++sx)c=c+trace(x+o[sx],y+o[sy]);c=c*.25f;dsr::image_writePixel(frame,x,y,{byte(c.x),byte(c.y),byte(c.z),255});}controls();for(int y=0;y<frameHeight;++y)for(int x=0;x<frameWidth;++x){auto c=dsr::image_readPixel_border(frame,x,y);std::size_t i=std::size_t((y*frameWidth+x)*4);rgba[i]=c.red;rgba[i+1]=c.green;rgba[i+2]=c.blue;rgba[i+3]=255;}}
void present(){std::uintptr_t p=reinterpret_cast<std::uintptr_t>(rgba.data());bool panOn=canPan(),levelUp=selectedLevel+1<int(levels.size()),levelDown=selectedLevel>0,levelReset=selectedLevel!=DEFAULT_LEVEL_INDEX&&DEFAULT_LEVEL_INDEX<int(levels.size());float vh=viewHeight(),wholeScale=wholeZoomScale();EM_ASM({
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
  const level=$11;
  const levelCount=$12;
  const levelUp=!!$13;
  const levelDown=!!$14;
  const levelReset=!!$15;
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
  window.isowebSelectedLevel=level;
  window.isowebLevelCount=levelCount;
  const set=(id,on)=>{const el=document.getElementById(id);if(el)el.disabled=!on;};
  set('zoom-in',!!$16);
  set('zoom-out',!!$17);
  set('reset-zoom',z!==3);
  set('rotate-counterclockwise',true);
  set('rotate-clockwise',true);
  set('reset-yaw',yaw!==0);
  set('pan-up',canPan);
  set('pan-down',canPan);
  set('pan-left',canPan);
  set('pan-right',canPan);
  set('reset-camera',canPan);
  set('level-up',levelUp);
  set('level-down',levelDown);
  set('reset-level',levelReset);
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
  if(status)status.textContent='Camera '+yaw+' degrees around Z; pan X '+panX.toFixed(2)+'; Y '+panY.toFixed(2)+'; zoom '+label+(detailed?' detailed':' regular')+'; Z level '+(level+1)+' of '+levelCount+(canPan?String():'; panning disabled');
},int(p),frameWidth,frameHeight,cameraYawStep*45,cameraPanX,cameraPanY,zoomPreset,detailedZoomMode?1:0,panOn?1:0,vh,wholeScale,selectedLevel,int(levels.size()),levelUp?1:0,levelDown?1:0,levelReset?1:0,canZoomIn()?1:0,canZoomOut()?1:0);}
void redraw(){render();present();}
