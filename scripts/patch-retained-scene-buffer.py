from pathlib import Path

hpp = Path("src/engine/render/Renderer.hpp")
hpp_text = hpp.read_text()
old = "  dsr::OrderedImageRgbaU8 frame_;\n  std::vector<std::uint8_t> rgba_;\n"
new = "  dsr::OrderedImageRgbaU8 frame_;\n  std::vector<std::uint8_t> rgba_;\n  // Clean scene-only framebuffer retained separately from UI/control overlays.\n  std::vector<std::uint8_t> sceneRgba_;\n"
if old not in hpp_text:
    raise SystemExit("Renderer.hpp framebuffer fields not found")
hpp.write_text(hpp_text.replace(old, new, 1))

cpp = Path("src/engine/render/Renderer.cpp")
text = cpp.read_text()

old = "  const std::size_t required = static_cast<std::size_t>(frameWidth_) * frameHeight_ * 4;\n  if (rgba_.size() != required) rgba_.resize(required);\n"
new = "  const std::size_t required = static_cast<std::size_t>(frameWidth_) * frameHeight_ * 4;\n  if (rgba_.size() != required) rgba_.resize(required);\n  if (sceneRgba_.size() != required) sceneRgba_.resize(required);\n"
if old not in text:
    raise SystemExit("ensureFrame buffer sizing block not found")
text = text.replace(old, new, 1)

needle = """  if (fullSceneRender) {\n    std::fill(dirtyMinimumX_.begin(), dirtyMinimumX_.end(), 0);\n    std::fill(dirtyMaximumX_.begin(), dirtyMaximumX_.end(), frameWidth_);\n  } else {\n    for (const DamageRect& rect : previousDamageRects_) markDamage(rect);\n    for (const DamageRect& rect : currentDamageRects_) markDamage(rect);\n  }\n\n  // Runtime compositing, gamma conversion and framebuffer writes are independent\n"""
replacement = """  if (fullSceneRender) {\n    std::fill(dirtyMinimumX_.begin(), dirtyMinimumX_.end(), 0);\n    std::fill(dirtyMaximumX_.begin(), dirtyMaximumX_.end(), frameWidth_);\n  } else {\n    for (const DamageRect& rect : previousDamageRects_) markDamage(rect);\n    for (const DamageRect& rect : currentDamageRects_) markDamage(rect);\n  }\n\n  // frame_ contains controls from the preceding presented frame. Retained/damage\n  // rendering must start from the clean scene, otherwise semi-transparent control\n  // sprites are blended repeatedly and dirty regions can partially erase them.\n  // Restoring a contiguous RGBA buffer is deliberately cheap compared with the\n  // four exact ray samples avoided for every clean pixel.\n  if (!fullSceneRender && sceneRgba_.size() == pixelCount * 4) {\n    const std::size_t sceneRowBytes = static_cast<std::size_t>(frameWidth_) * 4;\n    for (int y = 0; y < frameHeight_; ++y) {\n      std::uint8_t* destination = reinterpret_cast<std::uint8_t*>(\n        &dsr::image_accessPixel(frame_, 0, y)\n      );\n      std::memcpy(\n        destination,\n        sceneRgba_.data() + static_cast<std::size_t>(y) * sceneRowBytes,\n        sceneRowBytes\n      );\n    }\n  }\n\n  // Runtime compositing, gamma conversion and framebuffer writes are independent\n"""
if needle not in text:
    raise SystemExit("damage/full-frame block not found")
text = text.replace(needle, replacement, 1)

needle = """  previousDamageRects_ = currentDamageRects_;\n  damageHistoryValid_ = true;\n  lastRenderedPreviewRevision_ = previewVisualRevision_;\n\n  LevelControlState levelState;\n"""
replacement = """  previousDamageRects_ = currentDamageRects_;\n  damageHistoryValid_ = true;\n  lastRenderedPreviewRevision_ = previewVisualRevision_;\n\n  // Snapshot the exact scene before UI/control sprites are drawn. The next\n  // retained frame restores from this copy, updates only damaged scene pixels,\n  // then applies controls exactly once.\n  const std::size_t sceneRowBytes = static_cast<std::size_t>(frameWidth_) * 4;\n  for (int y = 0; y < frameHeight_; ++y) {\n    const std::uint8_t* source = reinterpret_cast<const std::uint8_t*>(\n      &dsr::image_accessPixel(frame_, 0, y)\n    );\n    std::memcpy(\n      sceneRgba_.data() + static_cast<std::size_t>(y) * sceneRowBytes,\n      source,\n      sceneRowBytes\n    );\n  }\n\n  LevelControlState levelState;\n"""
if needle not in text:
    raise SystemExit("pre-controls snapshot insertion point not found")
text = text.replace(needle, replacement, 1)

cpp.write_text(text)
