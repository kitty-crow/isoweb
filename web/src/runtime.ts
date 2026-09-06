export interface IsowebModule {
  HEAPU8: Uint8Array;
  _malloc(size: number): number;
  _free(pointer: number): void;

  _isoweb_render(): void;
  _isoweb_tick(deltaSeconds: number): void;
  _isoweb_needs_tick(): number;
  _isoweb_resize(width: number, height: number): void;
  _isoweb_rotate_clockwise(): void;
  _isoweb_rotate_counterclockwise(): void;
  _isoweb_reset_yaw(): void;
  _isoweb_set_detailed_yaw_mode(enabled: number): void;
  _isoweb_zoom_in(): void;
  _isoweb_zoom_out(): void;
  _isoweb_reset_zoom(): void;
  _isoweb_set_detailed_mode(enabled: number): void;
  _isoweb_pan(right: number, down: number): void;
  _isoweb_reset_camera(): void;
  _isoweb_level_up(): void;
  _isoweb_level_down(): void;
  _isoweb_reset_level(): void;
  _isoweb_level_count(): number;
  _isoweb_active_level_index(): number;
  _isoweb_default_level_index(): number;

  _isoweb_pointer_tap(x: number, y: number, additive: number): number;
  _isoweb_clear_selection(): void;
  _isoweb_clear_entities(): void;
  _isoweb_create_character(id: number, world: number, timeline: number, level: number, x: number, y: number, z: number): number;
  _isoweb_set_character_location(id: number, world: number, timeline: number, level: number, x: number, y: number, z: number): number;
  _isoweb_set_character_forward(id: number, x: number, y: number): number;
  _isoweb_set_character_hitbox(id: number, minX: number, minY: number, minZ: number, maxX: number, maxY: number, maxZ: number): number;
  _isoweb_set_character_flags(id: number, solid: number, npc: number, controllable: number): number;
  _isoweb_set_character_speed(id: number, multiplier: number): number;
  _isoweb_add_character_collision_tag(id: number, tag: number): number;
  _isoweb_add_character_must_collide_with(id: number, selector: number): number;
  _isoweb_clear_character_collision_filters(id: number): number;
  _isoweb_set_character_sprite(
    id: number,
    state: number,
    action: number,
    facing: number,
    resource: number,
    frameCount: number,
    columns: number,
    rows: number,
    nominalFps: number,
    worldWidth: number,
    worldHeight: number,
    loop: number
  ): number;
  _isoweb_set_character_action(id: number, action: number): number;
  _isoweb_register_sprite_atlas(resource: number, width: number, height: number, rgba: number, byteCount: number): number;
  _isoweb_set_base_movement_speed(speed: number): void;
  _isoweb_set_selection_mode(mode: number): void;
  _isoweb_set_selection_style(red: number, green: number, blue: number, strength: number): void;

  onRuntimeInitialized?: () => void;
}

export type PresentFrame = (
  heap: Uint8Array,
  pointer: number,
  width: number,
  height: number,
  yaw: number,
  panX: number,
  panY: number,
  zoomPreset: number,
  detailed: boolean,
  canPan: boolean,
  viewHeight: number,
  wholeZoomScale: number,
  controlMask: number
) => void;

declare global {
  var Module: IsowebModule;
  var isowebPresent: PresentFrame | undefined;

  interface Window {
    isowebViewHeightWorld?: number;
    isowebCameraCanPan?: boolean;
    isowebWholeZoomScale?: number;
  }
}
