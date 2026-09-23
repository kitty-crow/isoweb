from pathlib import Path
p = Path('src/engine/render/Renderer.cpp')
text = p.read_text()
text = text.replace('constexpr int GAMMA_BUCKET_COUNT = 8192;\n', '', 1)
start = text.find('const std::array<std::uint8_t, GAMMA_BUCKET_COUNT>& gammaBucketStarts() {')
if start < 0:
    raise SystemExit('gamma bucket helper not found')
end_marker = '\n}\n\n} // namespace'
end = text.find(end_marker, start)
if end < 0:
    raise SystemExit('gamma bucket helper end not found')
text = text[:start] + text[end + 3:]
old = '''std::uint8_t Renderer::toByte(float value) {\n  if (value <= 0.0f) return 0;\n  if (value >= 1.0f) return 255;\n\n  const auto& thresholds = gammaThresholds();\n  const auto& starts = gammaBucketStarts();\n  int bucket = static_cast<int>(value * static_cast<float>(GAMMA_BUCKET_COUNT));\n  bucket = std::max(0, std::min(GAMMA_BUCKET_COUNT - 1, bucket));\n  int output = starts[static_cast<std::size_t>(bucket)];\n\n  // Correct in both directions so bin-boundary floating-point rounding can\n  // never alter the transfer curve.\n  while (output > 0 && value < thresholds[static_cast<std::size_t>(output - 1)]) {\n    --output;\n  }\n  while (output < 255 && value >= thresholds[static_cast<std::size_t>(output)]) {\n    ++output;\n  }\n  return static_cast<std::uint8_t>(output);\n}'''
new = '''std::uint8_t Renderer::toByte(float value) {\n  if (value <= 0.0f) return 0;\n  if (value >= 1.0f) return 255;\n  const auto& thresholds = gammaThresholds();\n  return static_cast<std::uint8_t>(\n    std::upper_bound(thresholds.begin(), thresholds.end(), value) - thresholds.begin()\n  );\n}'''
if old not in text:
    raise SystemExit('bucketed toByte implementation not found')
text = text.replace(old, new, 1)
p.write_text(text)
print('Restored reference gamma conversion exactly.')
