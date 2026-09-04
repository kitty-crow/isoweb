# isoweb

A small browser rendering experiment built as C++ compiled with Emscripten to WebAssembly.

The current scene is intentionally tiny: a cube, a sphere, a bounded ground plane and a background. The scene is rendered by a CPU ray tracer into a DFPSR RGBA image. Lighting and shadows are geometric, not painted overlays: every visible surface uses its real normal for Lambert shading and casts a shadow ray towards the point light to determine occlusion.

## Dependencies

Both dependencies are vendored as pinned git submodules:

- `vendor/dfpsr` -> `Dawoodoz/DFPSR` at `66e9e9592752a338ae12d4e21d526d82b0f8579d`
- `vendor/pages` -> `kitty-crow/github-pages-template` at `426075b675d8ff79b8f97e351905b0358c612e05`

DFPSR stays headless in the browser. Its native window layer is replaced with its own `NoWindow` backend, while a tiny Emscripten bridge blits the completed RGBA buffer into an HTML canvas. This keeps browser presentation separate from rendering and leaves room to grow into DFPSR's isometric sprite/depth/lighting facilities later.

## Local build

Prerequisites: Bun and the Emscripten SDK (`em++` on PATH).

```sh
git submodule update --init --recursive
(cd vendor/pages && bun install)
bun run check
bun run build
bun run build:wasm
bun run audit:mobile
```

Serve `site/` from a local HTTP server to test the generated WebAssembly build.
