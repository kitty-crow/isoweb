import { BASE_VIEW_HEIGHT, INPUT } from '../config';
import type { IsowebModule } from '../runtime';
import { ControlLayout } from '../controls/ControlLayout';

export class ViewportController {
  private renderWidth = 512;
  private renderHeight = 288;
  private resizeFrame = 0;
  private readonly observer: ResizeObserver;

  constructor(
    private readonly viewport: HTMLElement,
    private readonly module: IsowebModule,
    private readonly layout: ControlLayout
  ) {
    this.observer = new ResizeObserver(() => this.scheduleResize());
  }

  startObserving(): void {
    window.addEventListener('resize', () => this.scheduleResize());
    window.addEventListener('orientationchange', () => this.scheduleResize());
    window.visualViewport?.addEventListener('resize', () => this.scheduleResize());
    this.observer.observe(this.viewport);
  }

  syncRendererSize(): void {
    const rect = this.viewport.getBoundingClientRect();
    if (rect.width <= 0 || rect.height <= 0) return;

    const size = this.chooseRenderSize(rect.width, rect.height);
    this.renderWidth = size.width;
    this.renderHeight = size.height;
    this.layout.position(this.renderWidth, this.renderHeight);
    this.module._isoweb_resize(this.renderWidth, this.renderHeight);
  }

  pixelsToWorld(pixels: number): number {
    const rect = this.viewport.getBoundingClientRect();
    if (rect.height <= 0) return 0;
    const viewHeight = Number(window.isowebViewHeightWorld) || BASE_VIEW_HEIGHT;
    return pixels * (viewHeight / rect.height);
  }

  rendererPoint(clientX: number, clientY: number): { x: number; y: number } | null {
    const rect = this.viewport.getBoundingClientRect();
    if (rect.width <= 0 || rect.height <= 0) return null;
    return {
      x: (clientX - rect.left) * (this.renderWidth / rect.width),
      y: (clientY - rect.top) * (this.renderHeight / rect.height)
    };
  }

  private scheduleResize(): void {
    if (this.resizeFrame) return;
    this.resizeFrame = requestAnimationFrame(() => {
      this.resizeFrame = 0;
      this.syncRendererSize();
    });
  }

  private chooseRenderSize(cssWidth: number, cssHeight: number): { width: number; height: number } {
    const safeWidth = Math.max(160, Math.round(cssWidth));
    const safeHeight = Math.max(160, Math.round(cssHeight));
    const pixels = safeWidth * safeHeight;
    const scale = pixels > INPUT.maxRenderPixels
      ? Math.sqrt(INPUT.maxRenderPixels / pixels)
      : 1;

    return {
      width: Math.max(160, Math.round(safeWidth * scale)),
      height: Math.max(160, Math.round(safeHeight * scale))
    };
  }
}
