# isoweb

Experimental CPU-rendered C++/WebAssembly isometric rendering foundation built around DFPSR.

## Architecture

The repository deliberately separates the reusable engine from the proof-of-concept world:

- `src/engine/` contains vendorable engine concepts such as maths, camera state, framebuffer rendering, control sprites, world interfaces, and browser presentation.
- `src/demo/` contains the current cube/sphere proof world and its application wiring. Demo geometry, colours, lighting, and bounds do not live in the engine.
- `src/wasm/` is the narrow WebAssembly export boundary used by the browser demo.
- `web/src/` contains TypeScript browser behaviour. `web/index.html` is markup only.
- `web/styles.css` is only the stylesheet index; functional CSS lives under `web/styles/`.

The browser TypeScript is compiled during `bun run build`, and the C++ sources are compiled to WebAssembly by `bun run build:wasm`.
