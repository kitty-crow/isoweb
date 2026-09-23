from pathlib import Path

# Reuse the reviewed guarded damage transform, correcting only the renderer's
# long-standing 160..1600 resize clamp in the transform source before execution.
staging_path = Path('scripts/apply-damage-rendering.py')
staging = staging_path.read_text()
if 'std::max(1, width)' not in staging or 'std::max(1, height)' not in staging:
    raise SystemExit('damage staging transform no longer has the expected resize guard')
staging = staging.replace(
    'std::max(1, width)',
    'std::max(160, std::min(1600, width))',
    1
).replace(
    'std::max(1, height)',
    'std::max(160, std::min(1600, height))',
    1
)
exec(compile(staging, str(staging_path), 'exec'), {'__name__': '__main__'})

# Strengthen the pthread browser smoke so damage rendering is proven against a
# forced full redraw from the exact same world/camera state.
p = Path('scripts/pthread-browser-smoke.ts')
text = p.read_text()
old = '''    let mismatchCount = 0;\n    let maxChannelDelta = 0;\n    let firstMismatch = -1;\n    for (let index = 0; index < reference.length; ++index) {\n      const delta = Math.abs(reference[index] - parallel[index]);\n      if (delta !== 0) {\n        ++mismatchCount;\n        if (firstMismatch < 0) firstMismatch = index;\n        if (delta > maxChannelDelta) maxChannelDelta = delta;\n      }\n    }\n\n    return {'''
new = '''    let mismatchCount = 0;\n    let maxChannelDelta = 0;\n    let firstMismatch = -1;\n    for (let index = 0; index < reference.length; ++index) {\n      const delta = Math.abs(reference[index] - parallel[index]);\n      if (delta !== 0) {\n        ++mismatchCount;\n        if (firstMismatch < 0) firstMismatch = index;\n        if (delta > maxChannelDelta) maxChannelDelta = delta;\n      }\n    }\n\n    // Move the dynamic Character without changing the static camera/cache. This\n    // render must take the retained-frame damage path. Then invalidate the\n    // static cache via the existing thread-limit setter and render the exact\n    // same state in full. Every output byte must match.\n    const moved = module.ccall(\n      'isoweb_set_character_location',\n      'number',\n      ['string', 'string', 'string', 'string', 'number', 'number', 'number'],\n      ['pthread-render-probe', 'demo', 'default', 'lower', 0.42, 0.18, 0.0]\n    );\n    if (moved !== 1) throw new Error('Could not move pthread render probe Character.');\n    module._isoweb_render();\n    const incremental = new Uint8ClampedArray(\n      context.getImageData(0, 0, canvas.width, canvas.height).data\n    );\n\n    // setRenderThreadLimit intentionally invalidates the static cache even when\n    // the numerical limit is unchanged, giving this test a same-state full redraw.\n    module._isoweb_set_render_thread_limit(4);\n    module._isoweb_render();\n    const forcedFull = context.getImageData(0, 0, canvas.width, canvas.height).data;\n    let damageMismatchCount = 0;\n    let damageMaxChannelDelta = 0;\n    let damageFirstMismatch = -1;\n    for (let index = 0; index < incremental.length; ++index) {\n      const delta = Math.abs(incremental[index] - forcedFull[index]);\n      if (delta !== 0) {\n        ++damageMismatchCount;\n        if (damageFirstMismatch < 0) damageFirstMismatch = index;\n        if (delta > damageMaxChannelDelta) damageMaxChannelDelta = delta;\n      }\n    }\n\n    return {'''
if old not in text:
    raise SystemExit('pthread parity insertion anchor not found')
text = text.replace(old, new, 1)
old_return = '''      mismatchCount,\n      maxChannelDelta,\n      firstMismatch,\n      hardwareConcurrency: navigator.hardwareConcurrency'''
new_return = '''      mismatchCount,\n      maxChannelDelta,\n      firstMismatch,\n      damageMismatchCount,\n      damageMaxChannelDelta,\n      damageFirstMismatch,\n      hardwareConcurrency: navigator.hardwareConcurrency'''
if old_return not in text:
    raise SystemExit('pthread parity return anchor not found')
text = text.replace(old_return, new_return, 1)
old_assert = '''  if (result.mismatchCount !== 0) {\n    throw new Error(`Parallel render differs from same-binary single-thread reference: ${JSON.stringify(result)}`);\n  }\n  if (errors.length) throw new Error(errors.join('\\n\\n'));'''
new_assert = '''  if (result.mismatchCount !== 0) {\n    throw new Error(`Parallel render differs from same-binary single-thread reference: ${JSON.stringify(result)}`);\n  }\n  if (result.damageMismatchCount !== 0) {\n    throw new Error(`Incremental damage render differs from forced full redraw: ${JSON.stringify(result)}`);\n  }\n  if (errors.length) throw new Error(errors.join('\\n\\n'));'''
if old_assert not in text:
    raise SystemExit('pthread parity assertion anchor not found')
text = text.replace(old_assert, new_assert, 1)
p.write_text(text)

print('Applied consolidated exact damage rendering and parity test.')
