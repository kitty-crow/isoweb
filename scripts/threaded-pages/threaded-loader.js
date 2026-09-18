(async () => {
  const loading = document.getElementById('loading');
  const fail = (message) => {
    if (loading) loading.textContent = message;
    console.error(message);
  };

  if (!('serviceWorker' in navigator)) {
    fail('This browser does not support the isolation worker required for multithreaded WASM.');
    return;
  }

  if (!crossOriginIsolated) {
    try {
      await navigator.serviceWorker.register('./coi-serviceworker.js', { scope: './' });
      await navigator.serviceWorker.ready;

      const reloadKey = 'isoweb-threaded-coi-reload';
      if (sessionStorage.getItem(reloadKey) === '1') {
        fail('Could not enable cross-origin isolation for the multithreaded test.');
        return;
      }
      sessionStorage.setItem(reloadKey, '1');
      location.reload();
      return;
    } catch (error) {
      fail('Could not install the multithreaded WASM isolation worker.');
      console.error(error);
      return;
    }
  }

  sessionStorage.removeItem('isoweb-threaded-coi-reload');

  const loadScript = (src) => new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = src;
    script.onload = resolve;
    script.onerror = () => reject(new Error(`Failed to load ${src}`));
    document.body.appendChild(script);
  });

  try {
    await loadScript('./assets/bootstrap.js');
    await loadScript('./assets/isoweb.js');
  } catch (error) {
    fail('Could not start the multithreaded WASM build.');
    console.error(error);
  }
})();
