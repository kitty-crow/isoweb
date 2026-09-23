from pathlib import Path

p = Path('src/engine/render/Renderer.cpp')
text = p.read_text()

static_old = '''        std::size_t pixelIndex =\n          static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth_);\n\n        for (int x = xBegin; x < xEnd; ++x, ++pixelIndex) {\n          for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {\n            StaticSample& sample = staticSamples_[pixelIndex * 4 + sampleIndex];'''
static_new = '''        std::size_t pixelIndex =\n          static_cast<std::size_t>(y) * static_cast<std::size_t>(frameWidth_);\n\n        for (int x = 0; x < frameWidth_; ++x, ++pixelIndex) {\n          for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {\n            StaticSample& sample = staticSamples_[pixelIndex * 4 + sampleIndex];'''
if static_old not in text:
    raise SystemExit('static-cache loop correction anchor not found')
text = text.replace(static_old, static_new, 1)

dynamic_old = '''      std::uint8_t* frameRow = reinterpret_cast<std::uint8_t*>(\n        &dsr::image_accessPixel(frame_, 0, y)\n      );\n\n      for (int x = 0; x < frameWidth_; ++x, ++pixelIndex) {\n        PreviewSample* previewForPixel = nullptr;'''
dynamic_new = '''      std::uint8_t* frameRow = reinterpret_cast<std::uint8_t*>(\n        &dsr::image_accessPixel(frame_, 0, y)\n      );\n\n      for (int x = xBegin; x < xEnd; ++x, ++pixelIndex) {\n        PreviewSample* previewForPixel = nullptr;'''
if dynamic_old not in text:
    raise SystemExit('runtime-compositor loop correction anchor not found')
text = text.replace(dynamic_old, dynamic_new, 1)

p.write_text(text)
print('Corrected damage spans to affect only runtime compositing.')
