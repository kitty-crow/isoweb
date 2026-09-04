# isoweb

Browser-side C++/WebAssembly isometric rendering experiment built on DFPSR.

The renderer is deliberately graphics-only. Camera state, projection, lighting, shadows, render objects, Z-level views and screen-space controls live in C++/WASM; the browser supplies the canvas presentation layer, viewport sizing, pointer/wheel input and transparent accessibility hitboxes.

## Demo camera controls

- Eight discrete yaw positions around the world Z axis, 45° apart.
- X/Y ground-plane panning with bounded world edges.
- Regular zoom presets at 0.5×, 1× and 2×. Detailed mode adds 0.25×, 4× and a fit-whole-world view.
- Dynamic Z-level selection. Z levels are alternate render views of the same world, not physical camera height. Lower levels can be ghosted translucently beneath the selected level; upper levels are never shown until selected.
- Reset controls affect only their own camera component.
- Disabled controls are rendered dimmed when that action would have no effect.

Desktop wheel mappings:

- wheel: X-direction pan
- Ctrl + wheel: Y-direction pan
- Alt + wheel: zoom
- Shift + wheel: Z-yaw, down clockwise and up counter-clockwise

Touch mappings:

- one-finger drag: X/Y pan
- two-finger pinch: zoom
- two-finger twist: Z-yaw

## Demo Z levels

The current demo supplies three `LevelView` entries only to exercise the API:

- lower: purple cone and yellow pyramid, darker floor, its own point-light position
- default: blue cube and orange sphere, current floor colour, its own point-light position
- upper: blue dodecahedron and red icosahedron, lighter floor, its own point-light position

The engine does not assume three levels. The level stack is a dynamic container and can contain any number of views.

## Rendering

The scene is CPU-rendered by DFPSR and compiled with Emscripten. Primary rays intersect the active level geometry, surface normals drive Lambert lighting, and secondary rays test real geometry for point-light shadow occlusion. Lower-level ghost views are rendered independently using each lower level's own geometry and light before translucent compositing.

DFPSR runs headlessly with its `NoWindow` and `NoSound` backends. The browser receives the finished RGBA framebuffer and blits it to the canvas.

## Build

```sh
git submodule update --init --recursive
(cd vendor/pages && bun install)
bun run check
bun run build
bun run build:wasm
bun run audit:mobile
```

Detailed zoom mode can currently be exercised with `?detailed=1`.
