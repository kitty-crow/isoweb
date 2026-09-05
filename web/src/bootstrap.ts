import { App } from './App';
import { installPresenter } from './presentation/presenter';
import type { IsowebModule } from './runtime';

installPresenter();

globalThis.Module = {
  onRuntimeInitialized: () => {
    new App(globalThis.Module as IsowebModule).start();
  }
} as IsowebModule;
