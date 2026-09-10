import { INPUT } from '../config';
import type { IsowebModule } from '../runtime';
import { PanQueue } from './PanQueue';
import { ViewportController } from '../viewport/ViewportController';

type Point = { x: number; y: number };
type TapRecord = { point: Point; timestamp: number; pointerType: string };

const TAP_THRESHOLD_PX = 8;
const DOUBLE_TAP_WINDOW_MS = 325;
const DOUBLE_TAP_RADIUS_PX = 24;
const SCREEN_VERTICAL_WORLD_SCALE = Math.sqrt(3);

export class PointerController {
  private readonly pointers = new Map<number, Point>();
  private readonly starts = new Map<number, Point>();
  private pinchDistance = 0;
  private rotationAngle = 0;
  private rotationAccumulator = 0;
  private hadMultiTouch = false;
  private lastTap: TapRecord | null = null;

  constructor(
    private readonly viewport: HTMLElement,
    private readonly module: IsowebModule,
    private readonly viewportController: ViewportController,
    private readonly panQueue: PanQueue
  ) {}

  bind(): void {
    this.viewport.addEventListener('pointerdown', event => this.onPointerDown(event));
    this.viewport.addEventListener('pointermove', event => this.onPointerMove(event));
    this.viewport.addEventListener('pointerup', event => this.endPointer(event, false));
    this.viewport.addEventListener('pointercancel', event => this.endPointer(event, true));

    // Safari can still recognise browser-level gestures even with a locked
    // viewport meta tag. The game owns every gesture in this viewport, so
    // explicitly cancel native double-tap/gesture defaults as a second line
    // of defence. Scene touchend has no browser click semantic to preserve.
    this.viewport.addEventListener('dblclick', this.preventBrowserGesture);
    this.viewport.addEventListener('touchend', this.preventSceneTouchEnd, { passive: false });
    for (const name of ['gesturestart', 'gesturechange', 'gestureend']) {
      this.viewport.addEventListener(name, this.preventBrowserGesture, { passive: false });
    }
  }

  private readonly preventBrowserGesture = (event: Event): void => {
    event.preventDefault();
  };

  private readonly preventSceneTouchEnd = (event: TouchEvent): void => {
    const target = event.target;
    // Ordinary control buttons still use their click events. Their CSS
    // touch-action is locked separately, so do not suppress a single tap here.
    if (target instanceof Element && target.closest('button')) return;
    event.preventDefault();
  };

  private zoomStep(direction: number): void {
    if (direction > 0) this.module._isoweb_zoom_in();
    else if (direction < 0) this.module._isoweb_zoom_out();
  }

  private normaliseAngleDelta(value: number): number {
    while (value > Math.PI) value -= Math.PI * 2;
    while (value < -Math.PI) value += Math.PI * 2;
    return value;
  }

  private currentPinchDistance(): number {
    if (this.pointers.size < 2) return 0;
    const points = Array.from(this.pointers.values());
    const dx = points[0].x - points[1].x;
    const dy = points[0].y - points[1].y;
    return Math.hypot(dx, dy);
  }

  private currentTouchAngle(): number {
    if (this.pointers.size < 2) return 0;
    const points = Array.from(this.pointers.values());
    return Math.atan2(points[1].y - points[0].y, points[1].x - points[0].x);
  }

  private distanceFromStart(pointerId: number, point: Point): number {
    const start = this.starts.get(pointerId);
    return start ? Math.hypot(point.x - start.x, point.y - start.y) : 0;
  }

  private dispatchTap(screenPoint: Point, rendererPoint: Point, pointerType: string): void {
    const now = performance.now();
    const previous = this.lastTap;
    const isDoubleTap = previous !== null &&
      previous.pointerType === pointerType &&
      now - previous.timestamp <= DOUBLE_TAP_WINDOW_MS &&
      Math.hypot(screenPoint.x - previous.point.x, screenPoint.y - previous.point.y) <= DOUBLE_TAP_RADIUS_PX;

    if (isDoubleTap) {
      this.lastTap = null;
      // The engine only accepts this gesture when the second tap actually
      // hits a Character. If it does not, preserve ordinary rapid tapping by
      // falling back to the normal tap action for this second tap.
      if (this.module._isoweb_pointer_double_tap(rendererPoint.x, rendererPoint.y)) return;
      this.module._isoweb_pointer_tap(rendererPoint.x, rendererPoint.y, 1);
      return;
    }

    this.lastTap = { point: screenPoint, timestamp: now, pointerType };
    this.module._isoweb_pointer_tap(rendererPoint.x, rendererPoint.y, 1);
  }

  private onPointerDown(event: PointerEvent): void {
    const target = event.target;
    if (target instanceof Element && target.closest('button')) return;
    if (event.pointerType === 'mouse' && event.button !== 0) return;
    if (event.pointerType !== 'mouse') event.preventDefault();

    const point = { x: event.clientX, y: event.clientY };
    this.pointers.set(event.pointerId, point);
    this.starts.set(event.pointerId, point);
    this.viewport.setPointerCapture(event.pointerId);

    if (event.pointerType !== 'mouse' && this.pointers.size >= 2) {
      this.hadMultiTouch = true;
      this.pinchDistance = this.currentPinchDistance();
      this.rotationAngle = this.currentTouchAngle();
      this.rotationAccumulator = 0;
    }
  }

  private onPointerMove(event: PointerEvent): void {
    const previous = this.pointers.get(event.pointerId);
    if (!previous) return;
    if (event.pointerType !== 'mouse') event.preventDefault();

    const next = { x: event.clientX, y: event.clientY };
    this.pointers.set(event.pointerId, next);

    if (event.pointerType === 'mouse') return;

    if (this.pointers.size >= 2) {
      this.hadMultiTouch = true;
      const distance = this.currentPinchDistance();
      if (this.pinchDistance > 0) {
        const ratio = distance / this.pinchDistance;
        if (ratio >= INPUT.pinchStepRatioIn) {
          this.zoomStep(1);
          this.pinchDistance = distance;
        } else if (ratio <= INPUT.pinchStepRatioOut) {
          this.zoomStep(-1);
          this.pinchDistance = distance;
        }
      } else {
        this.pinchDistance = distance;
      }

      const angle = this.currentTouchAngle();
      this.rotationAccumulator += this.normaliseAngleDelta(angle - this.rotationAngle);
      this.rotationAngle = angle;

      while (this.rotationAccumulator >= INPUT.touchRotateStep) {
        this.module._isoweb_rotate_clockwise();
        this.rotationAccumulator -= INPUT.touchRotateStep;
      }
      while (this.rotationAccumulator <= -INPUT.touchRotateStep) {
        this.module._isoweb_rotate_counterclockwise();
        this.rotationAccumulator += INPUT.touchRotateStep;
      }
      return;
    }

    if (this.distanceFromStart(event.pointerId, next) < TAP_THRESHOLD_PX) return;

    const dx = next.x - previous.x;
    const dy = next.y - previous.y;
    this.panQueue.queue(
      -this.viewportController.pixelsToWorld(dx),
      -this.viewportController.pixelsToWorld(dy) * SCREEN_VERTICAL_WORLD_SCALE
    );
  }

  private endPointer(event: PointerEvent, cancelled: boolean): void {
    const last = this.pointers.get(event.pointerId);
    const start = this.starts.get(event.pointerId);
    if (!last || !start) return;
    if (event.pointerType !== 'mouse') event.preventDefault();

    const distance = Math.hypot(last.x - start.x, last.y - start.y);
    const wasMouse = event.pointerType === 'mouse';
    const multiTouch = this.hadMultiTouch;

    this.pointers.delete(event.pointerId);
    this.starts.delete(event.pointerId);

    if (!cancelled && wasMouse) {
      const from = this.viewportController.rendererPoint(start.x, start.y);
      const to = this.viewportController.rendererPoint(last.x, last.y);
      if (from && to) {
        if (distance < TAP_THRESHOLD_PX) {
          this.dispatchTap(last, to, event.pointerType);
        } else {
          this.module._isoweb_drag_select(from.x, from.y, to.x, to.y, event.shiftKey ? 1 : 0);
        }
      }
    } else if (!cancelled && !wasMouse && !multiTouch && distance < TAP_THRESHOLD_PX) {
      const point = this.viewportController.rendererPoint(last.x, last.y);
      if (point) this.dispatchTap(last, point, event.pointerType);
    }

    if (this.pointers.size >= 2) {
      this.pinchDistance = this.currentPinchDistance();
      this.rotationAngle = this.currentTouchAngle();
    } else {
      this.pinchDistance = 0;
      this.rotationAccumulator = 0;
      if (this.pointers.size === 0) this.hadMultiTouch = false;
    }
  }
}
