from pathlib import Path
import re

# Renderer: settled preview is full-resolution (still demand-driven/lazy), while
# the immediate camera-motion fallback stays cheap. Keep an aspect-preserving
# memory budget for unusually large render targets.
renderer = Path('src/engine/render/Renderer.cpp')
text = renderer.read_text()
text = text.replace(
    'constexpr std::size_t MAX_PREVIEW_PIXELS = 360000;',
    'constexpr std::size_t MAX_PREVIEW_PIXELS = 1600000;'
)
text = text.replace(
    'float previewScale = std::max(0.0625f, std::min(0.5f, world_.lowerPreviewResolutionScale()));',
    'float previewScale = std::max(0.0625f, std::min(1.0f, world_.lowerPreviewResolutionScale()));'
)
renderer.write_text(text)

# Demo asks for a full-resolution settled preview. This does not make lower
# levels full render-resident: the sampler remains the cheap analytic floor plan
# and only demanded/visible texels are refined.
demo = Path('src/demo/DemoWorld.cpp')
text = demo.read_text()
if 'setLowerPreviewResolutionScale(0.50f);' not in text:
    raise SystemExit('demo preview scale line not found')
text = text.replace('setLowerPreviewResolutionScale(0.50f);', 'setLowerPreviewResolutionScale(1.0f);', 1)
demo.write_text(text)

# World: allow 1.0 preview scale and restore lower Characters to the lightweight
# full-screen runtime overlay instead of baking them into low-resolution floor
# texels. Preview Characters skip lower-level lighting/shadow queries.
hpp = Path('src/engine/world/World.hpp')
text = hpp.read_text()
text = text.replace(
    '    bool selected = false;\n\n    // Sprite state and geometry are fixed for one render pass.',
    '    bool selected = false;\n    bool previewOverlay = false;\n\n    // Sprite state and geometry are fixed for one render pass.'
)
text = text.replace(
    '      runtimeRenderEntries_.empty() &&\n      destinationFeedbackMarkers_.empty()\n',
    '      runtimeRenderEntries_.empty() &&\n      destinationFeedbackMarkers_.empty() &&\n      lowDetailPreviewMarkers_.empty()\n'
)
hpp.write_text(text)

world = Path('src/engine/world/World.cpp')
text = world.read_text()
text = text.replace(
    'const float nextScale = std::max(0.0625f, std::min(0.5f, scale));',
    'const float nextScale = std::max(0.0625f, std::min(1.0f, scale));'
)

old_lower_block = '''    const std::size_t characterLevelIndex = levelIndex(character->location.levelId);
    if (
      character->location.liminalObjectId.empty() &&
      characterLevelIndex < activeLevelIndex_ &&
      activeLevelIndex_ - characterLevelIndex <= lowerLevelPreviewDepth_
    ) {
      LowDetailPreviewCharacter preview;
      preview.levelIndex = characterLevelIndex;
      preview.position = character->location.position;
      Vec3 forward = character->forward;
      const float magnitudeSquared = forward.x * forward.x + forward.y * forward.y;
      if (magnitudeSquared > 1e-12f) {
        const float inverseMagnitude = 1.0f / std::sqrt(magnitudeSquared);
        forward = {forward.x * inverseMagnitude, forward.y * inverseMagnitude, 0.0f};
      } else {
        forward = {0.0f, 1.0f, 0.0f};
      }
      preview.forward = forward;
      preview.right = {forward.y, -forward.x, 0.0f};
      preview.minimumX = character->hitBox.minimum.x;
      preview.maximumX = character->hitBox.maximum.x;
      preview.minimumY = character->hitBox.minimum.y;
      preview.maximumY = character->hitBox.maximum.y;
      preview.selected = characterSystem_ && characterSystem_->isSelected(character->id);
      lowDetailPreviewCharacters_.push_back(preview);
      continue;
    }

    RuntimeRenderEntry entry;
'''
new_lower_block = '''    const std::size_t characterLevelIndex = levelIndex(character->location.levelId);
    const bool previewOverlayCharacter =
      character->location.liminalObjectId.empty() &&
      characterLevelIndex < activeLevelIndex_ &&
      activeLevelIndex_ - characterLevelIndex <= lowerLevelPreviewDepth_;

    RuntimeRenderEntry entry;
'''
if old_lower_block not in text:
    raise SystemExit('lower Character preview block not found')
text = text.replace(old_lower_block, new_lower_block, 1)
text = text.replace(
    '    entry.selected = characterSystem_ && characterSystem_->isSelected(character->id);\n',
    '    entry.selected = characterSystem_ && characterSystem_->isSelected(character->id);\n    entry.previewOverlay = previewOverlayCharacter;\n',
    1
)

# Dynamic Characters/markers must not invalidate the static preview cache.
sig_start = text.find('  // Track only data that can change the cheap lower-preview pixels.')
sig_end_marker = '  runtimeSampleScratch_.clear();\n'
if sig_start < 0:
    raise SystemExit('preview signature block start not found')
sig_end = text.find(sig_end_marker, sig_start)
if sig_end < 0:
    raise SystemExit('preview signature block end not found')
text = text[:sig_start] + '  // Lower-preview geometry is static. Characters and destination feedback are\n  // composited separately at full screen resolution, so their motion does not\n  // invalidate or restart progressive floor refinement.\n' + text[sig_end:]

# Strip dynamic overlays from the static low-detail floor sampler.
marker_start = text.find('  const SelectionStyle style = characterSystem_\n', text.find('bool World::sampleLowDetailLowerPreview'))
return_marker = text.find('  return true;\n}\n\nVec3 World::sample(', marker_start)
if marker_start < 0 or return_marker < 0:
    raise SystemExit('static preview dynamic-overlay block not found')
text = text[:marker_start] + text[return_marker:]

# Insert cheap full-resolution lower-preview dynamic visibility/compositing into
# sampleRuntimeEntities. It analytically resolves which preview floor is visible
# for this ray, so Bottom Characters cannot show through Middle. No lower scene
# raycast, lighting ray or shadow query is performed.
needle = '''  const Vec3 compositedEnvironment = compositeDestinationFeedback(
    ray,
    environmentColour,
    environmentHitDistance,
    destinationFound
  );

  std::vector<RuntimeSample>& samples = runtimeSampleScratch_;
'''
replacement = '''  Vec3 compositedEnvironment = compositeDestinationFeedback(
    ray,
    environmentColour,
    environmentHitDistance,
    destinationFound
  );

  std::size_t visiblePreviewLevel = levels_.size();
  Vec3 visiblePreviewLocalPoint;
  bool visiblePreviewResolved = false;
  if (
    environmentHitDistance > 1.0e20f &&
    (!lowDetailPreviewMarkers_.empty() || !runtimeRenderEntries_.empty())
  ) {
    for (
      std::size_t depth = 1;
      depth <= lowerLevelPreviewDepth_ && depth <= activeLevelIndex_;
      ++depth
    ) {
      const std::size_t index = activeLevelIndex_ - depth;
      const RoomLayout* layout = levels_[index]->roomLayout();
      if (!layout || layout->rooms.empty()) continue;
      const Vec3 offset = levelOffsetInActiveView(index);
      for (const Room& room : layout->rooms) {
        const float planeZ = room.floorZ + offset.z;
        if (std::fabs(ray.direction.z) < 1e-7f) continue;
        const float t = (planeZ - ray.origin.z) / ray.direction.z;
        if (t <= 0.001f) continue;
        const Vec3 localPoint = ray.origin + ray.direction * t - offset;
        if (!room.containsXY(localPoint.x, localPoint.y, 0.0015f)) continue;
        visiblePreviewLevel = index;
        visiblePreviewLocalPoint = localPoint;
        visiblePreviewResolved = true;
        break;
      }
      if (visiblePreviewResolved) break;
    }
  }

  bool previewDestinationFound = false;
  if (visiblePreviewResolved && !lowDetailPreviewMarkers_.empty()) {
    const SelectionStyle style = characterSystem_
      ? characterSystem_->selectionStyle()
      : SelectionStyle();
    for (const LowDetailPreviewMarker& marker : lowDetailPreviewMarkers_) {
      if (marker.levelIndex != visiblePreviewLevel) continue;
      const Vec3 delta = visiblePreviewLocalPoint - marker.position;
      const float localX = dot(delta, marker.right);
      const float localY = dot(delta, marker.forward);
      if (
        localX < marker.minimumX || localX > marker.maximumX ||
        localY < marker.minimumY || localY > marker.maximumY
      ) continue;
      const float pulse = 0.5f + 0.5f * std::sin(
        marker.elapsedSeconds * 6.28318530717958647692f * 1.75f
      );
      const float alpha = std::max(0.15f, std::min(0.58f, 0.18f + 0.40f * pulse));
      compositedEnvironment = style.tint * alpha + compositedEnvironment * (1.0f - alpha);
      previewDestinationFound = true;
    }
  }

  std::vector<RuntimeSample>& samples = runtimeSampleScratch_;
'''
if needle not in text:
    raise SystemExit('sampleRuntimeEntities insertion point not found')
text = text.replace(needle, replacement, 1)

text = text.replace(
    '    const Character* character = entry.character;\n    if (!character) continue;\n\n    RuntimeSample sample;\n',
    '    const Character* character = entry.character;\n    if (!character) continue;\n    if (entry.previewOverlay && (!visiblePreviewResolved || entry.levelIndex != visiblePreviewLevel)) continue;\n\n    RuntimeSample sample;\n',
    1
)

old_sprite_light = '''            sample.colour = sample.colour * runtimeSpriteLightFactor(
              entry.levelIndex,
              sample.point - entry.viewOffset
            );
'''
new_sprite_light = '''            if (!entry.previewOverlay) {
              sample.colour = sample.colour * runtimeSpriteLightFactor(
                entry.levelIndex,
                sample.point - entry.viewOffset
              );
            }
'''
if old_sprite_light not in text:
    raise SystemExit('sprite lighting block not found')
text = text.replace(old_sprite_light, new_sprite_light, 1)

old_box_light = '''    sample.colour = shadeRuntimeSurface(
      entry.levelIndex,
      sample.point - entry.viewOffset,
      hit.worldNormal,
      sample.colour
    );
'''
new_box_light = '''    if (!entry.previewOverlay) {
      sample.colour = shadeRuntimeSurface(
        entry.levelIndex,
        sample.point - entry.viewOffset,
        hit.worldNormal,
        sample.colour
      );
    }
'''
if old_box_light not in text:
    raise SystemExit('debug-box lighting block not found')
text = text.replace(old_box_light, new_box_light, 1)

text = text.replace(
    '  if (samples.empty()) {\n    found = destinationFound;\n    return destinationFound ? compositedEnvironment : Vec3();\n  }\n',
    '  if (samples.empty()) {\n    found = destinationFound || previewDestinationFound;\n    return found ? compositedEnvironment : Vec3();\n  }\n',
    1
)

world.write_text(text)

# Regression: full settled preview density, and lower Character/marker must be
# visible through the full-resolution runtime compositor rather than baked into
# a low-res floor texel.
smoke = Path('scripts/progressive-preview-smoke.ts')
text = smoke.read_text()
text = text.replace(
    'initial.potential >= initial.framePixels * 0.20',
    'initial.potential >= initial.framePixels * 0.95'
)
smoke.write_text(text)

feedback = Path('scripts/destination-feedback-smoke.cpp')
text = feedback.read_text()
old_body = '''  Vec3 lowerBody;
  require(
    cheapPreview({-12.0f, 0.0f, lowerPreviewZ}, lowerBody),
    "second lower preview floor was not available to the cheap sampler"
  );
  require(
    colourDistance(lowerBody, {0.82f, 0.84f, 0.88f}) < 0.05f,
    "Character on second lower preview level was not visible in the cheap preview"
  );

  Vec3 withMarker;
  require(
    cheapPreview({-11.0f, -2.0f, lowerPreviewZ}, withMarker),
    "lower destination preview floor was not available"
  );
  lowerCharacter->moving = false;
  previewWorld.prepareRenderFrame(previewDirection);
  Vec3 withoutMarker;
  require(cheapPreview({-11.0f, -2.0f, lowerPreviewZ}, withoutMarker), "marker baseline preview missing");
  require(
    colourDistance(withMarker, withoutMarker) > 0.02f,
    "destination feedback on second lower preview level was not visible in the cheap preview"
  );
'''
new_body = '''  const Vec3 lowerBodyPoint{-12.0f, 0.0f, lowerPreviewZ};
  const Ray lowerBodyRay{lowerBodyPoint - previewDirection * 24.0f, previewDirection};
  Vec3 lowerBodyFloor;
  require(
    cheapPreview(lowerBodyPoint, lowerBodyFloor),
    "second lower preview floor was not available to the cheap sampler"
  );
  const Vec3 lowerBodyRuntime = previewWorld.compositeRuntime(
    lowerBodyRay,
    lowerBodyFloor,
    std::numeric_limits<float>::max()
  );
  require(
    colourDistance(lowerBodyFloor, lowerBodyRuntime) > 0.03f,
    "Character on second lower preview level was not visible in the full-resolution preview overlay"
  );

  const Vec3 markerPoint{-11.0f, -2.0f, lowerPreviewZ};
  const Ray markerRay{markerPoint - previewDirection * 24.0f, previewDirection};
  Vec3 markerFloor;
  require(cheapPreview(markerPoint, markerFloor), "lower destination preview floor was not available");
  const Vec3 withMarker = previewWorld.compositeRuntime(
    markerRay,
    markerFloor,
    std::numeric_limits<float>::max()
  );
  lowerCharacter->moving = false;
  previewWorld.prepareRenderFrame(previewDirection);
  const Vec3 withoutMarker = previewWorld.compositeRuntime(
    markerRay,
    markerFloor,
    std::numeric_limits<float>::max()
  );
  require(
    colourDistance(withMarker, withoutMarker) > 0.02f,
    "destination feedback on second lower preview level was not visible in the full-resolution preview overlay"
  );
'''
if old_body not in text:
    raise SystemExit('destination lower-preview regression block not found')
text = text.replace(old_body, new_body, 1)
feedback.write_text(text)

print('preview quality and full-resolution lower Character overlay patch applied')
