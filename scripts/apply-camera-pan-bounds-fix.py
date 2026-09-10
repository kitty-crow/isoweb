from pathlib import Path

camera = Path('src/engine/camera/Camera.cpp')
text = camera.read_text()

old_helper = '''float clampPan(float value, float limit) {
  return std::max(-limit, std::min(limit, value));
}
'''
new_helper = '''float clampPan(float value, float minimum, float maximum) {
  return std::max(minimum, std::min(maximum, value));
}

Vec3 clampPanToBounds(
  const Vec3& candidate,
  const Vec3& forward,
  const Vec3& right,
  const Vec3& down,
  float viewHeight,
  float aspect,
  const WorldBounds& bounds,
  float fallbackLimit
) {
  if (bounds.points.empty() || viewHeight <= 1e-7f || aspect <= 1e-7f) {
    return {
      clampPan(candidate.x, -fallbackLimit, fallbackLimit),
      clampPan(candidate.y, -fallbackLimit, fallbackLimit),
      0.0f
    };
  }

  const Vec3 up = normalise(cross(right, forward));
  const float verticalProjection = dot(down, up);
  if (std::fabs(verticalProjection) <= 1e-7f) return candidate;

  float minimumRight = 1e9f;
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

  return right * clampedRight + down * (clampedUp / verticalProjection);
}
'''
if old_helper not in text:
    raise SystemExit('old clamp helper not found')
text = text.replace(old_helper, new_helper, 1)

old_signature = '''CameraControlState Camera::controlState(
  int,
  int,
  const WorldBounds&,
  bool panEnabled,
  float wholeZoomScaleValue
) const {
'''
new_signature = '''CameraControlState Camera::controlState(
  int frameWidth,
  int frameHeight,
  const WorldBounds& bounds,
  bool panEnabled,
  float wholeZoomScaleValue
) const {
'''
if old_signature not in text:
    raise SystemExit('controlState overload signature not found')
text = text.replace(old_signature, new_signature, 1)

old_control = '''  if (panEnabled) {
    const Vec3 rightAxis = groundRight();
    const Vec3 downAxis = groundDown();
    const auto wouldMove = [&](float right, float down) {
      const Vec3 delta = rightAxis * right + downAxis * down;
      const float nextX = clampPan(panX_ + delta.x, config_.panLimit);
      const float nextY = clampPan(panY_ + delta.y, config_.panLimit);
      return std::fabs(nextX - panX_) > 0.0001f ||
        std::fabs(nextY - panY_) > 0.0001f;
    };
'''
new_control = '''  if (panEnabled) {
    const Vec3 rightAxis = groundRight();
    const Vec3 downAxis = groundDown();
    const float aspect = static_cast<float>(frameWidth) / frameHeight;
    const float currentHeight = viewHeight(frameWidth, frameHeight, bounds);
    const auto wouldMove = [&](float right, float down) {
      const Vec3 delta = rightAxis * right + downAxis * down;
      const Vec3 next = clampPanToBounds(
        {panX_ + delta.x, panY_ + delta.y, 0.0f},
        forward(),
        rightAxis,
        downAxis,
        currentHeight,
        aspect,
        bounds,
        config_.panLimit
      );
      return std::fabs(next.x - panX_) > 0.0001f ||
        std::fabs(next.y - panY_) > 0.0001f;
    };
'''
if old_control not in text:
    raise SystemExit('control pan block not found')
text = text.replace(old_control, new_control, 1)

old_pan = '''  const Vec3 delta = rightAxis * quantisedRight + downAxis * quantisedDown;
  const float unclampedX = panX_ + delta.x;
  const float unclampedY = panY_ + delta.y;
  const float nextX = clampPan(unclampedX, config_.panLimit);
  const float nextY = clampPan(unclampedY, config_.panLimit);

  if (nextX != unclampedX || nextY != unclampedY) resetPanPixelRemainder();
  panX_ = nextX;
  panY_ = nextY;
'''
new_pan = '''  const Vec3 delta = rightAxis * quantisedRight + downAxis * quantisedDown;
  const Vec3 candidate{panX_ + delta.x, panY_ + delta.y, 0.0f};
  const float aspect = static_cast<float>(frameWidth) / frameHeight;
  const Vec3 clamped = clampPanToBounds(
    candidate,
    forward(),
    rightAxis,
    downAxis,
    height,
    aspect,
    bounds,
    config_.panLimit
  );

  if (
    std::fabs(clamped.x - candidate.x) > 1e-6f ||
    std::fabs(clamped.y - candidate.y) > 1e-6f
  ) resetPanPixelRemainder();
  panX_ = clamped.x;
  panY_ = clamped.y;
'''
if old_pan not in text:
    raise SystemExit('pan clamp block not found')
text = text.replace(old_pan, new_pan, 1)
camera.write_text(text)

iworld = Path('src/engine/world/IWorld.hpp')
text = iworld.read_text()
needle = '  virtual const WorldBounds& bounds() const = 0;\n'
replacement = needle + '  virtual const WorldBounds& cameraBounds() const { return bounds(); }\n'
if replacement not in text:
    if needle not in text:
        raise SystemExit('IWorld bounds declaration not found')
    text = text.replace(needle, replacement, 1)
iworld.write_text(text)

hpp = Path('src/engine/world/World.hpp')
text = hpp.read_text()
needle = '  const WorldBounds& bounds() const override;\n'
replacement = needle + '  const WorldBounds& cameraBounds() const override;\n'
if replacement not in text:
    if needle not in text:
        raise SystemExit('World bounds declaration not found')
    text = text.replace(needle, replacement, 1)
hpp.write_text(text)

world = Path('src/engine/world/World.cpp')
text = world.read_text()
needle = '''const WorldBounds& World::bounds() const {
  return visibleBounds_;
}
'''
replacement = needle + '''
const WorldBounds& World::cameraBounds() const {
  return activeLevel().bounds();
}
'''
if replacement not in text:
    if needle not in text:
        raise SystemExit('World bounds definition not found')
    text = text.replace(needle, replacement, 1)
world.write_text(text)

renderer = Path('src/engine/render/Renderer.cpp')
text = renderer.read_text()
old_ray = '''  const WorldBounds& bounds = world_.bounds();
  const Vec3 focus = frameCanPan_
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;

  return {
    focus - forward * rayOriginDistance(bounds, forward) + right * screenX + up * screenY,
'''
new_ray = '''  const WorldBounds& cameraBounds = world_.cameraBounds();
  const WorldBounds& visibleBounds = world_.bounds();
  const Vec3 focus = frameCanPan_
    ? cameraBounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : cameraBounds.focus;

  return {
    focus - forward * rayOriginDistance(visibleBounds, forward) + right * screenX + up * screenY,
'''
if old_ray not in text:
    raise SystemExit('rayForPixel bounds block not found')
text = text.replace(old_ray, new_ray, 1)

old_world_point = '''  const WorldBounds& bounds = world_.bounds();
  const Vec3 focus = frameCanPan_
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;
'''
new_world_point = '''  const WorldBounds& bounds = world_.cameraBounds();
  const Vec3 focus = frameCanPan_
    ? bounds.focus + Vec3(camera_.panX(), camera_.panY(), 0.0f)
    : bounds.focus;
'''
if old_world_point not in text:
    raise SystemExit('worldPointToPixel bounds block not found')
text = text.replace(old_world_point, new_world_point, 1)

old_render = '''  const WorldBounds& bounds = world_.bounds();
  const Vec3 forward = camera_.forward();
'''
new_render = '''  const WorldBounds& visibleBounds = world_.bounds();
  const WorldBounds& bounds = world_.cameraBounds();
  const Vec3 forward = camera_.forward();
'''
if old_render not in text:
    raise SystemExit('render bounds declaration not found')
text = text.replace(old_render, new_render, 1)

# Only render()-local ray-origin calls after the declaration should use the full
# visible stacked bounds. Keep camera framing/focus based on active bounds.
render_pos = text.find('void Renderer::render()')
if render_pos < 0:
    raise SystemExit('render function not found')
head = text[:render_pos]
tail = text[render_pos:]
tail = tail.replace('rayOriginDistance(bounds, forward)', 'rayOriginDistance(visibleBounds, forward)')
text = head + tail
renderer.write_text(text)

app = Path('src/demo/DemoApplication.cpp')
text = app.read_text()
text = text.replace('world_.bounds());', 'world_.cameraBounds());')
app.write_text(text)

package = Path('package.json')
text = package.read_text()
needle = '    "test:joystick": "bun scripts/joystick-smoke.ts",\n'
replacement = '    "test:camera-pan": "c++ -std=c++14 -Isrc scripts/camera-pan-bounds-smoke.cpp src/engine/camera/Camera.cpp -o /tmp/isoweb-camera-pan-smoke && /tmp/isoweb-camera-pan-smoke",\n' + needle
if replacement not in text:
    if needle not in text:
        raise SystemExit('package insertion point not found')
    text = text.replace(needle, replacement, 1)
package.write_text(text)

print('active-level camera pan bounds patch applied')
