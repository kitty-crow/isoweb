(async () => {
  const loading = document.getElementById('loading');
  const setStatus = (message) => {
    if (loading) loading.textContent = message;
  };
  const logFallback = (message, error) => {
    console.warn(message, error ?? '');
  };

  const loadScript = (src) => new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = src;
    script.onload = resolve;
    script.onerror = () => reject(new Error(`Failed to load ${src}`));
    document.body.appendChild(script);
  });

  const start = async (wasmScript, mode) => {
    window.isowebAdaptiveContainer = mode;
    await loadScript('./assets/bootstrap.js');
    await loadScript(wasmScript);
  };

  let useThreaded = false;
  if ('serviceWorker' in navigator) {
    if (crossOriginIsolated && typeof SharedArrayBuffer === 'function') {
      useThreaded = true;
    } else {
      try {
        setStatus('Preparing adaptive renderer…');
        await navigator.serviceWorker.register('./coi-serviceworker.js', { scope: './' });
        await navigator.serviceWorker.ready;

        const reloadKey = 'isoweb-adaptive-coi-reload';
        if (sessionStorage.getItem(reloadKey) !== '1') {
          sessionStorage.setItem(reloadKey, '1');
          location.reload();
          return;
        }
        sessionStorage.removeItem(reloadKey);
        useThreaded = crossOriginIsolated && typeof SharedArrayBuffer === 'function';
      } catch (error) {
        sessionStorage.removeItem('isoweb-adaptive-coi-reload');
        logFallback('Could not enable pthread isolation; using single-thread fallback.', error);
      }
    }
  }

  try {
    if (useThreaded) {
      sessionStorage.removeItem('isoweb-adaptive-coi-reload');
      await start('./assets/isoweb.js', 'pthread');
    } else {
      setStatus('Starting compatibility renderer…');
      await start('./single/isoweb.js', 'single-thread');
    }
  } catch (error) {
    setStatus('Could not start the adaptive renderer.');
    console.error(error);
  }
})();
