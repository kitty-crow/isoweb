export interface ControlElements {
  zoomIn: HTMLButtonElement;
  resetZoom: HTMLButtonElement;
  zoomOut: HTMLButtonElement;
  counterClockwise: HTMLButtonElement;
  resetYaw: HTMLButtonElement;
  clockwise: HTMLButtonElement;
  panUp: HTMLButtonElement;
  panDown: HTMLButtonElement;
  panLeft: HTMLButtonElement;
  panRight: HTMLButtonElement;
  resetCamera: HTMLButtonElement;
}

export interface AppElements {
  viewport: HTMLElement;
  canvas: HTMLCanvasElement;
  loading: HTMLElement;
  status: HTMLElement;
  controls: ControlElements;
}

function required<T extends HTMLElement>(id: string): T {
  const element = document.getElementById(id);
  if (!element) throw new Error(`Missing required element #${id}`);
  return element as T;
}

export function getAppElements(): AppElements {
  return {
    viewport: required<HTMLElement>('viewport'),
    canvas: required<HTMLCanvasElement>('canvas'),
    loading: required<HTMLElement>('loading'),
    status: required<HTMLElement>('view-status'),
    controls: {
      zoomIn: required<HTMLButtonElement>('zoom-in'),
      resetZoom: required<HTMLButtonElement>('reset-zoom'),
      zoomOut: required<HTMLButtonElement>('zoom-out'),
      counterClockwise: required<HTMLButtonElement>('rotate-counterclockwise'),
      resetYaw: required<HTMLButtonElement>('reset-yaw'),
      clockwise: required<HTMLButtonElement>('rotate-clockwise'),
      panUp: required<HTMLButtonElement>('pan-up'),
      panDown: required<HTMLButtonElement>('pan-down'),
      panLeft: required<HTMLButtonElement>('pan-left'),
      panRight: required<HTMLButtonElement>('pan-right'),
      resetCamera: required<HTMLButtonElement>('reset-camera')
    }
  };
}
