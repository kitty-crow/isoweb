# isoweb

Browser C++/WebAssembly rendering proof built on DFPSR.

## Development

The reusable engine lives under `src/engine/`; demo-world content lives under `src/demo/`; browser behaviour is TypeScript under `web/src/`.

## Regression checks

After the WASM build, CI validates the generated Emscripten `ASM_CONSTS` bridge so a wrapper cannot reference undeclared `$N` arguments. CI then launches Chromium against the built site and verifies that WASM reaches its first rendered frame, clears the loading state, presents non-transparent canvas pixels, and updates camera-control state after a real rotation.

These checks run before the existing mobile audit and Pages deployment.
