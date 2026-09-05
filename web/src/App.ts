import { getAppElements } from './dom/elements';
import type { IsowebModule } from './runtime';
import { ControlLayout } from './controls/ControlLayout';
import { ControlBindings } from './controls/ControlBindings';
import { PanQueue } from './input/PanQueue';
import { PointerController } from './input/PointerController';
import { WheelController } from './input/WheelController';
import { loadDemoState } from './state/DemoStateLoader';
import { ViewportController } from './viewport/ViewportController';

export class App {
  constructor(private readonly module: IsowebModule) {}

  async start(): Promise<void> {
    await loadDemoState(this.module);

    const elements = getAppElements();
    const layout = new ControlLayout(elements.viewport, elements.controls);
    const viewport = new ViewportController(elements.viewport, this.module, layout);
    const panQueue = new PanQueue(this.module);
    const controls = new ControlBindings(elements.controls, this.module, panQueue);
    const wheel = new WheelController(elements.viewport, this.module, viewport, panQueue);
    const pointer = new PointerController(elements.viewport, this.module, viewport, panQueue);
    const browserArgs = new URLSearchParams(window.location.search);
    const detailedZoomMode = browserArgs.get('dzoom') === '1';
    const detailedYawMode = browserArgs.get('dyaw') === '1';

    controls.bind();
    wheel.bind();
    pointer.bind();
    viewport.startObserving();
    window.addEventListener('keydown', event => {
      if (event.key === 'Escape') this.module._isoweb_clear_selection();
    });

    this.module._isoweb_set_detailed_mode(detailedZoomMode ? 1 : 0);
    this.module._isoweb_set_detailed_yaw_mode(detailedYawMode ? 1 : 0);
    viewport.syncRendererSize();
    controls.enableInitialState();

    let previousTime = performance.now();
    const tick = (time: number) => {
      const deltaSeconds = Math.max(0, (time - previousTime) / 1000);
      previousTime = time;
      this.module._isoweb_tick(deltaSeconds);
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  }
}
