#!/usr/bin/env bash
set -euo pipefail

mkdir -p site/assets

mapfile -t ISOWEB_SOURCES < <(find src -type f -name '*.cpp' -print | sort)
mapfile -t DFPSR_SOURCES < <(find vendor/dfpsr/Source/DFPSR -type f -name '*.cpp' -print | sort)

em++ \
  "${ISOWEB_SOURCES[@]}" \
  "${DFPSR_SOURCES[@]}" \
  vendor/dfpsr/Source/windowManagers/NoWindow.cpp \
  vendor/dfpsr/Source/soundManagers/NoSound.cpp \
  -std=c++14 \
  -O3 \
  -DDISABLE_MULTI_THREADING \
  -D__unix__ \
  -Isrc \
  -Ivendor/dfpsr/Source \
  --no-entry \
  -sALLOW_MEMORY_GROWTH=1 \
  -sENVIRONMENT=web \
  -sEXPORTED_FUNCTIONS='["_isoweb_render","_isoweb_resize","_isoweb_rotate_clockwise","_isoweb_rotate_counterclockwise","_isoweb_reset_yaw","_isoweb_zoom_in","_isoweb_zoom_out","_isoweb_reset_zoom","_isoweb_set_detailed_mode","_isoweb_pan","_isoweb_reset_camera","_isoweb_level_up","_isoweb_level_down","_isoweb_reset_level","_isoweb_level_count","_isoweb_active_level_index","_isoweb_default_level_index"]' \
  -o site/assets/isoweb.js
