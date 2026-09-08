from pathlib import Path


def replace_once(path: str, old: str, new: str, label: str) -> None:
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one anchor, found {count}")
    p.write_text(text.replace(old, new, 1))


replace_once(
    "src/engine/world/World.hpp",
    """  struct RuntimeRenderEntry {\n    const Character* character = nullptr;\n    Vec3 renderPosition;\n    Object proxy;\n    bool selected = false;\n""",
    """  struct RuntimeRenderEntry {\n    const Character* character = nullptr;\n    Vec3 renderPosition;\n    Vec3 viewOffset;\n    std::size_t levelIndex = 0;\n    Object proxy;\n    bool selected = false;\n""",
    "World.hpp runtime entry",
)

replace_once(
    "src/engine/world/World.cpp",
    """bool World::renderPositionFor(const Character& character, Vec3& position) const {\n  return mapLiminalPosition(character.location, activeLevelId(), position);\n}\n""",
    """bool World::renderPositionFor(const Character& character, Vec3& position) const {\n  // Preserve the existing liminal projection when a Character occupies a\n  // connector touching the active level.\n  if (mapLiminalPosition(character.location, activeLevelId(), position)) return true;\n\n  // Ordinary Characters on resident lower preview levels are translated into\n  // the same active-view coordinate stack as static preview geometry. Levels\n  // above the active one, or below the configured preview depth, stay hidden.\n  const std::size_t index = levelIndex(character.location.levelId);\n  if (index >= levels_.size() || index > activeLevelIndex_) return false;\n  if (activeLevelIndex_ - index > lowerLevelPreviewDepth_) return false;\n  position = character.location.position + levelOffsetInActiveView(index);\n  return true;\n}\n""",
    "World.cpp renderPositionFor",
)

replace_once(
    "src/engine/world/World.cpp",
    """  const auto& characters = entities_.characters();\n  runtimeRenderEntries_.reserve(characters.size());\n  destinationFeedbackMarkers_.reserve(characters.size());\n  const std::string& levelId = activeLevelId();\n\n  for (const Character* character : characters) {\n    if (!character) continue;\n\n    if (\n      character->moving &&\n      character->movement.hasDestination &&\n      character->movement.destination.levelId == levelId\n    ) {\n""",
    """  const auto& characters = entities_.characters();\n  runtimeRenderEntries_.reserve(characters.size());\n  destinationFeedbackMarkers_.reserve(characters.size());\n  const std::string& levelId = activeLevelId();\n\n  const auto previewLevelVisible = [&](const std::string& candidateLevelId, std::size_t& index) {\n    index = levelIndex(candidateLevelId);\n    return index < levels_.size() &&\n      index <= activeLevelIndex_ &&\n      activeLevelIndex_ - index <= lowerLevelPreviewDepth_;\n  };\n\n  for (const Character* character : characters) {\n    if (!character) continue;\n\n    std::size_t destinationLevelIndex = levels_.size();\n    if (\n      character->moving &&\n      character->movement.hasDestination &&\n      previewLevelVisible(character->movement.destination.levelId, destinationLevelIndex)\n    ) {\n""",
    "World.cpp preview marker header",
)

replace_once(
    "src/engine/world/World.cpp",
    """      DestinationFeedbackMarker marker;\n      marker.position = character->movement.destination.position;\n      marker.forward = forward;\n""",
    """      DestinationFeedbackMarker marker;\n      marker.position = character->movement.destination.position +\n        levelOffsetInActiveView(destinationLevelIndex);\n      marker.forward = forward;\n""",
    "World.cpp destination marker position",
)

replace_once(
    "src/engine/world/World.cpp",
    """    RuntimeRenderEntry entry;\n    entry.character = character;\n    entry.renderPosition = renderPosition;\n    entry.proxy = renderProxy(*character, renderPosition, levelId);\n    entry.selected = characterSystem_ && characterSystem_->isSelected(character->id);\n""",
    """    RuntimeRenderEntry entry;\n    entry.character = character;\n    entry.renderPosition = renderPosition;\n    entry.viewOffset = renderPosition - character->location.position;\n\n    // A liminal Character explicitly projected into the active endpoint keeps\n    // the active endpoint's lighting, matching the pre-preview behaviour.\n    // Otherwise a lower preview Character uses the light/shadow geometry of\n    // the level it actually occupies.\n    Vec3 activeMappedPosition;\n    const bool mappedToActive = mapLiminalPosition(\n      character->location,\n      levelId,\n      activeMappedPosition\n    );\n    entry.levelIndex = activeLevelIndex_;\n    if (!mappedToActive) {\n      const std::size_t characterLevelIndex = levelIndex(character->location.levelId);\n      if (characterLevelIndex < levels_.size()) entry.levelIndex = characterLevelIndex;\n    }\n\n    entry.proxy = renderProxy(*character, renderPosition, levelId);\n    entry.selected = characterSystem_ && characterSystem_->isSelected(character->id);\n""",
    "World.cpp runtime entry",
)

replace_once(
    "src/engine/world/World.cpp",
    """            sample.colour = sample.colour * runtimeSpriteLightFactor(activeLevelIndex_, sample.point);\n""",
    """            sample.colour = sample.colour * runtimeSpriteLightFactor(\n              entry.levelIndex,\n              sample.point - entry.viewOffset\n            );\n""",
    "World.cpp sprite lighting",
)

replace_once(
    "src/engine/world/World.cpp",
    """    sample.colour = shadeRuntimeSurface(\n      activeLevelIndex_,\n      sample.point,\n      hit.worldNormal,\n      sample.colour\n    );\n""",
    """    sample.colour = shadeRuntimeSurface(\n      entry.levelIndex,\n      sample.point - entry.viewOffset,\n      hit.worldNormal,\n      sample.colour\n    );\n""",
    "World.cpp runtime box lighting",
)

replace_once(
    "src/engine/render/Renderer.cpp",
    """constexpr std::size_t MAX_STATIC_CACHE_PIXELS = 600000;\nconstexpr float PAN_SHIFT_EPSILON = 0.025f;\nconstexpr float NO_HIT_DISTANCE = 1.0e30f;\n\nconst std::array<float, 255>& gammaThresholds() {\n""",
    """constexpr std::size_t MAX_STATIC_CACHE_PIXELS = 600000;\nconstexpr float PAN_SHIFT_EPSILON = 0.025f;\nconstexpr float NO_HIT_DISTANCE = 1.0e30f;\nconstexpr float BASE_RAY_ORIGIN_DISTANCE = 9.0f;\nconstexpr float RAY_ORIGIN_MARGIN = 4.0f;\n\nfloat rayOriginDistance(const WorldBounds& bounds, const Vec3& forward) {\n  float distance = BASE_RAY_ORIGIN_DISTANCE;\n  for (const Vec3& point : bounds.points) {\n    // A ray begins at focus - forward * distance. Ensure every visible-bound\n    // point is comfortably in front of that plane. The margin also covers\n    // normal Character height beyond a floor-only edge of the static bounds.\n    const float projection = dot(point - bounds.focus, forward);\n    distance = std::max(distance, -projection + RAY_ORIGIN_MARGIN);\n  }\n  return distance;\n}\n\nconst std::array<float, 255>& gammaThresholds() {\n""",
    "Renderer.cpp ray distance helper",
)

replace_once(
    "src/engine/render/Renderer.cpp",
    """  return {focus - forward * 9.0f + right * screenX + up * screenY, forward};\n""",
    """  return {\n    focus - forward * rayOriginDistance(bounds, forward) + right * screenX + up * screenY,\n    forward\n  };\n""",
    "Renderer.cpp rayForPixel origin",
)

replace_once(
    "src/engine/render/Renderer.cpp",
    """  const Vec3 cornerOrigin = focus - forward * 9.0f - right * (width * 0.5f) + up * (height * 0.5f);\n""",
    """  const float originDistance = rayOriginDistance(bounds, forward);\n  const Vec3 cornerOrigin =\n    focus - forward * originDistance - right * (width * 0.5f) + up * (height * 0.5f);\n""",
    "Renderer.cpp render origin",
)

replace_once(
    "scripts/destination-feedback-smoke.cpp",
    """  std::cout << \"Character destination acknowledgement footprint smoke test passed.\\n\";\n  return 0;\n}\n""",
    """  // Lower preview runtime entities remain dynamic without rendering a second\n  // complete level. From the upper level, choose a ray that misses upper and\n  // middle geometry before reaching an exposed point on the lower west room.\n  isoweb::demo::DemoWorld previewWorld;\n  CharacterSystem previewCharacters(previewWorld);\n  require(previewWorld.levelUp(), \"could not switch preview regression to upper level\");\n\n  std::unique_ptr<Character> lowerOwned(new Character());\n  Character* lowerCharacter = lowerOwned.get();\n  lowerCharacter->id = \"lower-preview-runner\";\n  lowerCharacter->location = {\"demo\", \"default\", \"lower\", {-12.0f, 0.0f, 0.0f}};\n  lowerCharacter->hitBox.minimum = {-0.38f, -0.16f, 0.0f};\n  lowerCharacter->hitBox.maximum = {0.38f, 0.16f, 1.65f};\n  lowerCharacter->forward = {0.0f, 1.0f, 0.0f};\n  lowerCharacter->moving = true;\n  lowerCharacter->movement.hasDestination = true;\n  lowerCharacter->movement.destination = lowerCharacter->location;\n  lowerCharacter->movement.destination.position = {-12.0f, -2.0f, 0.0f};\n  lowerCharacter->movement.destinationForward = {0.0f, -1.0f, 0.0f};\n  lowerCharacter->movement.feedbackElapsedSeconds = 0.08f;\n  previewWorld.entities().add(std::move(lowerOwned));\n\n  previewCharacters.selection().style.tint = {0.92f, 0.18f, 0.86f};\n  previewCharacters.selection().style.strength = 0.72f;\n\n  const Vec3 previewDirection = normalisedHorizontal({1.0f, 1.0f, 0.0f}) * 0.81649658f +\n    Vec3(0.0f, 0.0f, -0.57735027f);\n  auto previewSample = [&](const Vec3& activeViewPoint) {\n    const Ray ray{activeViewPoint - previewDirection * 24.0f, previewDirection};\n    float environmentDistance = 0.0f;\n    SamplePair pair;\n    pair.environment = previewWorld.sampleEnvironment(ray, 0.5f, environmentDistance);\n    pair.runtime = previewWorld.compositeRuntime(ray, pair.environment, environmentDistance);\n    return pair;\n  };\n\n  previewWorld.prepareRenderFrame(previewDirection);\n\n  // Lower floor is displayed 2.8 units beneath upper in this demo. Sample the\n  // Character body above that floor at a screen ray known to be exposed.\n  const SamplePair lowerBody = previewSample({-12.0f, 0.0f, -2.0f});\n  require(\n    colourDistance(lowerBody.environment, lowerBody.runtime) > 0.03f,\n    \"Character on second lower preview level was not visible from upper\"\n  );\n\n  const SamplePair lowerDestinationFeedback = previewSample({-12.0f, -2.0f, -2.8f});\n  require(\n    colourDistance(lowerDestinationFeedback.environment, lowerDestinationFeedback.runtime) > 0.02f,\n    \"destination feedback on second lower preview level was not visible from upper\"\n  );\n\n  std::cout << \"Character destination acknowledgement and lower-preview runtime smoke test passed.\\n\";\n  return 0;\n}\n""",
    "destination feedback lower preview regression",
)
