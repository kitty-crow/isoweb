import { getAppElements } from '../dom/elements';
import type { PresentFrame } from '../runtime';
import { createFrameBackend, type FrameBackend } from './frameBackend';

const CONTROL_ZOOM_IN = 1 << 0;
const CONTROL_ZOOM_OUT = 1 << 1;
const CONTROL_RESET_ZOOM = 1 << 2;
const CONTROL_RESET_YAW = 1 << 3;
const CONTROL_PAN_UP = 1 << 4;
const CONTROL_PAN_DOWN = 1 << 5;
const CONTROL_PAN_LEFT = 1 << 6;
const CONTROL_PAN_RIGHT = 1 << 7;
const CONTROL_RESET_PAN = 1 << 8;

function enabled(mask: number, flag: number): boolean {
  return (mask & flag) !== 0;
}

export function installPresenter(): void {
  let elements: ReturnType<typeof getAppElements> | null = null;
  let frameBackend: FrameBackend | null = null;
  let lastControlMask = -1;
  let lastActiveLevel = -1;
  let levelCount = -1;
  let defaultLevel = -1;
  let lastStatus = '';
  let readyPresented = false;

  const present: PresentFrame = (
    heap,
    pointer,
    width,
    height,
    yaw,
    panX,
    panY,
    zoomPreset,
    detailed,
    canPan,
    viewHeight,
    wholeZoomScale,
    controlMask
  ) => {
    if (!elements) elements = getAppElements();
    const { canvas, loading, status, controls } = elements;

    if (canvas.width !== width) canvas.width = width;
    if (canvas.height !== height) canvas.height = height;

    if (!frameBackend) {
      frameBackend = createFrameBackend(canvas);
      window.isowebPresentationBackend = frameBackend.name;
    }
    const presentStarted = performance.now();
    frameBackend.present(heap, pointer, width, height);
    window.isowebLastPresentMilliseconds = performance.now() - presentStarted;

    window.isowebViewHeightWorld = viewHeight;
    window.isowebCameraCanPan = canPan;
    window.isowebWholeZoomScale = wholeZoomScale;

    if (controlMask !== lastControlMask) {
      controls.zoomIn.disabled = !enabled(controlMask, CONTROL_ZOOM_IN);
      controls.zoomOut.disabled = !enabled(controlMask, CONTROL_ZOOM_OUT);
      controls.counterClockwise.disabled = false;
      controls.clockwise.disabled = false;
      controls.panUp.disabled = !enabled(controlMask, CONTROL_PAN_UP);
      controls.panDown.disabled = !enabled(controlMask, CONTROL_PAN_DOWN);
      controls.panLeft.disabled = !enabled(controlMask, CONTROL_PAN_LEFT);
      controls.panRight.disabled = !enabled(controlMask, CONTROL_PAN_RIGHT);

      // Centre discs are controls even at their reset position, so they must
      // remain pointer-interactive while reset availability is separate state.
      controls.resetZoom.disabled = false;
      controls.resetYaw.disabled = false;
      controls.resetCamera.disabled = false;
      controls.resetZoom.dataset.resetEnabled = enabled(controlMask, CONTROL_RESET_ZOOM) ? 'true' : 'false';
      controls.resetYaw.dataset.resetEnabled = enabled(controlMask, CONTROL_RESET_YAW) ? 'true' : 'false';
      controls.resetCamera.dataset.resetEnabled = enabled(controlMask, CONTROL_RESET_PAN) ? 'true' : 'false';
      lastControlMask = controlMask;
    }

    const module = globalThis.Module;
    if (levelCount < 0) levelCount = module._isoweb_level_count();
    if (defaultLevel < 0) defaultLevel = module._isoweb_default_level_index();
    const activeLevel = module._isoweb_active_level_index();
    if (activeLevel !== lastActiveLevel) {
      controls.levelUp.disabled = activeLevel + 1 >= levelCount;
      controls.levelDown.disabled = activeLevel <= 0;
      controls.resetLevel.disabled = false;
      controls.resetLevel.dataset.resetEnabled = activeLevel === defaultLevel ? 'false' : 'true';
      lastActiveLevel = activeLevel;
    }

    if (!readyPresented) {
      document.documentElement.classList.add('wasm-ready');
      loading.hidden = true;
      readyPresented = true;
    }

    let zoomLabel = '1x';
    if (zoomPreset === 0) zoomLabel = 'whole';
    else if (zoomPreset === 1) zoomLabel = '0.25x';
    else if (zoomPreset === 2) zoomLabel = '0.5x';
    else if (zoomPreset === 4) zoomLabel = '2x';
    else if (zoomPreset === 5) zoomLabel = '4x';

    const nextStatus =
      `Camera ${yaw} degrees around Z; pan X ${panX.toFixed(2)}; Y ${panY.toFixed(2)}; zoom ${zoomLabel}` +
      `${detailed ? ' detailed' : ' regular'}` +
      `${canPan ? '' : '; panning disabled'}`;
    if (nextStatus !== lastStatus) {
      status.textContent = nextStatus;
      lastStatus = nextStatus;
    }
  };

  globalThis.isowebPresent = present;
}
