from pathlib import Path

# Trigger after the patch workflow exists.
path = Path("src/engine/render/Renderer.cpp")
text = path.read_text()
old = """    const std::size_t coarseRequired =
      static_cast<std::size_t>(coarsePreviewWidth_) * coarsePreviewHeight_;
    if (resetPreviewCache || coarsePreviewSamples_.size() != coarseRequired) {
      coarsePreviewSamples_.assign(coarseRequired, PreviewSample());
    }
"""
new = """    // Preserve the reference renderer's cache lifetime exactly: the coarse
    // fallback is transient and is rebuilt for each rendered frame. Threaded
    // builds still resolve the fresh cache serially before worker fan-out.
    coarsePreviewSamples_.assign(
      static_cast<std::size_t>(coarsePreviewWidth_) * coarsePreviewHeight_,
      PreviewSample()
    );
"""
if old not in text:
    raise SystemExit("retained coarse-preview block not found")
path.write_text(text.replace(old, new, 1))
