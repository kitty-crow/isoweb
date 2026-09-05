import { App } from './App';
import { installPresenter } from './presentation/presenter';
import type { IsowebModule } from './runtime';

installPresenter();

globalThis.Module = {
  onRuntimeInitialized: () => {
    void new App(globalThis.Module as IsowebModule).start().catch(error => {
      console.error('Failed to start isoweb demo:', error);
      throw error;
    });
  }
} as IsowebModule;
