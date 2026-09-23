import type { IsowebModule } from './runtime';
import { browserMode } from './browserMode';

type MemoryPerformance = Performance & {
  memory?: {
    usedJSHeapSize: number;
    totalJSHeapSize: number;
    jsHeapSizeLimit: number;
  };
  measureUserAgentSpecificMemory?: () => Promise<{
    bytes: number;
  }>;
};

type NavigatorDiagnostics = Navigator & {
  deviceMemory?: number;
};

const MIB = 1024 * 1024;
const MEMORY_SAMPLE_INTERVAL_MS = 2000;

function formatBytes(bytes: number | undefined): string {
  if (bytes === undefined || !Number.isFinite(bytes)) return 'unavailable';
  if (bytes < MIB) return `${(bytes / 1024).toFixed(1)} KiB`;
  if (bytes < 1024 * MIB) return `${(bytes / MIB).toFixed(1)} MiB`;
  return `${(bytes / (1024 * MIB)).toFixed(2)} GiB`;
}

function staticBackendLabel(value: number): string {
  if (value === 2) return 'WebGL2 GPU';
  if (value === 1) return 'multi-thread CPU';
  return 'single-thread CPU';
}

function gpuName(): string {
  const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
  const gl = canvas?.getContext('webgl2');
  if (!gl) return 'unavailable';
  const debug = gl.getExtension('WEBGL_debug_renderer_info');
  if (!debug) return 'WebGL2 device';
  return String(gl.getParameter(debug.UNMASKED_RENDERER_WEBGL) ?? 'WebGL2 device');
}

export class StatsOverlay {
  private readonly root: HTMLElement;
  private readonly values = new Map<string, HTMLElement>();
  private lastAnimationFrame = performance.now();
  private displayFrameSamples: number[] = [];
  private lastUpdate = performance.now();
  private lastPresentedCount = window.isowebPresentedFrameCount ?? 0;
  private renderFps = 0;
  private activity = 'starting';
  private readonly gpu = gpuName();
  private pageMemoryBytes: number | undefined;
  private memoryMeasurementPending = false;
  private lastMemoryMeasurement = Number.NEGATIVE_INFINITY;

  constructor(private readonly module: IsowebModule) {
    const root = document.createElement('aside');
    root.id = 'performance-stats';
    root.className = 'stats-panel';
    root.setAttribute('aria-label', 'IsoWeb rendering statistics');

    const header = document.createElement('div');
    header.className = 'stats-header';

    const title = document.createElement('strong');
    title.className = 'stats-title';
    title.textContent = 'IsoWeb stats';

    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'stats-toggle';
    toggle.setAttribute('aria-label', 'Minimise rendering statistics');
    toggle.setAttribute('aria-expanded', 'true');
    toggle.title = 'Minimise stats';
    header.append(title, toggle);
    root.appendChild(header);

    const scroll = document.createElement('div');
    scroll.className = 'stats-scroll';

    for (const label of [
      'FPS',
      'Frame',
      'Activity',
      'Requested',
      'Runtime',
      'Renderer',
      'Display',
      'CPU',
      'RAM',
      'VRAM',
      'GPU',
      'Preview'
    ]) {
      const row = document.createElement('div');
      row.className = 'stats-row';
      const key = document.createElement('span');
      key.className = 'stats-key';
      key.textContent = label;
      const value = document.createElement('span');
      value.className = 'stats-value';
      value.textContent = '…';
      row.append(key, value);
      scroll.appendChild(row);
      this.values.set(label, value);
    }

    root.appendChild(scroll);

    const setMinimised = (minimised: boolean): void => {
      root.classList.toggle('stats-panel--minimised', minimised);
      toggle.setAttribute('aria-expanded', minimised ? 'false' : 'true');
      toggle.setAttribute(
        'aria-label',
        minimised ? 'Restore rendering statistics' : 'Minimise rendering statistics'
      );
      toggle.title = minimised ? 'Restore stats' : 'Minimise stats';
      if (minimised) root.scrollLeft = 0;
    };
    toggle.addEventListener('click', event => {
      event.stopPropagation();
      setMinimised(!root.classList.contains('stats-panel--minimised'));
    });

    document.body.appendChild(root);
    this.root = root;
    this.requestMemoryMeasurement(performance.now());
    this.update(performance.now(), true);
  }

  frame(now: number, activity: string): void {
    this.activity = activity;
    const delta = now - this.lastAnimationFrame;
    this.lastAnimationFrame = now;
    if (delta > 0 && delta < 1000) {
      this.displayFrameSamples.push(delta);
      if (this.displayFrameSamples.length > 90) this.displayFrameSamples.shift();
    }
    if (now - this.lastUpdate >= 250) this.update(now, false);
  }

  private set(label: string, text: string): void {
    const target = this.values.get(label);
    if (target && target.textContent !== text) target.textContent = text;
  }

  private requestMemoryMeasurement(now: number): void {
    const memoryPerformance = performance as MemoryPerformance;
    const measure = memoryPerformance.measureUserAgentSpecificMemory;
    if (
      typeof measure !== 'function' ||
      this.memoryMeasurementPending ||
      now - this.lastMemoryMeasurement < MEMORY_SAMPLE_INTERVAL_MS
    ) {
      return;
    }

    this.memoryMeasurementPending = true;
    this.lastMemoryMeasurement = now;
    void measure.call(performance)
      .then(result => {
        if (Number.isFinite(result.bytes) && result.bytes >= 0) {
          this.pageMemoryBytes = result.bytes;
        }
      })
      .catch(() => {
        // The API can exist but still be unavailable without the browser's
        // required isolation/privacy conditions. Legacy heap telemetry and
        // directly measurable WASM memory remain useful fallbacks.
      })
      .finally(() => {
        this.memoryMeasurementPending = false;
      });
  }

  private update(now: number, force: boolean): void {
    const elapsed = Math.max(1, now - this.lastUpdate);
    const presented = window.isowebPresentedFrameCount ?? 0;
    if (!force) {
      this.renderFps = ((presented - this.lastPresentedCount) * 1000) / elapsed;
    }
    this.lastPresentedCount = presented;
    this.lastUpdate = now;
    this.requestMemoryMeasurement(now);

    const averageDisplayMs = this.displayFrameSamples.length
      ? this.displayFrameSamples.reduce((sum, value) => sum + value, 0) /
        this.displayFrameSamples.length
      : 0;
    const displayFps = averageDisplayMs > 0 ? 1000 / averageDisplayMs : 0;

    const backend = this.module._isoweb_last_static_render_backend();
    const threads = Math.max(1, this.module._isoweb_last_render_thread_count());
    const helpers = Math.max(0, this.module._isoweb_last_render_helper_rows());
    const logicalCores = navigator.hardwareConcurrency || 1;
    const presentation = window.isowebPresentationBackend ?? 'unknown';
    const gpuMs = window.isowebLastGpuMilliseconds;
    const staticGpuMs = window.isowebLastGpuStaticMilliseconds;
    const presentMs = window.isowebLastPresentMilliseconds ?? 0;

    const memoryPerformance = performance as MemoryPerformance;
    const legacyMemory = memoryPerformance.memory;
    const wasmBytes = this.module.HEAPU8?.buffer?.byteLength;
    const memoryParts: string[] = [];
    if (legacyMemory) {
      memoryParts.push(
        `${formatBytes(legacyMemory.usedJSHeapSize)} / ${formatBytes(legacyMemory.totalJSHeapSize)} JS`
      );
    }
    if (this.pageMemoryBytes !== undefined) {
      memoryParts.push(`~${formatBytes(this.pageMemoryBytes)} page`);
    }
    if (memoryParts.length === 0) memoryParts.push('browser heap not exposed');
    memoryParts.push(`WASM ${formatBytes(wasmBytes)}`);

    const deviceMemory = (navigator as NavigatorDiagnostics).deviceMemory;
    if (deviceMemory) memoryParts.push(`device ~${deviceMemory} GiB`);

    const frameDetail = [
      `${presentMs.toFixed(2)} ms present`,
      gpuMs !== undefined ? `${gpuMs.toFixed(2)} ms GPU present` : null,
      staticGpuMs !== undefined && backend === 2 ? `${staticGpuMs.toFixed(2)} ms GPU trace` : null
    ].filter(Boolean).join(' · ');

    this.set('FPS', `${this.renderFps.toFixed(1)} render · ${displayFps.toFixed(1)} display`);
    this.set('Frame', frameDetail || 'waiting for first frame');
    const recentPresent =
      now - (window.isowebLastPresentedAt ?? Number.NEGATIVE_INFINITY) < 300;
    this.set(
      'Activity',
      this.activity === 'idle' && recentPresent ? 'render / presentation' : this.activity
    );
    const requested = browserMode();
    const requestedModes = [
      requested.threaded ? 'threaded' : 'single-thread',
      requested.webgl ? 'webgl' : 'cpu'
    ];
    this.set('Requested', requestedModes.join(' + '));
    const runtimeMode = globalThis.isowebRuntimeMode ?? 'single-thread';
    const fallback = globalThis.isowebRuntimeFallbackReason;
    this.set('Runtime', fallback ? `${runtimeMode} · fallback: ${fallback}` : runtimeMode);
    this.set('Renderer', staticBackendLabel(backend));
    this.set('Display', presentation === 'webgl2' ? 'WebGL2' : 'Canvas2D');
    this.set(
      'CPU',
      backend === 2
        ? `GPU static trace; CPU helpers ${threads}/${logicalCores} logical`
        : `${threads}/${logicalCores} logical render threads · ${helpers} helper rows`
    );
    this.set('RAM', memoryParts.join(' · '));
    this.set(
      'VRAM',
      presentation === 'webgl2'
        ? `~${formatBytes((window.isowebFrameGpuBytes ?? 0) + (window.isowebStaticGpuBytes ?? 0))} tracked`
        : 'not used by Canvas2D presenter'
    );
    this.set('GPU', this.gpu);
    this.set(
      'Preview',
      `${this.module._isoweb_preview_refined_sample_count()} refined · ` +
      `${this.module._isoweb_preview_demanded_texel_count()} demanded`
    );
  }
}
