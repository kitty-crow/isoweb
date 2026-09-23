from pathlib import Path

path = Path('scripts/joystick-browser-smoke.ts')
text = path.read_text()

text = text.replace(
    '    let retained: Uint8Array;\n    let full: Uint8Array;',
    '    let retained: Uint8Array;\n    let shiftedFull: Uint8Array;\n    let full: Uint8Array;'
)

needle = """      retained = captured;
      captured = null;

      // Invalidate the static cache and redraw the exact same state from scratch.
      module._isoweb_set_render_thread_limit(1);"""
replacement = """      retained = captured;
      captured = null;

      // Force full scene recomposition without discarding the shifted static cache.
      module._isoweb_resize(canvas.width, canvas.height);
      module._isoweb_render();
      if (!captured) throw new Error('Shifted-static full renderer frame was not captured.');
      shiftedFull = captured;
      captured = null;

      // Then invalidate the static cache and redraw the exact same state from scratch.
      module._isoweb_set_render_thread_limit(1);"""
if needle not in text:
    raise SystemExit('pan parity capture needle not found')
text = text.replace(needle, replacement)

old = """    let mismatches = 0;
    let firstMismatch = -1;
    let maximumDelta = 0;
    for (let index = 0; index < retained.length; ++index) {
      const delta = Math.abs(retained[index] - full[index]);
      if (delta === 0) continue;
      ++mismatches;
      if (firstMismatch < 0) firstMismatch = index;
      maximumDelta = Math.max(maximumDelta, delta);
    }
    return { retainedCount, mismatches, firstMismatch, maximumDelta };"""
new = """    const compare = (a: Uint8Array, b: Uint8Array) => {
      let mismatches = 0;
      let mismatchPixels = 0;
      let firstMismatch = -1;
      let maximumDelta = 0;
      let minX = canvas.width;
      let minY = canvas.height;
      let maxX = -1;
      let maxY = -1;
      const byChannel = [0, 0, 0, 0];
      const seen = new Set<number>();
      const samples: any[] = [];
      for (let index = 0; index < a.length; ++index) {
        const delta = Math.abs(a[index] - b[index]);
        if (delta === 0) continue;
        ++mismatches;
        if (firstMismatch < 0) firstMismatch = index;
        maximumDelta = Math.max(maximumDelta, delta);
        const pixel = Math.floor(index / 4);
        const channel = index & 3;
        const x = pixel % canvas.width;
        const y = Math.floor(pixel / canvas.width);
        byChannel[channel]++;
        if (!seen.has(pixel)) {
          seen.add(pixel);
          ++mismatchPixels;
          minX = Math.min(minX, x);
          minY = Math.min(minY, y);
          maxX = Math.max(maxX, x);
          maxY = Math.max(maxY, y);
        }
        if (samples.length < 12) samples.push({ x, y, channel, a: a[index], b: b[index], delta });
      }
      return { mismatches, mismatchPixels, firstMismatch, maximumDelta, minX, minY, maxX, maxY, byChannel, samples };
    };
    const retainedVsShifted = compare(retained, shiftedFull);
    const shiftedVsFresh = compare(shiftedFull, full);
    console.log('[pan-diagnostic] retained-vs-shifted-static-full', JSON.stringify(retainedVsShifted));
    console.log('[pan-diagnostic] shifted-static-full-vs-fresh-static-full', JSON.stringify(shiftedVsFresh));
    return {
      retainedCount,
      mismatches: retainedVsShifted.mismatches,
      firstMismatch: retainedVsShifted.firstMismatch,
      maximumDelta: retainedVsShifted.maximumDelta,
      retainedVsShifted,
      shiftedVsFresh
    };"""
if old not in text:
    raise SystemExit('pan parity compare needle not found')

path.write_text(text.replace(old, new))
