import type { IsowebModule } from '../runtime';

export class PanQueue {
  private pendingX = 0;
  private pendingY = 0;
  private frame = 0;

  constructor(private readonly module: IsowebModule) {}

  queue(screenRight: number, screenDown: number): void {
    this.pendingX += screenRight;
    this.pendingY += screenDown;
    if (this.frame) return;

    this.frame = requestAnimationFrame(() => {
      const x = this.pendingX;
      const y = this.pendingY;
      this.pendingX = 0;
      this.pendingY = 0;
      this.frame = 0;
      if (x !== 0 || y !== 0) this.module._isoweb_pan(x, y);
    });
  }
}
