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

    this.controls.levelUp.addEventListener('click', () => {
      this.module._isoweb_level_up();
      this.syncLevelState();
    });
    this.controls.levelDown.addEventListener('click', () => {
      this.module._isoweb_level_down();
      this.syncLevelState();
    });
    this.controls.resetLevel.addEventListener('click', () => {
      this.module._isoweb_reset_level();
      this.syncLevelState();
    });
  }

  enableInitialState(): void {
    this.controls.counterClockwise.disabled = false;
    this.controls.clockwise.disabled = false;
    this.syncLevelState();
  }

  private syncLevelState(): void {
    const count = this.module._isoweb_level_count();
    const active = this.module._isoweb_active_level_index();
    const defaultLevel = this.module._isoweb_default_level_index();

    this.controls.levelUp.disabled = active + 1 >= count;
    this.controls.levelDown.disabled = active <= 0;
    this.controls.resetLevel.disabled = active === defaultLevel;
  }
}
