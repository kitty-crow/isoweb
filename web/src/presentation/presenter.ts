import { getAppElements } from '../dom/elements';
import type { PresentFrame } from '../runtime';

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
    const { canvas, loading, status, controls } = getAppElements();

    if (canvas.width !== width) canvas.width = width;
    if (canvas.height !== height) canvas.height = height;

    const context = canvas.getContext('2d', { alpha: false });
    if (!context) return;

    const image = context.createImageData(width, height);
    image.data.set(heap.subarray(pointer, pointer + width * height * 4));
    context.putImageData(image, 0, 0);

    window.isowebViewHeightWorld = viewHeight;
    window.isowebCameraCanPan = canPan;
    window.isowebWholeZoomScale = wholeZoomScale;

    controls.zoomIn.disabled = !enabled(controlMask, CONTROL_ZOOM_IN);
    controls.zoomOut.disabled = !enabled(controlMask, CONTROL_ZOOM_OUT);
    controls.resetZoom.disabled = !enabled(controlMask, CONTROL_RESET_ZOOM);
    controls.resetYaw.disabled = !enabled(controlMask, CONTROL_RESET_YAW);

    controls.counterClockwise.disabled = false;
    controls.clockwise.disabled = false;

    controls.panUp.disabled = !enabled(controlMask, CONTROL_PAN_UP);
    controls.panDown.disabled = !enabled(controlMask, CONTROL_PAN_DOWN);
    controls.panLeft.disabled = !enabled(controlMask, CONTROL_PAN_LEFT);
    controls.panRight.disabled = !enabled(controlMask, CONTROL_PAN_RIGHT);
    controls.resetCamera.disabled = !enabled(controlMask, CONTROL_RESET_PAN);

    document.documentElement.classList.add('wasm-ready');
    loading.hidden = true;

    let zoomLabel = '1x';
    if (zoomPreset === 0) zoomLabel = 'whole';
    else if (zoomPreset === 1) zoomLabel = '0.25x';
    else if (zoomPreset === 2) zoomLabel = '0.5x';
    else if (zoomPreset === 4) zoomLabel = '2x';
    else if (zoomPreset === 5) zoomLabel = '4x';

    status.textContent =
      `Camera ${yaw} degrees around Z; pan X ${panX.toFixed(2)}; Y ${panY.toFixed(2)}; zoom ${zoomLabel}` +
      `${detailed ? ' detailed' : ' regular'}` +
      `${canPan ? '' : '; panning disabled'}`;
  };

  globalThis.isowebPresent = present;
}
