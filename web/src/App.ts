import { getAppElements } from './dom/elements';
import type { IsowebModule } from './runtime';
import { ControlLayout } from './controls/ControlLayout';
import { ControlBindings } from './controls/ControlBindings';
import { PanQueue } from './input/PanQueue';
import { PointerController } from './input/PointerController';
import { WorldStateLoader } from './state/WorldStateLoader';
import { WheelController } from './input/WheelController';
import { ViewportController } from './viewport/ViewportController';
import { StatsOverlay } from './StatsOverlay';
import { browserMode } from './browserMode';

export class App {
  constructor(private readonly module: IsowebModule) {}

  start(): void {
    const elements = getAppElements();
    const layout = new ControlLayout(elements.viewport, elements.controls);
    const viewport = new ViewportController(elements.viewport, this.module, layout);
    const panQueue = new PanQueue(this.module);
    const controls = new ControlBindings(elements.controls, this.module, panQueue);
    const wheel = new WheelController(elements.viewport, this.module, viewport, panQueue);
    const pointer = new PointerController(elements.viewport, this.module, viewport, panQueue);
    const browserArgs = new URLSearchParams(window.location.search);
    const mode = browserMode();
    const detailedZoomMode = mode.detailedZoom;
    const detailedYawMode = mode.detailedYaw;
    const obstaclesEnabled = browserArgs.get('obstacles') === '1';
    const statsEnabled = mode.stats;
    const worldArgument = browserArgs.get('world');
    const worldUrl = worldArgument === 'source'
      ? new URL('assets/demo-source.isoworld', document.baseURI).toString()
      : worldArgument
        ? new URL(worldArgument, document.baseURI).toString()
        : undefined;

    controls.bind();
    wheel.bind();
    pointer.bind();
    viewport.startObserving();

    this.module._isoweb_set_detailed_mode(detailedZoomMode ? 1 : 0);
    this.module._isoweb_set_detailed_yaw_mode(detailedYawMode ? 1 : 0);
    // Package loading replaces the runtime world projection. Build optional
    // demo obstacles only after that transaction has completed.
    this.module._isoweb_set_obstacles_enabled(0);
    viewport.syncRendererSize();
    controls.enableInitialState();
    const stats = statsEnabled ? new StatsOverlay(this.module) : null;

    window.addEventListener('keydown', event => {
      if (event.key === 'Escape') this.module._isoweb_clear_selection();
    });

    const stateLoader = new WorldStateLoader(this.module);
    void stateLoader.load(worldUrl)
      .then(() => {
        this.module._isoweb_set_obstacles_enabled(obstaclesEnabled ? 1 : 0);
        document.documentElement.classList.add('world-ready');
        this.module._isoweb_render();
      })
      .catch(error => console.error('[IsoWeb world package]', error));

    let previousTime = performance.now();
    const animate = (now: number): void => {
      const deltaSeconds = Math.min(0.10, Math.max(0, (now - previousTime) / 1000));
      previousTime = now;
      let activity = 'idle';
      if (this.module._isoweb_needs_tick()) {
        activity = 'simulation + dynamic render';
        this.module._isoweb_tick(deltaSeconds);
      } else if (this.module._isoweb_preview_needs_refinement()) {
        activity = 'progressive preview refinement';
        // Camera motion still gets the coarse fallback immediately and the idle
        // guard prevents refinement from competing with panning. Once idle,
        // refine a small batch in one pass so a full-resolution preview settles
        // quickly without causing eight separate full-frame redraws.
        this.module._isoweb_refine_preview(8);
      }
      stats?.frame(now, activity);
      requestAnimationFrame(animate);
    };
    requestAnimationFrame(animate);
  }
}
