# isoweb

Browser C++/WebAssembly rendering proof built on DFPSR.

## Development

The reusable engine lives under `src/engine/`; demo-world content lives under `src/demo/`; browser behaviour is TypeScript under `web/src/`.

## Generic objects and characters

`engine::Object` is the generic world-entity base type. It exposes identity, world/timeline/level location, continuous horizontal facing, hitbox geometry, solidity, collision tags, and unilateral `mustCollideWith` overrides. Object-vs-object overlap uses the facing vector so a hitbox can rotate with its object. `solid = false` disables ordinary collision symmetrically, while either object's must-collide selectors can force a collision regardless of solidity.

`engine::Character` extends `Object`. It adds independent `npc` and `controllable` properties, a movement-speed multiplier, current moving/action state, and optional sprite metadata. Characters do not require artwork. Sprite metadata supports still and moving directional sets plus optional action sets. Front, back, and left are the baseline stored directions; an explicit right asset is optional so symmetric characters may mirror left while asymmetric characters may provide their own right artwork.

`engine::WorldObject` remains only as a compatibility alias for the existing demo. New engine-facing code should use `engine::Object`. `IWorld::objects()` exposes the active level's generic objects and `IWorld::collidesWith()` provides the object-aware collision query intended for future moving characters. The older axis-aligned `intersectsSolid()` query remains for the current demo path.

The current demo geometry is still mapped to hitboxes around each rendered object's extents and all six demo objects are solid. Stairs and floors remain traversal surfaces rather than blocking world objects.

## Regression checks

After the static build, CI compiles and runs a generic object/character collision smoke test covering overlap, edge contact, rotated hitboxes, `solid = false`, must-collide overrides, character inheritance, optional sprite definitions, and active-level demo collision. After the WASM build, CI validates the generated Emscripten `ASM_CONSTS` bridge so a wrapper cannot reference undeclared `$N` arguments. CI then launches Chromium against the built site and verifies that WASM reaches its first rendered frame, clears the loading state, presents non-transparent canvas pixels, and updates camera-control state after a real rotation.

These checks run before the existing mobile audit and Pages deployment.
