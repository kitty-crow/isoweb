import { INPUT } from '../config';
import type { IsowebModule } from '../runtime';
import { PanQueue } from '../input/PanQueue';

export const enum CentreStickId {
  Zoom = 0,
  Yaw = 1,
  Pan = 2,
  Level = 3
}

type StickAxis = 'horizontal' | 'vertical' | 'radial';

type CentreStickSpec = {
  id: CentreStickId;
  element: HTMLButtonElement;
  axis: StickAxis;
  reset: () => void;
};

export type QuantisedStick = {
  x: number;
  y: number;
  strength: number;
  level: number;
};

const EIGHTH_TURN = Math.PI / 4;
const TWO_PI = Math.PI * 2;

function clamp(value: number, minimum: number, maximum: number): number {
  return Math.max(minimum, Math.min(maximum, value));
}

function quantisedStrength(magnitude: number): { strength: number; level: number } {
  const deadzone = INPUT.joystickDeadzone;
  if (magnitude <= deadzone) return { strength: 0, level: 0 };
  const active = clamp((magnitude - deadzone) / (1 - deadzone), 0, 1);
  const level = Math.max(1, Math.min(INPUT.joystickLevels, Math.ceil(active * INPUT.joystickLevels)));
  return { strength: level / INPUT.joystickLevels, level };
}

export function quantisePanStick(x: number, y: number): QuantisedStick {
  const magnitude = Math.min(1, Math.hypot(x, y));
  const { strength, level } = quantisedStrength(magnitude);
  if (level === 0) return { x: 0, y: 0, strength: 0, level: 0 };

  const angle = Math.round(Math.atan2(y, x) / EIGHTH_TURN) * EIGHTH_TURN;
  return {
    x: Math.cos(angle) * strength,
    y: Math.sin(angle) * strength,
    strength,
    level
  };
}

export function quantiseAxisStick(value: number, horizontal: boolean): QuantisedStick {
  const signed = clamp(value, -1, 1);
  const { strength, level } = quantisedStrength(Math.abs(signed));
  if (level === 0) return { x: 0, y: 0, strength: 0, level: 0 };
  const direction = signed < 0 ? -1 : 1;
  return horizontal
    ? { x: direction * strength, y: 0, strength, level }
    : { x: 0, y: direction * strength, strength, level };
}

function rate(minimum: number, maximum: number, strength: number): number {
  return minimum + (maximum - minimum) * strength;
}

export class CentreJoystick {
  private pointerId: number | null = null;
  private originX = 0;
  private originY = 0;
  private radius = 1;
  private rawX = 0;
  private rawY = 0;
  private dragged = false;
  private frame = 0;
  private previousTime = 0;
  private repeatAccumulator = 0;
  private visualX = 0;
  private visualY = 0;

  constructor(
    private readonly module: IsowebModule,
    private readonly panQueue: PanQueue,
    private readonly spec: CentreStickSpec
  ) {}

  bind(): void {
    const element = this.spec.element;
    element.disabled = false;
    element.dataset.joystick = 'true';
    element.addEventListener('pointerdown', this.onPointerDown);
    element.addEventListener('pointermove', this.onPointerMove);
    element.addEventListener('pointerup', this.onPointerUp);
    element.addEventListener('pointercancel', this.onPointerCancel);
    element.addEventListener('lostpointercapture', this.onLostPointerCapture);
    element.addEventListener('contextmenu', event => event.preventDefault());
  }

  private readonly onPointerDown = (event: PointerEvent): void => {
    if (this.pointerId !== null || event.button !== 0) return;
    const rect = this.spec.element.getBoundingClientRect();
    this.pointerId = event.pointerId;
    this.originX = rect.left + rect.width * 0.5;
    this.originY = rect.top + rect.height * 0.5;
    // Allow the thumb/finger to travel beyond the visible disc while keeping
    // full-strength reachable on small 44px touch hitboxes.
    this.radius = Math.max(24, Math.min(64, Math.max(rect.width, rect.height) * 0.85));
    this.rawX = 0;
    this.rawY = 0;
    this.dragged = false;
    this.repeatAccumulator = 0;
    this.previousTime = performance.now();
    this.visualX = 0;
    this.visualY = 0;
    this.spec.element.setPointerCapture(event.pointerId);
    event.preventDefault();
  };

  private readonly onPointerMove = (event: PointerEvent): void => {
    if (event.pointerId !== this.pointerId) return;
    this.rawX = clamp((event.clientX - this.originX) / this.radius, -1, 1);
    this.rawY = clamp((event.clientY - this.originY) / this.radius, -1, 1);

    const magnitude = this.spec.axis === 'horizontal'
      ? Math.abs(this.rawX)
      : this.spec.axis === 'vertical'
        ? Math.abs(this.rawY)
        : Math.hypot(this.rawX, this.rawY);
    if (magnitude > INPUT.joystickDeadzone) this.dragged = true;

    if (this.dragged && !this.frame) {
      this.previousTime = performance.now();
      // Give the first non-neutral drag an immediate discrete response.
      this.repeatAccumulator = 1;
      this.frame = requestAnimationFrame(this.animate);
    }
    event.preventDefault();
  };

  private readonly onPointerUp = (event: PointerEvent): void => {
    if (event.pointerId !== this.pointerId) return;
    event.preventDefault();
    if (this.spec.element.hasPointerCapture(event.pointerId)) {
      this.spec.element.releasePointerCapture(event.pointerId);
    }
    this.finish(false);
  };

  private readonly onPointerCancel = (event: PointerEvent): void => {
    if (event.pointerId !== this.pointerId) return;
    event.preventDefault();
    this.finish(true);
  };

  private readonly onLostPointerCapture = (event: PointerEvent): void => {
    if (event.pointerId !== this.pointerId) return;
    this.finish(true);
  };

  private quantised(): QuantisedStick {
    if (this.spec.axis === 'horizontal') return quantiseAxisStick(this.rawX, true);
    if (this.spec.axis === 'vertical') return quantiseAxisStick(this.rawY, false);
    return quantisePanStick(this.rawX, this.rawY);
  }

  private setVisual(stick: QuantisedStick): boolean {
    if (stick.x === this.visualX && stick.y === this.visualY) return false;
    this.visualX = stick.x;
    this.visualY = stick.y;
    this.module._isoweb_set_control_stick(this.spec.id, stick.x, stick.y);
    return true;
  }

  private readonly animate = (now: number): void => {
    this.frame = 0;
    if (this.pointerId === null || !this.dragged) return;

    const deltaSeconds = Math.min(0.05, Math.max(0, (now - this.previousTime) / 1000));
    this.previousTime = now;
    const stick = this.quantised();
    const visualChanged = this.setVisual(stick);
    let redrawn = false;

    if (stick.level > 0) {
      switch (this.spec.id) {
        case CentreStickId.Pan: {
          // Pointer-space +Y is screen-down, while the established engine pan
          // binding uses +down to move the view upward. Preserve that mapping.
          const right = stick.x * INPUT.joystickPanUnitsPerSecond * deltaSeconds;
          const down = -stick.y * INPUT.joystickPanUnitsPerSecond * deltaSeconds;
          if (right !== 0 || down !== 0) {
            this.panQueue.queue(right, down);
            redrawn = true;
          }
          break;
        }
        case CentreStickId.Yaw:
          this.repeatAccumulator += deltaSeconds * rate(
            INPUT.joystickYawStepsPerSecondMin,
            INPUT.joystickYawStepsPerSecondMax,
            stick.strength
          );
          while (this.repeatAccumulator >= 1) {
            this.repeatAccumulator -= 1;
            if (stick.x > 0) this.module._isoweb_rotate_clockwise();
            else this.module._isoweb_rotate_counterclockwise();
            redrawn = true;
          }
          break;
        case CentreStickId.Zoom:
          this.repeatAccumulator += deltaSeconds * rate(
            INPUT.joystickZoomStepsPerSecondMin,
            INPUT.joystickZoomStepsPerSecondMax,
            stick.strength
          );
          while (this.repeatAccumulator >= 1) {
            this.repeatAccumulator -= 1;
            if (stick.x > 0) this.module._isoweb_zoom_in();
            else this.module._isoweb_zoom_out();
            redrawn = true;
          }
          break;
        case CentreStickId.Level:
          this.repeatAccumulator += deltaSeconds * rate(
            INPUT.joystickLevelStepsPerSecondMin,
            INPUT.joystickLevelStepsPerSecondMax,
            stick.strength
          );
          while (this.repeatAccumulator >= 1) {
            this.repeatAccumulator -= 1;
            const before = this.module._isoweb_active_level_index();
            if (stick.y < 0) this.module._isoweb_level_up();
            else this.module._isoweb_level_down();
            const after = this.module._isoweb_active_level_index();
            if (after !== before) redrawn = true;
            else if (visualChanged) {
              this.module._isoweb_render();
              redrawn = true;
            }
          }
          break;
      }
    }

    if (visualChanged && !redrawn && this.spec.id !== CentreStickId.Pan) {
      this.module._isoweb_render();
    }

    this.frame = requestAnimationFrame(this.animate);
  };

  private finish(cancelled: boolean): void {
    if (this.pointerId === null) return;
    const wasDragged = this.dragged;
    this.pointerId = null;
    this.rawX = 0;
    this.rawY = 0;
    this.dragged = false;
    this.repeatAccumulator = 0;
    if (this.frame) cancelAnimationFrame(this.frame);
    this.frame = 0;

    const hadVisualOffset = this.visualX !== 0 || this.visualY !== 0;
    this.visualX = 0;
    this.visualY = 0;
    this.module._isoweb_set_control_stick(this.spec.id, 0, 0);

    if (!cancelled && !wasDragged) {
      this.spec.reset();
      return;
    }
    if (hadVisualOffset) this.module._isoweb_render();
  }
}

// Keep the constants used above visibly finite for bundlers/static analysis and
// avoid accidental angle-normalisation drift in future changes.
void TWO_PI;
