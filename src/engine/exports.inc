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
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_level_up(){if(selectedLevel+1<int(levels.size()))++selectedLevel;redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_level_down(){if(selectedLevel>0)--selectedLevel;redraw();}
extern "C" EMSCRIPTEN_KEEPALIVE void isoweb_reset_level(){if(DEFAULT_LEVEL_INDEX<int(levels.size()))selectedLevel=DEFAULT_LEVEL_INDEX;redraw();}
