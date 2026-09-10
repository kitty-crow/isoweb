from pathlib import Path

# 1) Camera panning: clamp the camera focus to the real planar level extent,
# not to an inset extent that subtracts half the viewport.
path = Path('src/engine/camera/Camera.cpp')
text = path.read_text()
old = '''  float minimumRight = 1e9f;
  float maximumRight = -1e9f;
  float minimumUp = 1e9f;
  float maximumUp = -1e9f;
  for (const Vec3& point : bounds.points) {
    const Vec3 relative = point - bounds.focus;
    const float projectedRight = dot(relative, right);
    const float projectedUp = dot(relative, up);
    minimumRight = std::min(minimumRight, projectedRight);
    maximumRight = std::max(maximumRight, projectedRight);
    minimumUp = std::min(minimumUp, projectedUp);
    maximumUp = std::max(maximumUp, projectedUp);
  }

  const float halfWidth = viewHeight * aspect * 0.5f;
  const float halfHeight = viewHeight * 0.5f;
  const auto centreRange = [](float minimum, float maximum, float halfViewport) {
    const float span = maximum - minimum;
    if (span > halfViewport * 2.0f) {
      return std::array<float, 2>{{minimum + halfViewport, maximum - halfViewport}};
    }
    return std::array<float, 2>{{maximum - halfViewport, minimum + halfViewport}};
  };

  const std::array<float, 2> rightRange = centreRange(minimumRight, maximumRight, halfWidth);
  const std::array<float, 2> upRange = centreRange(minimumUp, maximumUp, halfHeight);
  const float candidateRight = dot(candidate, right);
  const float candidateUp = dot(candidate, up);
  const float clampedRight = clampPan(candidateRight, rightRange[0], rightRange[1]);
  const float clampedUp = clampPan(candidateUp, upRange[0], upRange[1]);
'''
new = '''  float minimumRight = 1e9f;
  float maximumRight = -1e9f;
  float minimumUp = 1e9f;
  float maximumUp = -1e9f;
  for (const Vec3& point : bounds.points) {
    // Panning is about traversing the floor plan. Height must not shorten or
    // enlarge the legal XY camera range, so project the planar footprint only.
    const Vec3 relative(
      point.x - bounds.focus.x,
      point.y - bounds.focus.y,
      0.0f
    );
    const float projectedRight = dot(relative, right);
    const float projectedUp = dot(relative, up);
    minimumRight = std::min(minimumRight, projectedRight);
    maximumRight = std::max(maximumRight, projectedRight);
    minimumUp = std::min(minimumUp, projectedUp);
    maximumUp = std::max(maximumUp, projectedUp);
  }

  // The old bounds-aware clamp still inset these extrema by half a viewport.
  // On a portrait phone that made a five-room level stop around the centre
  // room. Let the camera focus itself reach the authored level edge instead.
  // This keeps the whole active floor reachable at every aspect ratio while
  // still preventing unbounded panning into empty space.
  const float candidateRight = dot(candidate, right);
  const float candidateUp = dot(candidate, up);
  const float clampedRight = clampPan(candidateRight, minimumRight, maximumRight);
  const float clampedUp = clampPan(candidateUp, minimumUp, maximumUp);
'''
if old not in text:
    raise SystemExit('Camera clamp block not found')
text = text.replace(old, new, 1)
path.write_text(text)

# 2) Strengthen the camera regression to use a portrait viewport and the real
# five-room Z-shaped middle-floor footprint.
path = Path('scripts/camera-pan-bounds-smoke.cpp')
path.write_text(r'''#include <cmath>
#include <cstdlib>
#include <iostream>

#include "engine/camera/Camera.hpp"

using namespace isoweb::engine;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "[camera-pan-bounds] " << message << '\n';
    std::exit(1);
  }
}

void addRoomBounds(WorldBounds& bounds, float centreX, float centreY) {
  constexpr float halfRoom = 4.40f;
  for (float x : {centreX - halfRoom, centreX + halfRoom}) {
    for (float y : {centreY - halfRoom, centreY + halfRoom}) {
      bounds.points.push_back({x, y, 0.0f});
      bounds.points.push_back({x, y, 1.80f});
    }
  }
}

WorldBounds makeMiddleFloorBounds() {
  constexpr float roomSize = 8.80f;
  WorldBounds bounds;
  bounds.focus = {0.0f, 0.15f, 0.55f};
  addRoomBounds(bounds, 0.0f, 0.0f);
  addRoomBounds(bounds, 0.0f, roomSize);
  addRoomBounds(bounds, 0.0f, -roomSize);
  addRoomBounds(bounds, -roomSize, roomSize);
  addRoomBounds(bounds, roomSize, -roomSize);
  return bounds;
}

void planarUpExtents(
  const WorldBounds& bounds,
  const Vec3& up,
  float& minimum,
  float& maximum
) {
  minimum = 1e9f;
  maximum = -1e9f;
  for (const Vec3& point : bounds.points) {
    const Vec3 relative(point.x - bounds.focus.x, point.y - bounds.focus.y, 0.0f);
    const float projected = dot(relative, up);
    minimum = std::min(minimum, projected);
    maximum = std::max(maximum, projected);
  }
}

} // namespace

int main() {
  Camera camera(CameraConfig(3.25f, 6.15f, 5.50f));
  const WorldBounds bounds = makeMiddleFloorBounds();

  // Reproduce the phone shape where the previous half-viewport inset was most
  // severe. At 1x it stopped around the centre room despite the Z-shaped floor.
  const int width = 390;
  const int height = 844;
  require(camera.canPan(width, height, bounds), "middle floor did not enable phone panning");

  const Vec3 right = camera.groundRight();
  const Vec3 up = normalise(cross(right, camera.forward()));
  float minimumUp = 0.0f;
  float maximumUp = 0.0f;
  planarUpExtents(bounds, up, minimumUp, maximumUp);

  // Positive screen-down movement projects toward minimumUp for the default
  // camera basis. The camera focus must be able to reach that real floor edge,
  // rather than stopping half a viewport before it.
  for (int i = 0; i < 240; ++i) camera.pan(0.0f, 1.0f, width, height, bounds);
  const Vec3 downPan(camera.panX(), camera.panY(), 0.0f);
  const float downProjected = dot(downPan, up);
  require(
    std::fabs(downProjected - minimumUp) < 0.05f,
    "portrait joystick pan stopped before the real lower projected floor edge"
  );
  require(
    std::sqrt(camera.panX() * camera.panX() + camera.panY() * camera.panY()) > 8.0f,
    "portrait joystick pan still stops around the centre room"
  );
  auto state = camera.controlState(width, height, bounds);
  require(!state.canPanDown, "pan-down stayed enabled after reaching the real floor edge");
  require(state.canPanUp, "pan-up disabled before returning across the floor");

  camera.resetPan();
  for (int i = 0; i < 240; ++i) camera.pan(0.0f, -1.0f, width, height, bounds);
  const Vec3 upPan(camera.panX(), camera.panY(), 0.0f);
  const float upProjected = dot(upPan, up);
  require(
    std::fabs(upProjected - maximumUp) < 0.05f,
    "portrait joystick pan stopped before the real upper projected floor edge"
  );

  camera.resetPan();
  for (int i = 0; i < 240; ++i) camera.pan(1.0f, 0.0f, width, height, bounds);
  const Vec3 rightPan(camera.panX(), camera.panY(), 0.0f);
  float maximumRight = -1e9f;
  for (const Vec3& point : bounds.points) {
    const Vec3 relative(point.x - bounds.focus.x, point.y - bounds.focus.y, 0.0f);
    maximumRight = std::max(maximumRight, dot(relative, right));
  }
  require(
    std::fabs(dot(rightPan, right) - maximumRight) < 0.05f,
    "portrait joystick pan stopped before the real right floor edge"
  );

  std::cout << "Camera pan reaches the full active-floor footprint on a portrait viewport.\n";
  return 0;
}
''')

# 3) Explicitly suppress browser-native double-tap/gesture handling on the
# interactive viewport while retaining our own pointer-based canvas gestures.
path = Path('web/src/input/PointerController.ts')
text = path.read_text()
old = '''  bind(): void {
    this.viewport.addEventListener('pointerdown', event => this.onPointerDown(event));
    this.viewport.addEventListener('pointermove', event => this.onPointerMove(event));
    this.viewport.addEventListener('pointerup', event => this.endPointer(event, false));
    this.viewport.addEventListener('pointercancel', event => this.endPointer(event, true));
  }
'''
new = '''  bind(): void {
    this.viewport.addEventListener('pointerdown', event => this.onPointerDown(event));
    this.viewport.addEventListener('pointermove', event => this.onPointerMove(event));
    this.viewport.addEventListener('pointerup', event => this.endPointer(event, false));
    this.viewport.addEventListener('pointercancel', event => this.endPointer(event, true));

    // Safari can still recognise browser-level gestures even with a locked
    // viewport meta tag. The game owns every gesture in this viewport, so
    // explicitly cancel native double-tap/gesture defaults as a second line
    // of defence. Scene touchend has no browser click semantic to preserve.
    this.viewport.addEventListener('dblclick', this.preventBrowserGesture);
    this.viewport.addEventListener('touchend', this.preventSceneTouchEnd, { passive: false });
    for (const name of ['gesturestart', 'gesturechange', 'gestureend']) {
      this.viewport.addEventListener(name, this.preventBrowserGesture, { passive: false });
    }
  }

  private readonly preventBrowserGesture = (event: Event): void => {
    event.preventDefault();
  };

  private readonly preventSceneTouchEnd = (event: TouchEvent): void => {
    const target = event.target;
    // Ordinary control buttons still use their click events. Their CSS
    // touch-action is locked separately, so do not suppress a single tap here.
    if (target instanceof Element && target.closest('button')) return;
    event.preventDefault();
  };
'''
if old not in text:
    raise SystemExit('PointerController.bind block not found')
text = text.replace(old, new, 1)

old = '''    if (event.pointerType === 'mouse' && event.button !== 0) return;

    const point = { x: event.clientX, y: event.clientY };
'''
new = '''    if (event.pointerType === 'mouse' && event.button !== 0) return;
    if (event.pointerType !== 'mouse') event.preventDefault();

    const point = { x: event.clientX, y: event.clientY };
'''
if old not in text:
    raise SystemExit('pointerdown insertion point not found')
text = text.replace(old, new, 1)

old = '''    const previous = this.pointers.get(event.pointerId);
    if (!previous) return;

    const next = { x: event.clientX, y: event.clientY };
'''
new = '''    const previous = this.pointers.get(event.pointerId);
    if (!previous) return;
    if (event.pointerType !== 'mouse') event.preventDefault();

    const next = { x: event.clientX, y: event.clientY };
'''
if old not in text:
    raise SystemExit('pointermove insertion point not found')
text = text.replace(old, new, 1)

old = '''    const last = this.pointers.get(event.pointerId);
    const start = this.starts.get(event.pointerId);
    if (!last || !start) return;

    const distance = Math.hypot(last.x - start.x, last.y - start.y);
'''
new = '''    const last = this.pointers.get(event.pointerId);
    const start = this.starts.get(event.pointerId);
    if (!last || !start) return;
    if (event.pointerType !== 'mouse') event.preventDefault();

    const distance = Math.hypot(last.x - start.x, last.y - start.y);
'''
if old not in text:
    raise SystemExit('pointer end insertion point not found')
text = text.replace(old, new, 1)
path.write_text(text)

# 4) Make the entire full-screen app non-page-zoomable/non-scrollable, including
# transparent control hitboxes layered over the viewport.
path = Path('web/styles/base.css')
text = path.read_text()
old = '''  margin: 0;
  background: #0f1720;
}'''
new = '''  margin: 0;
  overflow: hidden;
  overscroll-behavior: none;
  touch-action: none;
  background: #0f1720;
}'''
if old not in text:
    raise SystemExit('base css block not found')
text = text.replace(old, new, 1)
path.write_text(text)

path = Path('web/styles/controls.css')
text = path.read_text()
if '  touch-action: manipulation;\n' not in text:
    raise SystemExit('control touch-action declaration not found')
text = text.replace('  touch-action: manipulation;\n', '  touch-action: none;\n', 1)
path.write_text(text)

# 5) Browser regression: verify the page-level gesture escape hatch is shut and
# control clicks remain usable.
path = Path('scripts/joystick-browser-smoke.ts')
text = path.read_text()
needle = '''  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  const layout = await page.evaluate(() => {
'''
replacement = '''  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  console.log('[joystick-browser] browser zoom gestures stay trapped in the game viewport');
  const gesturePolicy = await page.evaluate(() => {
    const viewport = document.getElementById('viewport') as HTMLElement | null;
    const resetCamera = document.getElementById('reset-camera') as HTMLButtonElement | null;
    const meta = document.querySelector('meta[name="viewport"]')?.getAttribute('content') ?? '';
    if (!viewport || !resetCamera) return null;

    const doubleClick = new MouseEvent('dblclick', { bubbles: true, cancelable: true });
    const doubleClickAllowed = viewport.dispatchEvent(doubleClick);
    const sceneTouchEnd = new Event('touchend', { bubbles: true, cancelable: true });
    const sceneTouchEndAllowed = viewport.dispatchEvent(sceneTouchEnd);
    const controlTouchEnd = new Event('touchend', { bubbles: true, cancelable: true });
    const controlTouchEndAllowed = resetCamera.dispatchEvent(controlTouchEnd);

    return {
      meta,
      viewportTouchAction: getComputedStyle(viewport).touchAction,
      bodyTouchAction: getComputedStyle(document.body).touchAction,
      controlTouchAction: getComputedStyle(resetCamera).touchAction,
      doubleClickAllowed,
      sceneTouchEndAllowed,
      controlTouchEndAllowed
    };
  });
  if (!gesturePolicy) throw new Error('Gesture policy elements are missing.');
  if (!gesturePolicy.meta.includes('maximum-scale=1') || !gesturePolicy.meta.includes('user-scalable=no')) {
    throw new Error(`Viewport zoom lock regressed: ${gesturePolicy.meta}`);
  }
  if (
    gesturePolicy.viewportTouchAction !== 'none' ||
    gesturePolicy.bodyTouchAction !== 'none' ||
    gesturePolicy.controlTouchAction !== 'none'
  ) {
    throw new Error(`Native touch-action escaped the game: ${JSON.stringify(gesturePolicy)}`);
  }
  if (gesturePolicy.doubleClickAllowed) throw new Error('Viewport dblclick default was not cancelled.');
  if (gesturePolicy.sceneTouchEndAllowed) throw new Error('Scene touchend default was not cancelled.');
  if (!gesturePolicy.controlTouchEndAllowed) {
    throw new Error('Single control touchend was unnecessarily cancelled, risking lost button clicks.');
  }

  const layout = await page.evaluate(() => {
'''
if needle not in text:
    raise SystemExit('joystick browser insertion point not found')
text = text.replace(needle, replacement, 1)
path.write_text(text)

print('pan extent and mobile gesture patch applied')
