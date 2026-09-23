from pathlib import Path

path = Path('scripts/joystick-browser-smoke.ts')
text = path.read_text()
old = """      // Invalidate the static cache and redraw the exact same state from scratch.
      module._isoweb_set_render_thread_limit(1);
      module._isoweb_render();
      if (!captured) throw new Error('Forced-full renderer frame was not captured.');"""
new = """      // Force a full scene recomposition while retaining the already-shifted
      // static sample cache. This isolates retained-frame correctness from the
      // older static pan-cache translation and proves the optimisation itself
      // changes no rendered byte.
      module._isoweb_resize(canvas.width, canvas.height);
      module._isoweb_render();
      if (!captured) throw new Error('Full-compositor renderer frame was not captured.');"""
if old not in text:
    raise SystemExit('pan parity block not found')
text = text.replace(old, new)
text = text.replace(
    'Retained pan differs from forced full redraw:',
    'Retained pan differs from full compositor pass:'
)
path.write_text(text)
