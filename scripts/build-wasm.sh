#!/usr/bin/env bash
set -euo pipefail

mkdir -p site/assets

mapfile -t DFPSR_SOURCES < <(find vendor/dfpsr/Source/DFPSR -type f -name '*.cpp' -print | sort)

em++ \
  src/main.cpp \
  "${DFPSR_SOURCES[@]}" \
  vendor/dfpsr/Source/windowManagers/NoWindow.cpp \
  vendor/dfpsr/Source/soundManagers/NoSound.cpp \
  -std=c++14 \
  -O3 \
  -DDISABLE_MULTI_THREADING \
  -D__unix__ \
  -Ivendor/dfpsr/Source \
  --no-entry \
  -sALLOW_MEMORY_GROWTH=1 \
  -sENVIRONMENT=web \
  -sEXPORTED_FUNCTIONS='["_isoweb_render","_isoweb_rotate_clockwise","_isoweb_rotate_counterclockwise","_isoweb_pan"]' \
  -o site/assets/isoweb.js
