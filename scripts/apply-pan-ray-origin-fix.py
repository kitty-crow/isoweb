from pathlib import Path

path = Path('src/engine/render/Renderer.cpp')
text = path.read_text()
old = '''float rayOriginDistance(const WorldBounds& bounds, const Vec3& forward) {
  float distance = BASE_RAY_ORIGIN_DISTANCE;
  for (const Vec3& point : bounds.points) {
    // A ray begins at focus - forward * distance. Ensure every visible-bound
    // point is comfortably in front of that plane. The margin also covers
    // normal Character height beyond a floor-only edge of the static bounds.
    const float projection = dot(point - bounds.focus, forward);
    distance = std::max(distance, -projection + RAY_ORIGIN_MARGIN);
  }
  return distance;
}
'''
new = '''float rayOriginDistance(
  const WorldBounds& bounds,
  const Vec3& forward,
  const Vec3& focus
) {
  float distance = BASE_RAY_ORIGIN_DISTANCE;
  for (const Vec3& point : bounds.points) {
    // A ray begins at the current camera focus - forward * distance. Use the
    // panned focus, not the authored bounds focus, so large valid pan offsets
    // cannot move the ray origin plane through distant rooms.
    const float projection = dot(point - focus, forward);
    distance = std::max(distance, -projection + RAY_ORIGIN_MARGIN);
  }
  return distance;
}
'''
if old not in text:
    raise SystemExit('rayOriginDistance helper not found')
text = text.replace(old, new, 1)
text = text.replace(
    'rayOriginDistance(visibleBounds, forward)',
    'rayOriginDistance(visibleBounds, forward, focus)'
)
path.write_text(text)
print('panned ray-origin safety applied')
