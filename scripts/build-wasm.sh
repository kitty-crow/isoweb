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
  -sEXPORTED_RUNTIME_METHODS='["ccall"]' \
  -sEXPORTED_FUNCTIONS='["_malloc","_free","_isoweb_render","_isoweb_tick","_isoweb_needs_tick","_isoweb_resize","_isoweb_rotate_clockwise","_isoweb_rotate_counterclockwise","_isoweb_reset_yaw","_isoweb_set_detailed_yaw_mode","_isoweb_zoom_in","_isoweb_zoom_out","_isoweb_reset_zoom","_isoweb_set_detailed_mode","_isoweb_pan","_isoweb_reset_camera","_isoweb_set_control_stick","_isoweb_level_up","_isoweb_level_down","_isoweb_reset_level","_isoweb_level_count","_isoweb_active_level_index","_isoweb_default_level_index","_isoweb_static_cache_build_count","_isoweb_pointer_tap","_isoweb_drag_select","_isoweb_clear_selection","_isoweb_clear_entities","_isoweb_character_count","_isoweb_selected_character_count","_isoweb_character_position_x","_isoweb_character_position_y","_isoweb_character_position_z","_isoweb_character_is_moving","_isoweb_create_character","_isoweb_set_character_location","_isoweb_set_character_forward","_isoweb_set_character_hitbox","_isoweb_set_character_flags","_isoweb_set_character_speed","_isoweb_add_character_collision_tag","_isoweb_add_character_must_collide_with","_isoweb_clear_character_collision_filters","_isoweb_set_character_sprite","_isoweb_set_character_action","_isoweb_register_sprite_atlas","_isoweb_set_base_movement_speed","_isoweb_set_selection_mode","_isoweb_set_selection_style"]' \
  -o site/assets/isoweb.js
