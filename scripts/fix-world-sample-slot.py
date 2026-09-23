from pathlib import Path
p = Path('src/engine/world/World.cpp')
text = p.read_text()
old = '''  const Vec3 runtime = sampleRuntimeEntities(\n    ray,\n    environmentColour,\n    environmentHitDistance,\n    runtimeFound\n  );'''
new = '''  const Vec3 runtime = sampleRuntimeEntities(\n    ray,\n    environmentColour,\n    environmentHitDistance,\n    runtimeFound,\n    0\n  );'''
if old not in text:
    raise SystemExit('reference sampler call site not found')
p.write_text(text.replace(old, new, 1))
print('Assigned reference World::sample runtime scratch slot 0.')
