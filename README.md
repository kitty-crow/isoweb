# isoweb

Browser C++/WebAssembly rendering proof built on DFPSR.

## Development

The reusable engine lives under `src/engine/`; demo-world content lives under `src/demo/`; browser behaviour is TypeScript under `web/src/`.

## World objects and collision

`engine::WorldObject` carries a `solid` flag and an axis-aligned `HitBox`. A solid object blocks another hitbox only when their volumes overlap; touching faces or edges is allowed. `IWorld::objects()` exposes the active level's collision objects and `IWorld::intersectsSolid()` provides the direct query future moving sprites or other world objects can use before accepting a candidate position.

The current demo geometry is mapped to hitboxes around each rendered object's extents and all six demo objects are solid. Stairs and floors remain traversal surfaces rather than blocking world objects.

## Regression checks

After the static build, CI compiles and runs a world-object hitbox smoke test covering overlap, edge contact and `solid = false`. After the WASM build, CI validates the generated Emscripten `ASM_CONSTS` bridge so a wrapper cannot reference undeclared `$N` arguments. CI then launches Chromium against the built site and verifies that WASM reaches its first rendered frame, clears the loading state, presents non-transparent canvas pixels, and updates camera-control state after a real rotation.

These checks run before the existing mobile audit and Pages deployment.