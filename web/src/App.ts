import { getAppElements } from './dom/elements';
import type { IsowebModule } from './runtime';
import { ControlLayout } from './controls/ControlLayout';
import { ControlBindings } from './controls/ControlBindings';
import { PanQueue } from './input/PanQueue';
import { PointerController } from './input/PointerController';
import { WheelController } from './input/WheelController';
import { ViewportController } from './viewport/ViewportController';

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
    const detailedZoomMode = new URLSearchParams(window.location.search).get('detailed') === '1';

    controls.bind();
    wheel.bind();
    pointer.bind();
    viewport.startObserving();

    this.module._isoweb_set_detailed_mode(detailedZoomMode ? 1 : 0);
    viewport.syncRendererSize();
    controls.enableInitialState();
  }
}
