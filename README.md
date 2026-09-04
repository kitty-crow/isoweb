# isoweb

A browser rendering experiment built as C++ compiled with Emscripten to WebAssembly.

The current scene is intentionally small: a cube, a sphere, a bounded ground plane and a background. The scene is rendered by a CPU ray tracer into a DFPSR RGBA image. Lighting and shadows are geometric, not painted overlays: every visible surface uses its real normal for Lambert shading and casts a shadow ray towards the point light to determine occlusion.

The camera has four discrete 90-degree viewpoints around the world Z axis. The scene itself never rotates. Camera panning stays entirely on the world X/Y ground plane and is clamped before the finite world can be panned completely out of view.

Visible camera controls are DFPSR-rendered sprites composited into the framebuffer. Transparent DOM buttons are used only as accessible pointer/touch hitboxes.

## Controls

- Left control cluster: quarter-turn camera rotation clockwise/counter-clockwise.
- Right control cluster: up/down/left/right panning.
- Mouse/trackpad wheel: vertical ground-plane panning.
- `Ctrl` + wheel: horizontal ground-plane panning.
- Touch: drag/swipe vertically or horizontally to pan on the X/Y ground plane.

## Dependencies

Both dependencies are vendored as pinned git submodules:

- `vendor/dfpsr` -> `Dawoodoz/DFPSR` at `66e9e9592752a338ae12d4e21d526d82b0f8579d`
- `vendor/pages` -> `kitty-crow/github-pages-template` at `426075b675d8ff79b8f97e351905b0358c612e05`

DFPSR stays headless in the browser. Its native window layer is replaced with its own `NoWindow` backend, while a tiny Emscripten bridge blits the completed RGBA buffer into an HTML canvas.

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
