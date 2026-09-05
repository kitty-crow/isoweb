import { INPUT } from '../config';
import type { IsowebModule } from '../runtime';
import { PanQueue } from './PanQueue';
import { ViewportController } from '../viewport/ViewportController';

type Point = { x: number; y: number };
type PointerStart = Point & { moved: boolean };

export class PointerController {
  private readonly pointers = new Map<number, Point>();
  private readonly starts = new Map<number, PointerStart>();
  private pinchDistance = 0;
  private rotationAngle = 0;
  private rotationAccumulator = 0;

  constructor(
    private readonly viewport: HTMLElement,
    private readonly module: IsowebModule,
    private readonly viewportController: ViewportController,
    private readonly panQueue: PanQueue
  ) {}

  bind(): void {
    this.viewport.addEventListener('pointerdown', event => this.onPointerDown(event));
    this.viewport.addEventListener('pointermove', event => this.onPointerMove(event));
    this.viewport.addEventListener('pointerup', event => this.endPointer(event, true));
    this.viewport.addEventListener('pointercancel', event => this.endPointer(event, false));
  }

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

  private suppressTapForActivePointers(): void {
    for (const start of this.starts.values()) start.moved = true;
  }

  private onPointerDown(event: PointerEvent): void {
    const target = event.target;
    if (target instanceof Element && target.closest('button')) return;
    if (event.pointerType === 'mouse' && event.button !== 0) return;

    this.starts.set(event.pointerId, {
      x: event.clientX,
      y: event.clientY,
      moved: false
    });
    this.viewport.setPointerCapture(event.pointerId);

    // Mouse is reserved for selection/destination clicks. Touch and pen retain
    // the existing one-finger pan and two-finger pinch/yaw gestures.
    if (event.pointerType === 'mouse') return;

    this.pointers.set(event.pointerId, { x: event.clientX, y: event.clientY });
    if (this.pointers.size >= 2) {
      this.suppressTapForActivePointers();
      this.pinchDistance = this.currentPinchDistance();
      this.rotationAngle = this.currentTouchAngle();
      this.rotationAccumulator = 0;
    }
  }

  private onPointerMove(event: PointerEvent): void {
    const start = this.starts.get(event.pointerId);
    if (start && !start.moved) {
      const distance = Math.hypot(event.clientX - start.x, event.clientY - start.y);
      if (distance > INPUT.pointerTapSlop) start.moved = true;
    }

    if (event.pointerType === 'mouse') return;

    const previous = this.pointers.get(event.pointerId);
    if (!previous) return;

    const next = { x: event.clientX, y: event.clientY };
    this.pointers.set(event.pointerId, next);

    if (this.pointers.size >= 2) {
      this.suppressTapForActivePointers();
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

    // Preserve tap intent until the pointer has actually moved far enough to
    // become a pan gesture. This prevents a tap on a character from nudging
    // the camera before selection is resolved.
    if (!start?.moved) return;

    const dx = next.x - previous.x;
    const dy = next.y - previous.y;
    this.panQueue.queue(
      -this.viewportController.pixelsToWorld(dx),
      -this.viewportController.pixelsToWorld(dy)
    );
  }

  private endPointer(event: PointerEvent, allowTap: boolean): void {
    const start = this.starts.get(event.pointerId);
    this.starts.delete(event.pointerId);

    if (event.pointerType !== 'mouse' && this.pointers.has(event.pointerId)) {
      this.pointers.delete(event.pointerId);
      if (this.pointers.size >= 2) {
        this.pinchDistance = this.currentPinchDistance();
        this.rotationAngle = this.currentTouchAngle();
      } else {
        this.pinchDistance = 0;
        this.rotationAccumulator = 0;
      }
    }

    if (this.viewport.hasPointerCapture(event.pointerId)) {
      this.viewport.releasePointerCapture(event.pointerId);
    }

    if (!allowTap || !start || start.moved) return;
    if (event.pointerType === 'mouse' && event.button !== 0) return;

    const rect = this.viewport.getBoundingClientRect();
    if (rect.width <= 0 || rect.height <= 0) return;

    const normalisedX = (event.clientX - rect.left) / rect.width;
    const normalisedY = (event.clientY - rect.top) / rect.height;
    if (normalisedX < 0 || normalisedX > 1 || normalisedY < 0 || normalisedY > 1) return;

    this.module._isoweb_pointer_action(normalisedX, normalisedY);
  }
}
