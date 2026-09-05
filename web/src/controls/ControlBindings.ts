import { INPUT } from '../config';
import type { ControlElements } from '../dom/elements';
import type { IsowebModule } from '../runtime';
import { PanQueue } from '../input/PanQueue';

export class ControlBindings {
  constructor(
    private readonly controls: ControlElements,
    private readonly module: IsowebModule,
    private readonly panQueue: PanQueue
  ) {}

  bind(): void {
    this.controls.zoomIn.addEventListener('click', () => this.module._isoweb_zoom_in());
    this.controls.resetZoom.addEventListener('click', () => this.module._isoweb_reset_zoom());
    this.controls.counterClockwise.addEventListener('click', () => this.module._isoweb_rotate_counterclockwise());
    this.controls.resetYaw.addEventListener('click', () => this.module._isoweb_reset_yaw());
    this.controls.clockwise.addEventListener('click', () => this.module._isoweb_rotate_clockwise());
    this.controls.zoomOut.addEventListener('click', () => this.module._isoweb_zoom_out());

    this.controls.panUp.addEventListener('click', () => this.panQueue.queue(0, INPUT.panButtonStep));
    this.controls.panDown.addEventListener('click', () => this.panQueue.queue(0, -INPUT.panButtonStep));
    this.controls.panLeft.addEventListener('click', () => this.panQueue.queue(-INPUT.panButtonStep, 0));
    this.controls.panRight.addEventListener('click', () => this.panQueue.queue(INPUT.panButtonStep, 0));
    this.controls.resetCamera.addEventListener('click', () => this.module._isoweb_reset_camera());
  }

  enableInitialState(): void {
    [
      this.controls.zoomIn,
      this.controls.resetZoom,
      this.controls.zoomOut,
      this.controls.counterClockwise,
      this.controls.resetYaw,
      this.controls.clockwise
    ].forEach(control => {
      control.disabled = false;
    });

    const panEnabled = window.isowebCameraCanPan !== false;
    [
      this.controls.panUp,
      this.controls.panDown,
      this.controls.panLeft,
      this.controls.panRight,
      this.controls.resetCamera
    ].forEach(control => {
      control.disabled = !panEnabled;
    });
  }
}
