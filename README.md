# isoweb

Browser C++/WebAssembly rendering proof built on DFPSR.

## Development

The reusable engine lives under `src/engine/`; demo-world content lives under `src/demo/`; browser behaviour is TypeScript under `web/src/`.

## Generic objects and characters

`engine::Object` is the generic world-entity base type. It exposes identity, world/timeline/level location, continuous horizontal facing, hitbox geometry, solidity, collision tags, and unilateral `mustCollideWith` overrides. Object-vs-object overlap uses the facing vector so a hitbox can rotate with its object. `solid = false` disables ordinary collision symmetrically, while either object's must-collide selectors can force a collision regardless of solidity.

`engine::Character` extends `Object`. It adds independent `npc` and `controllable` properties, a movement-speed multiplier, current moving/action state, and optional sprite metadata. Characters do not require artwork. Sprite metadata supports still and moving directional sets plus optional action sets. Front, back, and left are the baseline stored directions; an explicit right asset is optional so symmetric characters may mirror left while asymmetric characters may provide their own right artwork.

`engine::WorldObject` remains only as a compatibility alias for the existing demo. New engine-facing code should use `engine::Object`. `IWorld::objects()` exposes the active level's generic objects and `IWorld::collidesWith()` provides the object-aware collision query intended for future moving characters. The older axis-aligned `intersectsSolid()` query remains for the current demo path.

The current demo geometry is still mapped to hitboxes around each rendered object's extents and all six demo objects are solid. Stairs and floors remain traversal surfaces rather than blocking world objects.

## Browser modes

IsoWeb uses one canonical application path. Optional browser modes are selected by presence-only query flags and compose freely.

| Flag | Meaning |
|---|---|
| `?dzoom` | detailed zoom increments |
| `?dyaw` | detailed rotation/yaw increments |
| `?stats` | diagnostics overlay |
| `?threaded` | request the pthread/multi-core CPU runtime |
| `?webgl` | request the WebGL2 GPU static renderer |

Examples include `?threaded&webgl`, `?threaded&webgl&stats`, and `?dzoom&dyaw&webgl`. Parameter ordering does not matter. Presence means enabled, so legacy forms such as `?dzoom=1` and `?dyaw=1` still work naturally.

The four renderer/runtime combinations are:

| Query | WASM runtime | Static renderer preference |
|---|---|---|
| no renderer flag | single-thread | single-thread CPU |
| `?threaded` | pthread | multi-thread CPU |
| `?webgl` | single-thread | WebGL2 GPU, then single-thread CPU |
| `?threaded&webgl` | pthread | WebGL2 GPU, then multi-thread CPU, then single-thread CPU if pthread setup cannot be established |

`threaded` controls CPU threading only. WebGL does not expose individual GPU cores or portable multi-GPU dispatch, so `?threaded&webgl` means GPU shader parallelism plus the pthread CPU runtime, not selectable GPU core counts.

The root bootstrap establishes cross-origin isolation only when `?threaded` is requested and selects the appropriate Emscripten artefact before WASM initialisation. The generated single-thread and pthread artefacts remain separate internally. Legacy `/threaded/` and `/webgl/` paths are compatibility redirects to `/?threaded` and `/?webgl`; renderer policy itself is no longer pathname-derived.

WebGL framebuffer presentation remains independent from WebGL static rendering. Development/regression tests may use internal overrides such as `?presentation=canvas2d` or `?gpuStatic=0|1`; these are not the public renderer-selection interface.

## Runtime stats

Add `?stats` to any mode combination to show the live rendering diagnostics overlay. The overlay reports both the requested query mode and the runtime/backend actually in use, which makes degraded fallback visible rather than implicit.

It reports render/display FPS, presentation and GPU timings where available, current activity, requested mode, actual WASM runtime, active static-render backend, logical CPU count and actual IsoWeb render-thread count, JS/WASM memory, tracked IsoWeb WebGL allocations, GPU identity, and progressive-preview work. Browsers do not expose OS core IDs, per-core utilisation, or total process VRAM, so those values are explicitly marked as unavailable rather than inferred.

## Regression checks

After the static build, CI compiles and runs a generic object/character collision smoke test covering overlap, edge contact, rotated hitboxes, `solid = false`, must-collide overrides, character inheritance, optional sprite definitions, and active-level demo collision. After the WASM build, CI validates the generated Emscripten `ASM_CONSTS` bridge so a wrapper cannot reference undeclared `$N` arguments. CI then launches Chromium against the built site and verifies that WASM reaches its first rendered frame, clears the loading state, presents non-transparent canvas pixels, and updates camera-control state after a real rotation.

These checks run before the existing mobile audit and Pages deployment.
