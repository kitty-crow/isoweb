import { parseBrowserMode, type BrowserRuntimeMode } from './browserMode';

const mode = parseBrowserMode(location.search);
globalThis.isowebBrowserMode = mode;

const loading = document.getElementById('loading');
const setStatus = (message: string): void => {
  if (loading) loading.textContent = message;
};

function loadScript(src: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = new URL(src, document.baseURI).toString();
    script.onload = () => resolve();
    script.onerror = () => reject(new Error(`Failed to load ${src}`));
    document.body.appendChild(script);
  });
}

async function ensureThreadIsolation(): Promise<boolean> {
  if (
    crossOriginIsolated &&
    typeof SharedArrayBuffer === 'function'
  ) {
    return true;
  }

  if (!navigator.serviceWorker) {
    globalThis.isowebRuntimeFallbackReason =
      'Service workers are unavailable, so pthread isolation cannot be established.';
    return false;
  }

  const reloadKey = 'isoweb-root-threaded-coi-reload';
  try {
    setStatus('Preparing multi-core runtime…');
    await navigator.serviceWorker.register(
      new URL('coi-serviceworker.js', document.baseURI).toString(),
      { scope: new URL('./', document.baseURI).pathname }
    );
    await navigator.serviceWorker.ready;

    if (sessionStorage.getItem(reloadKey) !== '1') {
      sessionStorage.setItem(reloadKey, '1');
      location.reload();
      return new Promise<boolean>(() => {});
    }

    sessionStorage.removeItem(reloadKey);
    if (
      crossOriginIsolated &&
      typeof SharedArrayBuffer === 'function'
    ) {
      return true;
    }

    globalThis.isowebRuntimeFallbackReason =
      'Cross-origin isolation did not become available after the pthread bootstrap reload.';
    return false;
  } catch (error) {
    sessionStorage.removeItem(reloadKey);
    globalThis.isowebRuntimeFallbackReason =
      error instanceof Error ? error.message : String(error);
    console.warn(
      'Could not establish pthread isolation; using the single-thread runtime.',
      error
    );
    return false;
  }
}

async function boot(): Promise<void> {
  let runtime: BrowserRuntimeMode = 'single-thread';
  if (mode.threaded && await ensureThreadIsolation()) runtime = 'pthread';

  globalThis.isowebRuntimeMode = runtime;
  if (mode.threaded && runtime !== 'pthread' && !globalThis.isowebRuntimeFallbackReason) {
    globalThis.isowebRuntimeFallbackReason =
      'The requested pthread runtime is unavailable; using single-thread fallback.';
  }

  await loadScript('assets/bootstrap.js');
  await loadScript(runtime === 'pthread'
    ? 'assets/threaded/isoweb.js'
    : 'assets/isoweb.js');
}

void boot().catch(error => {
  setStatus('Could not start IsoWeb.');
  console.error(error);
});
