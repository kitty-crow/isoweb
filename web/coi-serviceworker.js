self.addEventListener('install', () => self.skipWaiting());

self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', event => {
  const request = event.request;
  if (request.cache === 'only-if-cached' && request.mode !== 'same-origin') return;

  event.respondWith((async () => {
    const response = await fetch(request);
    if (response.type === 'opaque') return response;

    // Only a navigation that explicitly requests ?threaded becomes an
    // isolated document. Once such a document is active, its same-origin
    // pthread/WASM subresources still need isolation-compatible response
    // headers, so non-navigation responses remain decorated.
    const isolateResponse =
      request.mode !== 'navigate' ||
      new URL(request.url).searchParams.has('threaded');
    if (!isolateResponse) return response;

    const headers = new Headers(response.headers);
    headers.set('Cross-Origin-Opener-Policy', 'same-origin');
    headers.set('Cross-Origin-Embedder-Policy', 'require-corp');

    return new Response(response.body, {
      status: response.status,
      statusText: response.statusText,
      headers
    });
  })());
});
