from pathlib import Path

renderer = Path('src/engine/render/Renderer.cpp')
text = renderer.read_text()

text = text.replace(
    'constexpr int PREVIEW_TILE_SIZE = 16;\nconstexpr float COARSE_PREVIEW_SCALE = 0.125f;\nconstexpr int MAX_COARSE_PREVIEW_WIDTH = 160;\nconstexpr int MAX_COARSE_PREVIEW_HEIGHT = 90;\n',
    'constexpr int PREVIEW_TILE_SIZE = 16;\nconstexpr float COARSE_PREVIEW_SCALE = 0.25f;\nconstexpr std::size_t MAX_PREVIEW_PIXELS = 360000;\nconstexpr std::size_t MAX_COARSE_PREVIEW_PIXELS = 90000;\n'
)

text = text.replace(
    '  // Lower previews are progressive and demand-driven. The normal 0.25x cache\n',
    '  // Lower previews are progressive and demand-driven. The normal high-detail cache\n'
)

old_target = '''    const float previewScale = std::max(0.0625f, std::min(0.5f, world_.lowerPreviewResolutionScale()));
    previewWidth_ = std::max(1, std::min(320, static_cast<int>(std::ceil(frameWidth_ * previewScale))));
    previewHeight_ = std::max(1, std::min(180, static_cast<int>(std::ceil(frameHeight_ * previewScale))));
'''
new_target = '''    float previewScale = std::max(0.0625f, std::min(0.5f, world_.lowerPreviewResolutionScale()));
    const double requestedPreviewPixels =
      static_cast<double>(frameWidth_) * static_cast<double>(frameHeight_) *
      static_cast<double>(previewScale) * static_cast<double>(previewScale);
    if (requestedPreviewPixels > static_cast<double>(MAX_PREVIEW_PIXELS)) {
      previewScale *= static_cast<float>(std::sqrt(
        static_cast<double>(MAX_PREVIEW_PIXELS) / requestedPreviewPixels
      ));
    }
    previewWidth_ = std::max(1, static_cast<int>(std::ceil(frameWidth_ * previewScale)));
    previewHeight_ = std::max(1, static_cast<int>(std::ceil(frameHeight_ * previewScale)));
'''
if old_target not in text:
    raise SystemExit('target preview block not found')
text = text.replace(old_target, new_target)

old_coarse = '''    coarsePreviewWidth_ = std::max(
      1,
      std::min(MAX_COARSE_PREVIEW_WIDTH, static_cast<int>(std::ceil(frameWidth_ * COARSE_PREVIEW_SCALE)))
    );
    coarsePreviewHeight_ = std::max(
      1,
      std::min(MAX_COARSE_PREVIEW_HEIGHT, static_cast<int>(std::ceil(frameHeight_ * COARSE_PREVIEW_SCALE)))
    );
'''
new_coarse = '''    float coarsePreviewScale = COARSE_PREVIEW_SCALE;
    const double requestedCoarsePixels =
      static_cast<double>(frameWidth_) * static_cast<double>(frameHeight_) *
      static_cast<double>(coarsePreviewScale) * static_cast<double>(coarsePreviewScale);
    if (requestedCoarsePixels > static_cast<double>(MAX_COARSE_PREVIEW_PIXELS)) {
      coarsePreviewScale *= static_cast<float>(std::sqrt(
        static_cast<double>(MAX_COARSE_PREVIEW_PIXELS) / requestedCoarsePixels
      ));
    }
    coarsePreviewWidth_ = std::max(
      1,
      static_cast<int>(std::ceil(frameWidth_ * coarsePreviewScale))
    );
    coarsePreviewHeight_ = std::max(
      1,
      static_cast<int>(std::ceil(frameHeight_ * coarsePreviewScale))
    );
'''
if old_coarse not in text:
    raise SystemExit('coarse preview block not found')
text = text.replace(old_coarse, new_coarse)
renderer.write_text(text)

world = Path('src/demo/DemoWorld.cpp')
text = world.read_text()
old = '  setLowerPreviewResolutionScale(0.25f);\n'
if old not in text:
    raise SystemExit('demo preview scale line not found')
text = text.replace(old, '  setLowerPreviewResolutionScale(0.50f);\n', 1)
world.write_text(text)

smoke = Path('scripts/progressive-preview-smoke.ts')
text = smoke.read_text()
old = '''    return {
      potential: module._isoweb_preview_potential_texel_count(),
      demanded: module._isoweb_preview_demanded_texel_count(),
      coarse: module._isoweb_preview_coarse_sample_count(),
      refined: module._isoweb_preview_refined_sample_count(),
      needs: module._isoweb_preview_needs_refinement()
    };
'''
new = '''    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    return {
      potential: module._isoweb_preview_potential_texel_count(),
      demanded: module._isoweb_preview_demanded_texel_count(),
      coarse: module._isoweb_preview_coarse_sample_count(),
      refined: module._isoweb_preview_refined_sample_count(),
      needs: module._isoweb_preview_needs_refinement(),
      framePixels: canvas ? canvas.width * canvas.height : 0
    };
'''
if old not in text:
    raise SystemExit('initial diagnostics block not found')
text = text.replace(old, new, 1)
needle = "  if (!(initial.potential > 0)) throw new Error(`No lower-preview buffer on upper level: ${JSON.stringify(initial)}`);\n"
replacement = needle + "  if (!(initial.framePixels > 0 && initial.potential >= initial.framePixels * 0.20)) {\n    throw new Error(`Settled preview target is still too heavily downsampled: ${JSON.stringify(initial)}`);\n  }\n"
if needle not in text:
    raise SystemExit('preview potential assertion not found')
text = text.replace(needle, replacement, 1)
smoke.write_text(text)

print('high-detail preview rebalance applied')
