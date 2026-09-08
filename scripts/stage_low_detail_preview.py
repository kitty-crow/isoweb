from pathlib import Path


def replace(path, old, new):
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"pattern not found in {path}: {old[:140]!r}")
    p.write_text(text.replace(old, new, 1))


# IWorld: low-detail preview API.
replace(
    "src/engine/world/IWorld.hpp",
    "  virtual bool supportsStaticSampleCache() const { return false; }\n\n  virtual Vec3 sampleEnvironment(",
    "  virtual bool supportsStaticSampleCache() const { return false; }\n\n"
    "  // Lower levels are decorative previews, not additional fully rendered scenes.\n"
    "  // Implementations may expose a cheap analytic preview sampler which the\n"
    "  // renderer evaluates at reduced resolution. This must not perform scene\n"
    "  // ray-tracing, shadow queries, or other active-level-quality work.\n"
    "  virtual bool supportsLowDetailLowerPreview() const { return false; }\n"
    "  virtual float lowerPreviewResolutionScale() const { return 0.25f; }\n"
    "  virtual bool sampleLowDetailLowerPreview(const Ray&, Vec3&) const { return false; }\n\n"
    "  virtual Vec3 sampleEnvironment("
)

# Room layout carries only the flat preview palette, not render implementation.
replace(
    "src/engine/world/Room.hpp",
    "struct RoomLayout {\n  std::vector<Room> rooms;\n  std::vector<RoomConnection> connections;\n",
    "struct RoomLayout {\n  std::vector<Room> rooms;\n  std::vector<RoomConnection> connections;\n\n"
    "  // Flat unlit palette used when this level appears only as a cheap lower preview.\n"
    "  Vec3 previewFloorDark = {0.34f, 0.34f, 0.36f};\n"
    "  Vec3 previewFloorLight = {0.40f, 0.40f, 0.42f};\n"
    "  Vec3 previewWall = {0.23f, 0.23f, 0.25f};\n"
    "  float previewWallBand = 0.10f;\n"
)

# World public API and preview state.
replace(
    "src/engine/world/World.hpp",
    "  bool supportsStaticSampleCache() const override { return true; }\n\n  Vec3 sampleEnvironment(",
    "  bool supportsStaticSampleCache() const override { return true; }\n"
    "  bool supportsLowDetailLowerPreview() const override {\n"
    "    return lowerLevelPreviewDepth_ > 0 && activeLevelIndex_ > 0;\n"
    "  }\n"
    "  float lowerPreviewResolutionScale() const override { return lowerPreviewResolutionScale_; }\n"
    "  bool sampleLowDetailLowerPreview(const Ray& ray, Vec3& colour) const override;\n\n"
    "  Vec3 sampleEnvironment("
)
replace(
    "src/engine/world/World.hpp",
    "  void setLowerLevelPreviewDepth(std::size_t depth);\n  std::size_t lowerLevelPreviewDepth() const { return lowerLevelPreviewDepth_; }\n  bool setLevelViewOrigin",
    "  void setLowerLevelPreviewDepth(std::size_t depth);\n"
    "  std::size_t lowerLevelPreviewDepth() const { return lowerLevelPreviewDepth_; }\n"
    "  void setLowerPreviewResolutionScale(float scale);\n"
    "  bool setLevelViewOrigin"
)
replace(
    "src/engine/world/World.hpp",
    "  // Preview depth is configuration, not a fixed engine limit. A depth of two\n"
    "  // means active + first lower + second lower may participate in the static\n"
    "  // environment render, with upper layers taking visual precedence.\n",
    "  // Preview depth is configuration, not a fixed engine limit. Lower levels\n"
    "  // remain available for topology/input but normal rendering uses only a cheap\n"
    "  // reduced-resolution floor-plan preview; only the active level is full quality.\n"
)
replace(
    "src/engine/world/World.hpp",
    "  const IWorldLevel& activeLevel() const;\n",
    "  struct LowDetailPreviewCharacter {\n"
    "    std::size_t levelIndex = 0;\n"
    "    Vec3 position;\n"
    "    Vec3 forward = {0.0f, 1.0f, 0.0f};\n"
    "    Vec3 right = {1.0f, 0.0f, 0.0f};\n"
    "    float minimumX = 0.0f;\n"
    "    float maximumX = 0.0f;\n"
    "    float minimumY = 0.0f;\n"
    "    float maximumY = 0.0f;\n"
    "    bool selected = false;\n"
    "  };\n\n"
    "  struct LowDetailPreviewMarker {\n"
    "    std::size_t levelIndex = 0;\n"
    "    Vec3 position;\n"
    "    Vec3 forward = {0.0f, 1.0f, 0.0f};\n"
    "    Vec3 right = {1.0f, 0.0f, 0.0f};\n"
    "    float minimumX = 0.0f;\n"
    "    float maximumX = 0.0f;\n"
    "    float minimumY = 0.0f;\n"
    "    float maximumY = 0.0f;\n"
    "    float elapsedSeconds = 0.0f;\n"
    "  };\n\n"
    "  const IWorldLevel& activeLevel() const;\n"
)
replace(
    "src/engine/world/World.hpp",
    "  std::size_t lowerLevelPreviewDepth_ = 0;\n  WorldBounds visibleBounds_;\n",
    "  std::size_t lowerLevelPreviewDepth_ = 0;\n"
    "  float lowerPreviewResolutionScale_ = 0.25f;\n"
    "  WorldBounds visibleBounds_;\n"
)
replace(
    "src/engine/world/World.hpp",
    "  mutable std::vector<RuntimeRenderEntry> runtimeRenderEntries_;\n"
    "  mutable std::vector<DestinationFeedbackMarker> destinationFeedbackMarkers_;\n",
    "  mutable std::vector<RuntimeRenderEntry> runtimeRenderEntries_;\n"
    "  mutable std::vector<DestinationFeedbackMarker> destinationFeedbackMarkers_;\n"
    "  mutable std::vector<LowDetailPreviewCharacter> lowDetailPreviewCharacters_;\n"
    "  mutable std::vector<LowDetailPreviewMarker> lowDetailPreviewMarkers_;\n"
)

# Preview resolution is configurable and clamped to deliberately-low values.
replace(
    "src/engine/world/World.cpp",
    "void World::setLowerLevelPreviewDepth(std::size_t depth) {\n"
    "  const std::size_t maximum = levels_.empty() ? 0 : levels_.size() - 1;\n"
    "  lowerLevelPreviewDepth_ = std::min(depth, maximum);\n"
    "  updateLevelResidency();\n"
    "  updateVisibleBounds();\n"
    "  runtimeRenderCachePrepared_ = false;\n"
    "}\n\n"
    "bool World::setLevelViewOrigin",
    "void World::setLowerLevelPreviewDepth(std::size_t depth) {\n"
    "  const std::size_t maximum = levels_.empty() ? 0 : levels_.size() - 1;\n"
    "  lowerLevelPreviewDepth_ = std::min(depth, maximum);\n"
    "  updateLevelResidency();\n"
    "  updateVisibleBounds();\n"
    "  runtimeRenderCachePrepared_ = false;\n"
    "}\n\n"
    "void World::setLowerPreviewResolutionScale(float scale) {\n"
    "  lowerPreviewResolutionScale_ = std::max(0.0625f, std::min(0.5f, scale));\n"
    "}\n\n"
    "bool World::setLevelViewOrigin"
)

# Render-time environment sampling is now active-level only.
replace(
    "src/engine/world/World.cpp",
    "Vec3 World::sampleEnvironment(\n"
    "  const Ray& ray,\n"
    "  float backgroundY,\n"
    "  float& environmentDistance\n"
    ") const {\n"
    "  SceneSurfaceHit hit;\n"
    "  const Vec3 colour = sampleVisibleEnvironment(ray, backgroundY, hit);\n"
    "  environmentDistance = hit.found\n"
    "    ? hit.distance\n"
    "    : std::numeric_limits<float>::max();\n"
    "  return colour;\n"
    "}\n",
    "Vec3 World::sampleEnvironment(\n"
    "  const Ray& ray,\n"
    "  float backgroundY,\n"
    "  float& environmentDistance\n"
    ") const {\n"
    "  // Full-quality render work is active-level only. Lower floors are supplied\n"
    "  // separately by sampleLowDetailLowerPreview() into a reduced-resolution buffer.\n"
    "  SceneSurfaceHit hit;\n"
    "  const Vec3 colour = activeLevel().sampleWithHit(ray, backgroundY, hit);\n"
    "  environmentDistance = hit.found\n"
    "    ? hit.distance\n"
    "    : std::numeric_limits<float>::max();\n"
    "  return colour;\n"
    "}\n"
)

# Exact lower tracing remains available for explicit picking only.
replace(
    "src/engine/world/World.cpp",
    "    const std::size_t index = activeLevelIndex_ - depth;\n"
    "    const Vec3 offset = levelOffsetInActiveView(index);\n"
    "    const Ray localRay{ray.origin - offset, ray.direction};\n"
    "    SceneSurfaceHit localHit;\n"
    "    if (!levels_[index]->traceEnvironment(localRay, localHit)) continue;\n",
    "    const std::size_t index = activeLevelIndex_ - depth;\n"
    "    const Vec3 offset = levelOffsetInActiveView(index);\n"
    "    const Ray localRay{ray.origin - offset, ray.direction};\n"
    "    // This exact lower-level trace is input-time only, never part of frame rendering.\n"
    "    levels_[index]->prepareRenderFrame(ray.direction);\n"
    "    SceneSurfaceHit localHit;\n"
    "    if (!levels_[index]->traceEnvironment(localRay, localHit)) continue;\n"
)

# Preview levels no longer become heavyweight render-resident.
replace(
    "src/engine/world/World.cpp",
    "void World::updateLevelResidency() {\n"
    "  for (std::size_t index = 0; index < levels_.size(); ++index) {\n"
    "    const bool previewResident =\n"
    "      index <= activeLevelIndex_ &&\n"
    "      activeLevelIndex_ - index <= lowerLevelPreviewDepth_;\n"
    "    levels_[index]->setResident(previewResident);\n"
    "  }\n"
    "}\n",
    "void World::updateLevelResidency() {\n"
    "  // Residency means full render resources. Decorative lower previews use only\n"
    "  // room topology/palette, so they must not keep textures or heavy level assets loaded.\n"
    "  for (std::size_t index = 0; index < levels_.size(); ++index) {\n"
    "    levels_[index]->setResident(index == activeLevelIndex_);\n"
    "  }\n"
    "}\n"
)

# Only active level gets full render-frame preparation. Build cheap preview dynamic data separately.
replace(
    "src/engine/world/World.cpp",
    "  // Resident level presentation receives the same camera direction used by\n"
    "  // the renderer before either static sampling or runtime compositing begins.\n"
    "  for (const auto& level : levels_) {\n"
    "    if (level && level->isResident()) level->prepareRenderFrame(viewDirection);\n"
    "  }\n\n"
    "  runtimeRenderEntries_.clear();\n"
    "  destinationFeedbackMarkers_.clear();\n",
    "  // Only the active level receives full renderer preparation. Lower previews\n"
    "  // are flat analytic floor plans and bypass lighting, shadows and wall ray work.\n"
    "  activeLevel().prepareRenderFrame(viewDirection);\n\n"
    "  runtimeRenderEntries_.clear();\n"
    "  destinationFeedbackMarkers_.clear();\n"
    "  lowDetailPreviewCharacters_.clear();\n"
    "  lowDetailPreviewMarkers_.clear();\n"
)

# Split destination feedback into full active-level and flat lower-preview markers.
old = """    std::size_t destinationLevelIndex = levels_.size();
    if (
      character->moving &&
      character->movement.hasDestination &&
      previewLevelVisible(character->movement.destination.levelId, destinationLevelIndex)
    ) {
      Vec3 forward = character->movement.destinationForward;
      float magnitudeSquared = forward.x * forward.x + forward.y * forward.y;
      if (magnitudeSquared <= 1e-12f) {
        forward = character->forward;
        magnitudeSquared = forward.x * forward.x + forward.y * forward.y;
      }
      if (magnitudeSquared <= 1e-12f) {
        forward = {0.0f, 1.0f, 0.0f};
      } else {
        const float inverseMagnitude = 1.0f / std::sqrt(magnitudeSquared);
        forward = {forward.x * inverseMagnitude, forward.y * inverseMagnitude, 0.0f};
      }

      DestinationFeedbackMarker marker;
      marker.position = character->movement.destination.position +
        levelOffsetInActiveView(destinationLevelIndex);
      marker.forward = forward;
      marker.right = {forward.y, -forward.x, 0.0f};
      marker.minimumX = character->hitBox.minimum.x;
      marker.maximumX = character->hitBox.maximum.x;
      marker.minimumY = character->hitBox.minimum.y;
      marker.maximumY = character->hitBox.maximum.y;
      marker.floorZ = marker.position.z + character->hitBox.minimum.z;
      marker.elapsedSeconds = character->movement.feedbackElapsedSeconds;
      destinationFeedbackMarkers_.push_back(marker);
    }
"""
new = """    std::size_t destinationLevelIndex = levels_.size();
    if (
      character->moving &&
      character->movement.hasDestination &&
      previewLevelVisible(character->movement.destination.levelId, destinationLevelIndex)
    ) {
      Vec3 forward = character->movement.destinationForward;
      float magnitudeSquared = forward.x * forward.x + forward.y * forward.y;
      if (magnitudeSquared <= 1e-12f) {
        forward = character->forward;
        magnitudeSquared = forward.x * forward.x + forward.y * forward.y;
      }
      if (magnitudeSquared <= 1e-12f) {
        forward = {0.0f, 1.0f, 0.0f};
      } else {
        const float inverseMagnitude = 1.0f / std::sqrt(magnitudeSquared);
        forward = {forward.x * inverseMagnitude, forward.y * inverseMagnitude, 0.0f};
      }

      if (destinationLevelIndex == activeLevelIndex_) {
        DestinationFeedbackMarker marker;
        marker.position = character->movement.destination.position;
        marker.forward = forward;
        marker.right = {forward.y, -forward.x, 0.0f};
        marker.minimumX = character->hitBox.minimum.x;
        marker.maximumX = character->hitBox.maximum.x;
        marker.minimumY = character->hitBox.minimum.y;
        marker.maximumY = character->hitBox.maximum.y;
        marker.floorZ = marker.position.z + character->hitBox.minimum.z;
        marker.elapsedSeconds = character->movement.feedbackElapsedSeconds;
        destinationFeedbackMarkers_.push_back(marker);
      } else {
        LowDetailPreviewMarker marker;
        marker.levelIndex = destinationLevelIndex;
        marker.position = character->movement.destination.position;
        marker.forward = forward;
        marker.right = {forward.y, -forward.x, 0.0f};
        marker.minimumX = character->hitBox.minimum.x;
        marker.maximumX = character->hitBox.maximum.x;
        marker.minimumY = character->hitBox.minimum.y;
        marker.maximumY = character->hitBox.maximum.y;
        marker.elapsedSeconds = character->movement.feedbackElapsedSeconds;
        lowDetailPreviewMarkers_.push_back(marker);
      }
    }
"""
replace("src/engine/world/World.cpp", old, new)

# Lower ordinary characters become flat preview footprints, not full runtime ray-tested entities.
replace(
    "src/engine/world/World.cpp",
    "    Vec3 renderPosition;\n"
    "    if (!renderPositionFor(*character, renderPosition)) continue;\n\n"
    "    RuntimeRenderEntry entry;\n",
    "    Vec3 renderPosition;\n"
    "    if (!renderPositionFor(*character, renderPosition)) continue;\n\n"
    "    const std::size_t characterLevelIndex = levelIndex(character->location.levelId);\n"
    "    if (\n"
    "      character->location.liminalObjectId.empty() &&\n"
    "      characterLevelIndex < activeLevelIndex_ &&\n"
    "      activeLevelIndex_ - characterLevelIndex <= lowerLevelPreviewDepth_\n"
    "    ) {\n"
    "      LowDetailPreviewCharacter preview;\n"
    "      preview.levelIndex = characterLevelIndex;\n"
    "      preview.position = character->location.position;\n"
    "      Vec3 forward = character->forward;\n"
    "      const float magnitudeSquared = forward.x * forward.x + forward.y * forward.y;\n"
    "      if (magnitudeSquared > 1e-12f) {\n"
    "        const float inverseMagnitude = 1.0f / std::sqrt(magnitudeSquared);\n"
    "        forward = {forward.x * inverseMagnitude, forward.y * inverseMagnitude, 0.0f};\n"
    "      } else {\n"
    "        forward = {0.0f, 1.0f, 0.0f};\n"
    "      }\n"
    "      preview.forward = forward;\n"
    "      preview.right = {forward.y, -forward.x, 0.0f};\n"
    "      preview.minimumX = character->hitBox.minimum.x;\n"
    "      preview.maximumX = character->hitBox.maximum.x;\n"
    "      preview.minimumY = character->hitBox.minimum.y;\n"
    "      preview.maximumY = character->hitBox.maximum.y;\n"
    "      preview.selected = characterSystem_ && characterSystem_->isSelected(character->id);\n"
    "      lowDetailPreviewCharacters_.push_back(preview);\n"
    "      continue;\n"
    "    }\n\n"
    "    RuntimeRenderEntry entry;\n"
)

# Cheap analytic preview sampler: plane + room rectangles + wall bands + flat dynamic footprints.
marker = "Vec3 World::sample(const Ray& ray, float backgroundY) const {\n"
implementation = r'''bool World::sampleLowDetailLowerPreview(const Ray& ray, Vec3& colour) const {
  if (lowerLevelPreviewDepth_ == 0 || activeLevelIndex_ == 0) return false;
  if (std::fabs(ray.direction.z) < 1e-7f) return false;

  std::size_t visibleLevelIndex = levels_.size();
  Vec3 visibleLocalPoint;
  const RoomLayout* visibleLayout = nullptr;

  // Higher preview levels win. Every candidate is only an analytic floor-plane
  // intersection plus rectangle containment. No lower scene traversal occurs.
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
      const float t = (planeZ - ray.origin.z) / ray.direction.z;
      if (t <= 0.001f) continue;
      const Vec3 localPoint = ray.origin + ray.direction * t - offset;
      if (!room.containsXY(localPoint.x, localPoint.y, 0.0015f)) continue;
      visibleLevelIndex = index;
      visibleLocalPoint = localPoint;
      visibleLayout = layout;
      break;
    }
    if (visibleLayout) break;
  }

  if (!visibleLayout) return false;

  const int checkerX = static_cast<int>(std::floor(visibleLocalPoint.x + 20.0f));
  const int checkerY = static_cast<int>(std::floor(visibleLocalPoint.y + 20.0f));
  colour = ((checkerX + checkerY) & 1)
    ? visibleLayout->previewFloorDark
    : visibleLayout->previewFloorLight;

  const auto portalOpenAt = [&](const Room& room, RoomSide side, float along) {
    for (const RoomConnection& connection : visibleLayout->connections) {
      if (!connection.openPassage) continue;
      const RoomPortal* portal = nullptr;
      if (connection.a.roomId == room.id && connection.a.side == side) portal = &connection.a;
      if (connection.b.roomId == room.id && connection.b.side == side) portal = &connection.b;
      if (!portal) continue;
      if (std::fabs(along - portal->offset) <= std::max(0.0f, portal->width) * 0.5f) return true;
    }
    return false;
  };

  // Preview walls are floor-plan bands only. Full 3-D walls and Sims-style
  // cutaways remain an active-level feature.
  for (const Room& room : visibleLayout->rooms) {
    if (!room.containsXY(visibleLocalPoint.x, visibleLocalPoint.y, 0.0015f)) continue;
    const float localX = visibleLocalPoint.x - room.centre.x;
    const float localY = visibleLocalPoint.y - room.centre.y;
    const float halfWidth = room.width * 0.5f;
    const float halfDepth = room.depth * 0.5f;
    const float band = std::max(0.02f, visibleLayout->previewWallBand);
    bool wallBand = false;
    if (halfDepth - std::fabs(localY) <= band) {
      const RoomSide side = localY >= 0.0f ? RoomSide::North : RoomSide::South;
      wallBand = !portalOpenAt(room, side, localX);
    }
    if (!wallBand && halfWidth - std::fabs(localX) <= band) {
      const RoomSide side = localX >= 0.0f ? RoomSide::East : RoomSide::West;
      wallBand = !portalOpenAt(room, side, localY);
    }
    if (wallBand) colour = visibleLayout->previewWall;
    break;
  }

  const SelectionStyle style = characterSystem_
    ? characterSystem_->selectionStyle()
    : SelectionStyle();

  for (const LowDetailPreviewMarker& previewMarker : lowDetailPreviewMarkers_) {
    if (previewMarker.levelIndex != visibleLevelIndex) continue;
    const Vec3 delta = visibleLocalPoint - previewMarker.position;
    const float localX = dot(delta, previewMarker.right);
    const float localY = dot(delta, previewMarker.forward);
    if (
      localX < previewMarker.minimumX || localX > previewMarker.maximumX ||
      localY < previewMarker.minimumY || localY > previewMarker.maximumY
    ) continue;
    const float pulse = 0.5f + 0.5f * std::sin(
      previewMarker.elapsedSeconds * 6.28318530717958647692f * 1.75f
    );
    const float alpha = std::max(0.15f, std::min(0.58f, 0.18f + 0.40f * pulse));
    colour = style.tint * alpha + colour * (1.0f - alpha);
  }

  for (const LowDetailPreviewCharacter& previewCharacter : lowDetailPreviewCharacters_) {
    if (previewCharacter.levelIndex != visibleLevelIndex) continue;
    const Vec3 delta = visibleLocalPoint - previewCharacter.position;
    const float localX = dot(delta, previewCharacter.right);
    const float localY = dot(delta, previewCharacter.forward);
    if (
      localX < previewCharacter.minimumX || localX > previewCharacter.maximumX ||
      localY < previewCharacter.minimumY || localY > previewCharacter.maximumY
    ) continue;
    Vec3 characterColour(0.82f, 0.84f, 0.88f);
    if (previewCharacter.selected) characterColour = applyTint(characterColour, style);
    colour = characterColour;
  }

  return true;
}

'''
replace("src/engine/world/World.cpp", marker, implementation + marker)

# Any non-Renderer caller still gets the cheap preview, never the old lower ray trace.
replace(
    "src/engine/world/World.cpp",
    "Vec3 World::sample(const Ray& ray, float backgroundY) const {\n"
    "  SceneSurfaceHit environmentHit;\n"
    "  const Vec3 environmentColour = sampleVisibleEnvironment(ray, backgroundY, environmentHit);\n"
    "  const float environmentHitDistance = environmentHit.found\n"
    "    ? environmentHit.distance\n"
    "    : std::numeric_limits<float>::max();\n",
    "Vec3 World::sample(const Ray& ray, float backgroundY) const {\n"
    "  SceneSurfaceHit environmentHit;\n"
    "  Vec3 environmentColour = activeLevel().sampleWithHit(ray, backgroundY, environmentHit);\n"
    "  if (!environmentHit.found) {\n"
    "    Vec3 previewColour;\n"
    "    if (sampleLowDetailLowerPreview(ray, previewColour)) environmentColour = previewColour;\n"
    "  }\n"
    "  const float environmentHitDistance = environmentHit.found\n"
    "    ? environmentHit.distance\n"
    "    : std::numeric_limits<float>::max();\n"
)

# Demo palette and 1/4 linear preview resolution.
replace(
    "src/demo/DemoWorld.cpp",
    "  level.floorDark = LOWER_FLOOR_DARK;\n  level.floorLight = LOWER_FLOOR_LIGHT;\n",
    "  level.floorDark = LOWER_FLOOR_DARK;\n  level.floorLight = LOWER_FLOOR_LIGHT;\n"
    "  level.roomLayout.previewFloorDark = LOWER_FLOOR_DARK;\n"
    "  level.roomLayout.previewFloorLight = LOWER_FLOOR_LIGHT;\n"
    "  level.roomLayout.previewWall = LOWER_FLOOR_DARK * 0.68f;\n"
)
replace(
    "src/demo/DemoWorld.cpp",
    "  level.floorDark = MIDDLE_FLOOR_DARK;\n  level.floorLight = MIDDLE_FLOOR_LIGHT;\n",
    "  level.floorDark = MIDDLE_FLOOR_DARK;\n  level.floorLight = MIDDLE_FLOOR_LIGHT;\n"
    "  level.roomLayout.previewFloorDark = MIDDLE_FLOOR_DARK;\n"
    "  level.roomLayout.previewFloorLight = MIDDLE_FLOOR_LIGHT;\n"
    "  level.roomLayout.previewWall = MIDDLE_FLOOR_DARK * 0.68f;\n"
)
replace(
    "src/demo/DemoWorld.cpp",
    "  level.floorDark = UPPER_FLOOR_DARK;\n  level.floorLight = UPPER_FLOOR_LIGHT;\n",
    "  level.floorDark = UPPER_FLOOR_DARK;\n  level.floorLight = UPPER_FLOOR_LIGHT;\n"
    "  level.roomLayout.previewFloorDark = UPPER_FLOOR_DARK;\n"
    "  level.roomLayout.previewFloorLight = UPPER_FLOOR_LIGHT;\n"
    "  level.roomLayout.previewWall = UPPER_FLOOR_DARK * 0.68f;\n"
)
replace(
    "src/demo/DemoWorld.cpp",
    "  setLowerLevelPreviewDepth(2);\n  setNavigationLinks({",
    "  setLowerLevelPreviewDepth(2);\n  setLowerPreviewResolutionScale(0.25f);\n  setNavigationLinks({"
)

# Renderer low-resolution preview buffer.
replace(
    "src/engine/render/Renderer.hpp",
    "  struct StaticSample {\n    Vec3 colour;\n    float environmentDistance = 0.0f;\n  };\n",
    "  struct StaticSample {\n    Vec3 colour;\n    float environmentDistance = 0.0f;\n  };\n\n"
    "  struct PreviewSample {\n    Vec3 colour;\n    bool found = false;\n  };\n"
)
replace(
    "src/engine/render/Renderer.hpp",
    "  std::vector<StaticSample> staticSamples_;\n  std::vector<Vec3> panBackgroundRows_;\n",
    "  std::vector<StaticSample> staticSamples_;\n"
    "  std::vector<PreviewSample> previewSamples_;\n"
    "  int previewWidth_ = 0;\n"
    "  int previewHeight_ = 0;\n"
    "  std::vector<Vec3> panBackgroundRows_;\n"
)
replace(
    "src/engine/render/Renderer.cpp",
    "  world_.prepareRenderFrame(forward);\n\n  const std::size_t pixelCount =",
    "  world_.prepareRenderFrame(forward);\n\n"
    "  // Decorative lower floors are rendered once per frame into a deliberately\n"
    "  // small buffer. This is one analytic sample per preview texel, no supersampling.\n"
    "  previewWidth_ = 0;\n"
    "  previewHeight_ = 0;\n"
    "  if (world_.supportsLowDetailLowerPreview()) {\n"
    "    const float previewScale = std::max(0.0625f, std::min(0.5f, world_.lowerPreviewResolutionScale()));\n"
    "    previewWidth_ = std::max(1, std::min(320, static_cast<int>(std::ceil(frameWidth_ * previewScale))));\n"
    "    previewHeight_ = std::max(1, std::min(180, static_cast<int>(std::ceil(frameHeight_ * previewScale))));\n"
    "    const std::size_t required = static_cast<std::size_t>(previewWidth_) * previewHeight_;\n"
    "    if (previewSamples_.size() != required) previewSamples_.resize(required);\n\n"
    "    const float previewStepX = width / static_cast<float>(previewWidth_);\n"
    "    const float previewStepY = height / static_cast<float>(previewHeight_);\n"
    "    const float previewOriginDistance = rayOriginDistance(bounds, forward);\n"
    "    const Vec3 previewCorner =\n"
    "      focus - forward * previewOriginDistance - right * (width * 0.5f) + up * (height * 0.5f);\n"
    "    const Vec3 previewRightStep = right * previewStepX;\n"
    "    const Vec3 previewDownStep = up * (-previewStepY);\n"
    "    Vec3 previewRow = previewCorner + previewRightStep * 0.5f + previewDownStep * 0.5f;\n"
    "    for (int py = 0; py < previewHeight_; ++py) {\n"
    "      Vec3 previewOrigin = previewRow;\n"
    "      for (int px = 0; px < previewWidth_; ++px) {\n"
    "        PreviewSample& sample = previewSamples_[static_cast<std::size_t>(py) * previewWidth_ + px];\n"
    "        sample.found = world_.sampleLowDetailLowerPreview({previewOrigin, forward}, sample.colour);\n"
    "        previewOrigin = previewOrigin + previewRightStep;\n"
    "      }\n"
    "      previewRow = previewRow + previewDownStep;\n"
    "    }\n"
    "  }\n\n"
    "  const std::size_t pixelCount ="
)

old = """        if (useStaticCache) {
          StaticSample& staticSample = staticSamples_[pixelIndex * 4 + sampleIndex];
          if (rebuildStaticCache || staticSample.environmentDistance < 0.0f) {
            staticSample.colour = world_.sampleEnvironment(
              ray,
              sampleBackgroundY,
              staticSample.environmentDistance
            );
          }
          colour = colour + world_.compositeRuntime(
            ray,
            staticSample.colour,
            staticSample.environmentDistance
          );
        } else {
          colour = colour + world_.sample(ray, sampleBackgroundY);
        }
"""
new = """        Vec3 environmentColour;
        float environmentDistance = std::numeric_limits<float>::max();
        if (useStaticCache) {
          StaticSample& staticSample = staticSamples_[pixelIndex * 4 + sampleIndex];
          if (rebuildStaticCache || staticSample.environmentDistance < 0.0f) {
            staticSample.colour = world_.sampleEnvironment(
              ray,
              sampleBackgroundY,
              staticSample.environmentDistance
            );
          }
          environmentColour = staticSample.colour;
          environmentDistance = staticSample.environmentDistance;
        } else {
          environmentColour = world_.sampleEnvironment(
            ray,
            sampleBackgroundY,
            environmentDistance
          );
        }

        if (
          environmentDistance >= NO_HIT_DISTANCE &&
          previewWidth_ > 0 && previewHeight_ > 0
        ) {
          const int previewX = std::min(previewWidth_ - 1, x * previewWidth_ / frameWidth_);
          const int previewY = std::min(previewHeight_ - 1, y * previewHeight_ / frameHeight_);
          const PreviewSample& preview = previewSamples_[
            static_cast<std::size_t>(previewY) * previewWidth_ + previewX
          ];
          if (preview.found) environmentColour = preview.colour;
        }

        colour = colour + world_.compositeRuntime(
          ray,
          environmentColour,
          environmentDistance
        );
"""
replace("src/engine/render/Renderer.cpp", old, new)

# Performance regression: lower preview levels receive zero render traces.
replace(
    "scripts/performance-architecture-smoke.cpp",
    '  std::cout << "Performance architecture smoke test passed: one primary trace, any-hit shadows, cached entity views, dynamic broad phase and fixed-density tiled UVs.\\n";\n  return 0;\n}\n',
    '''  std::unique_ptr<CountingLevel> previewLowerOwned(new CountingLevel());
  std::unique_ptr<CountingLevel> previewUpperOwned(new CountingLevel());
  CountingLevel* previewLower = previewLowerOwned.get();
  CountingLevel* previewUpper = previewUpperOwned.get();
  std::vector<std::unique_ptr<IWorldLevel>> previewLevels;
  previewLevels.push_back(std::move(previewLowerOwned));
  previewLevels.push_back(std::move(previewUpperOwned));
  World previewWorld(std::move(previewLevels), 1);
  previewWorld.setLevelId(0, "preview-lower");
  previewWorld.setLevelId(1, "preview-upper");
  previewWorld.setLowerLevelPreviewDepth(1);
  float previewDistance = 0.0f;
  previewWorld.sampleEnvironment(
    {{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
    0.5f,
    previewDistance
  );
  if (previewUpper->combinedSampleCalls != 1) return 11;
  if (previewLower->combinedSampleCalls != 0 || previewLower->traceCalls != 0) return 12;
  if (previewWorld.residentLevelCount() != 1 || !previewWorld.isLevelResident("preview-upper")) return 13;

  std::cout << "Performance architecture smoke test passed: one active-level trace, zero lower-preview render traces, active-only residency, any-hit shadows, cached entity views, dynamic broad phase and fixed-density tiled UVs.\\n";
  return 0;
}
'''
)

# Existing lower-preview Character/marker regression now exercises the cheap sampler directly.
start = '''  const Vec3 previewDirection = normalisedHorizontal({1.0f, 1.0f, 0.0f}) * 0.81649658f +
    Vec3(0.0f, 0.0f, -0.57735027f);
  auto previewSample = [&](const Vec3& activeViewPoint) {
    const Ray ray{activeViewPoint - previewDirection * 24.0f, previewDirection};
    float environmentDistance = 0.0f;
    SamplePair pair;
    pair.environment = previewWorld.sampleEnvironment(ray, 0.5f, environmentDistance);
    pair.runtime = previewWorld.compositeRuntime(ray, pair.environment, environmentDistance);
    return pair;
  };

  previewWorld.prepareRenderFrame(previewDirection);

  const float lowerPreviewZ = previewWorld.levelViewOrigin("lower").z -
    previewWorld.levelViewOrigin("upper").z;
  // Sample the Character body above the configured lower-floor offset at a
  // screen ray known to be exposed.
  const SamplePair lowerBody = previewSample({-12.0f, 0.0f, lowerPreviewZ + 0.80f});
  require(
    colourDistance(lowerBody.environment, lowerBody.runtime) > 0.03f,
    "Character on second lower preview level was not visible from upper"
  );

  const SamplePair lowerDestinationFeedback = previewSample({-11.0f, -2.0f, lowerPreviewZ});
  require(
    colourDistance(lowerDestinationFeedback.environment, lowerDestinationFeedback.runtime) > 0.02f,
    "destination feedback on second lower preview level was not visible from upper"
  );
'''
replacement = '''  const Vec3 previewDirection = normalisedHorizontal({1.0f, 1.0f, 0.0f}) * 0.81649658f +
    Vec3(0.0f, 0.0f, -0.57735027f);
  previewWorld.prepareRenderFrame(previewDirection);

  const float lowerPreviewZ = previewWorld.levelViewOrigin("lower").z -
    previewWorld.levelViewOrigin("upper").z;
  auto cheapPreview = [&](const Vec3& floorPoint, Vec3& colour) {
    const Ray ray{floorPoint - previewDirection * 24.0f, previewDirection};
    return previewWorld.sampleLowDetailLowerPreview(ray, colour);
  };

  Vec3 lowerBody;
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
replace("scripts/destination-feedback-smoke.cpp", start, replacement)

# Residency expectations: only the active level is fully render-resident now.
liminal = Path("scripts/liminal-lighting-smoke.cpp")
text = liminal.read_text()
text = text.replace("world.residentLevelCount() != 2", "world.residentLevelCount() != 1")
text = text.replace("!world.isLevelResident(\"middle\") || !world.isLevelResident(\"lower\")", "!world.isLevelResident(\"middle\") || world.isLevelResident(\"lower\")")
text = text.replace("world.residentLevelCount() != 3", "world.residentLevelCount() != 1")
text = text.replace("!world.isLevelResident(\"upper\") || !world.isLevelResident(\"middle\") || !world.isLevelResident(\"lower\")", "!world.isLevelResident(\"upper\") || world.isLevelResident(\"middle\") || world.isLevelResident(\"lower\")")
liminal.write_text(text)

geometry = Path("scripts/world-geometry-smoke.cpp")
text = geometry.read_text()
text = text.replace("world.residentLevelCount() != 2 || !world.isLevelResident(\"lower\") || !world.isLevelResident(\"middle\")", "world.residentLevelCount() != 1 || world.isLevelResident(\"lower\") || !world.isLevelResident(\"middle\")")
text = text.replace("world.activeLevelId() != \"upper\" || world.residentLevelCount() != 3", "world.activeLevelId() != \"upper\" || world.residentLevelCount() != 1")
text = text.replace("!world.isLevelResident(\"middle\") || !world.isLevelResident(\"lower\")", "world.isLevelResident(\"middle\") || world.isLevelResident(\"lower\")")
geometry.write_text(text)

print("low-detail preview staging transformations applied")
