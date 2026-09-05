import { getAppElements } from '../dom/elements';
import type { PresentFrame } from '../runtime';

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
    wholeZoomScale
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

    controls.panUp.disabled = !canPan;
    controls.panDown.disabled = !canPan;
    controls.panLeft.disabled = !canPan;
    controls.panRight.disabled = !canPan;
    controls.resetCamera.disabled = !canPan;

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
