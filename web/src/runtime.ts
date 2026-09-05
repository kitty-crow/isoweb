export interface IsowebModule {
  _isoweb_render(): void;
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

  _isoweb_state_begin(baseMovementSpeed: number): void;
  _isoweb_state_add_character(
    x: number,
    y: number,
    z: number,
    forwardX: number,
    forwardY: number,
    width: number,
    depth: number,
    height: number,
    levelIndex: number,
    solid: number,
    npc: number,
    controllable: number,
    speedMultiplier: number,
    selectionRed: number,
    selectionGreen: number,
    selectionBlue: number
  ): void;
  _isoweb_character_count(): number;
  _isoweb_pointer_action(normalisedX: number, normalisedY: number): void;
  _isoweb_clear_selection(): void;
  _isoweb_tick(deltaSeconds: number): void;

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
