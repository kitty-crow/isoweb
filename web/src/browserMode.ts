export type BrowserMode = {
  detailedZoom: boolean;
  detailedYaw: boolean;
  stats: boolean;
  threaded: boolean;
  webgl: boolean;
};

export type BrowserRuntimeMode = 'single-thread' | 'pthread';

export function parseBrowserMode(search: string | URLSearchParams): BrowserMode {
  const params = typeof search === 'string' ? new URLSearchParams(search) : search;
  return {
    detailedZoom: params.has('dzoom'),
    detailedYaw: params.has('dyaw'),
    stats: params.has('stats'),
    threaded: params.has('threaded'),
    webgl: params.has('webgl')
  };
}

export function browserMode(): BrowserMode {
  const existing = globalThis.isowebBrowserMode;
  if (existing) return existing;
  const parsed = parseBrowserMode(globalThis.location?.search ?? '');
  globalThis.isowebBrowserMode = parsed;
  return parsed;
}

declare global {
  var isowebBrowserMode: BrowserMode | undefined;
  var isowebRuntimeMode: BrowserRuntimeMode | undefined;
  var isowebRuntimeFallbackReason: string | undefined;
}
