import { INPUT } from '../config';
import type { ControlElements } from '../dom/elements';
import type { IsowebModule } from '../runtime';
import { PanQueue } from '../input/PanQueue';
import { CentreJoystick, CentreStickId } from './CentreJoystick';

const SCREEN_VERTICAL_WORLD_SCALE = Math.sqrt(3);

export class ControlBindings {
  private joysticks: CentreJoystick[] = [];

  constructor(
    private readonly controls: ControlElements,
    private readonly module: IsowebModule,
    private readonly panQueue: PanQueue
  ) {}

  bind(): void {
    this.controls.zoomIn.addEventListener('click', () => this.module._isoweb_zoom_in());
    this.controls.zoomOut.addEventListener('click', () => this.module._isoweb_zoom_out());
    this.controls.counterClockwise.addEventListener('click', () => this.module._isoweb_rotate_counterclockwise());
    this.controls.clockwise.addEventListener('click', () => this.module._isoweb_rotate_clockwise());

    this.controls.panUp.addEventListener('click', () => this.panQueue.queue(0, INPUT.panButtonStep * SCREEN_VERTICAL_WORLD_SCALE));
    this.controls.panDown.addEventListener('click', () => this.panQueue.queue(0, -INPUT.panButtonStep * SCREEN_VERTICAL_WORLD_SCALE));
    this.controls.panLeft.addEventListener('click', () => this.panQueue.queue(-INPUT.panButtonStep, 0));
    this.controls.panRight.addEventListener('click', () => this.panQueue.queue(INPUT.panButtonStep, 0));

    this.controls.levelUp.addEventListener('click', () => {
      this.module._isoweb_level_up();
      this.syncLevelState();
    });
    this.controls.levelDown.addEventListener('click', () => {
      this.module._isoweb_level_down();
      this.syncLevelState();
    });

    this.joysticks = [
      new CentreJoystick(this.module, {
        id: CentreStickId.Zoom,
        element: this.controls.resetZoom,
        axis: 'horizontal',
        reset: () => this.module._isoweb_reset_zoom()
      }),
      new CentreJoystick(this.module, {
        id: CentreStickId.Yaw,
        element: this.controls.resetYaw,
        axis: 'horizontal',
        reset: () => this.module._isoweb_reset_yaw()
      }),
      new CentreJoystick(this.module, {
        id: CentreStickId.Pan,
        element: this.controls.resetCamera,
        axis: 'radial',
        reset: () => this.module._isoweb_reset_camera()
      }),
      new CentreJoystick(this.module, {
        id: CentreStickId.Level,
        element: this.controls.resetLevel,
        axis: 'vertical',
        reset: () => {
          const before = this.module._isoweb_active_level_index();
          this.module._isoweb_reset_level();
          this.syncLevelState();
          // World::resetLevel intentionally skips redraw at the default level;
          // a centre-disc tap still needs to be a visible interaction.
          if (before === this.module._isoweb_default_level_index()) this.module._isoweb_render();
        }
      })
    ];
    for (const joystick of this.joysticks) joystick.bind();
  }

  enableInitialState(): void {
    this.controls.counterClockwise.disabled = false;
    this.controls.clockwise.disabled = false;
    this.controls.resetZoom.disabled = false;
    this.controls.resetYaw.disabled = false;
    this.controls.resetCamera.disabled = false;
    this.controls.resetLevel.disabled = false;
    this.syncLevelState();
  }

  private syncLevelState(): void {
    const count = this.module._isoweb_level_count();
    const active = this.module._isoweb_active_level_index();
    const defaultLevel = this.module._isoweb_default_level_index();

    this.controls.levelUp.disabled = active + 1 >= count;
    this.controls.levelDown.disabled = active <= 0;
    this.controls.resetLevel.disabled = false;
    this.controls.resetLevel.dataset.resetEnabled = active === defaultLevel ? 'false' : 'true';
  }
}
