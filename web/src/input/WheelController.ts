import { INPUT } from '../config';
import type { IsowebModule } from '../runtime';
import { PanQueue } from './PanQueue';
import { ViewportController } from '../viewport/ViewportController';

export class WheelController {
  private zoomAccumulator = 0;
  private yawAccumulator = 0;

  constructor(
    private readonly viewport: HTMLElement,
    private readonly module: IsowebModule,
    private readonly viewportController: ViewportController,
    private readonly panQueue: PanQueue
  ) {}

  bind(): void {
    this.viewport.addEventListener('wheel', event => this.onWheel(event), { passive: false });
  }

  private zoomStep(direction: number): void {
    if (direction > 0) this.module._isoweb_zoom_in();
    else if (direction < 0) this.module._isoweb_zoom_out();
  }

  private onWheel(event: WheelEvent): void {
    event.preventDefault();
    const rect = this.viewport.getBoundingClientRect();
    let delta = event.deltaY;
    if (event.deltaMode === WheelEvent.DOM_DELTA_LINE) delta *= 16;
    else if (event.deltaMode === WheelEvent.DOM_DELTA_PAGE) delta *= rect.height;

    if (event.shiftKey) {
      this.yawAccumulator += delta;
      this.zoomAccumulator = 0;
      while (this.yawAccumulator >= INPUT.yawWheelThreshold) {
        this.module._isoweb_rotate_clockwise();
        this.yawAccumulator -= INPUT.yawWheelThreshold;
      }
      while (this.yawAccumulator <= -INPUT.yawWheelThreshold) {
        this.module._isoweb_rotate_counterclockwise();
        this.yawAccumulator += INPUT.yawWheelThreshold;
      }
      return;
    }

    this.yawAccumulator = 0;
    if (event.altKey) {
      this.zoomAccumulator += delta;
      while (this.zoomAccumulator <= -INPUT.zoomWheelThreshold) {
        this.zoomStep(1);
        this.zoomAccumulator += INPUT.zoomWheelThreshold;
      }
      while (this.zoomAccumulator >= INPUT.zoomWheelThreshold) {
        this.zoomStep(-1);
        this.zoomAccumulator -= INPUT.zoomWheelThreshold;
      }
      return;
    }

    this.zoomAccumulator = 0;
    const worldDelta = this.viewportController.pixelsToWorld(delta) * 0.35;
    if (event.ctrlKey) this.panQueue.queue(worldDelta, 0);
    else this.panQueue.queue(0, worldDelta);
  }
}
