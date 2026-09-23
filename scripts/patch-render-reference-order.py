from pathlib import Path

path = Path("src/engine/render/Renderer.cpp")
text = path.read_text()

old = """  const bool previewPreparedReadOnly =\n    previewWidth_ == 0 ||\n    (useStaticCache && previewWidth_ > 0 && coarsePreviewWidth_ > 0);\n"""
new = """#ifdef ISOWEB_ENABLE_RENDER_THREADS\n  const bool previewPreparedReadOnly =\n    previewWidth_ == 0 ||\n    (useStaticCache && previewWidth_ > 0 && coarsePreviewWidth_ > 0);\n#else\n  // Keep the single-thread/reference renderer in the original lazy preview\n  // evaluation order. Pre-resolution exists only to make preview state immutable\n  // before pthread workers fan out.\n  const bool previewPreparedReadOnly = false;\n#endif\n"""
if old not in text:
    raise SystemExit("previewPreparedReadOnly block not found")
text = text.replace(old, new, 1)

old = "  if (previewWidth_ > 0 && useStaticCache && coarsePreviewWidth_ > 0) {"
new = "  if (previewPreparedReadOnly && previewWidth_ > 0 && useStaticCache && coarsePreviewWidth_ > 0) {"
if old not in text:
    raise SystemExit("preview pre-resolution guard not found")
text = text.replace(old, new, 1)

path.write_text(text)
