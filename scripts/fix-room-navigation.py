from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}")
    p.write_text(text.replace(old, new, 1))


replace_once(
    "src/demo/DemoWorld.cpp",
    '''  bool overlapsAdditionalStatic(const Object& candidate) const override {
    for (const RoomWallBox& box : roomWalls_) {
      Object wall;
      wall.location = candidate.location;
      wall.location.position = {0.0f, 0.0f, 0.0f};
      wall.hitBox.minimum = box.centre - box.halfExtent;
      wall.hitBox.maximum = box.centre + box.halfExtent;
      wall.solid = true;
      if (wall.overlaps(candidate)) return true;
    }
    return false;
  }
''',
    '''  bool overlapsAdditionalStatic(const Object& candidate) const override {
    // Room boundaries are axis-aligned. Reject distant walls against the
    // candidate's conservative world AABB before paying for exact OBB SAT.
    // Navigation probes collision many times, so this keeps extra rooms from
    // multiplying collision cost for Characters nowhere near their walls.
    Vec3 facing;
    Vec3 right;
    candidate.horizontalBasis(facing, right);
    const Vec3 localCentre = candidate.hitBox.centre();
    const Vec3 localHalf = candidate.hitBox.halfExtent();
    const Vec3 candidateCentre = candidate.location.position +
      right * localCentre.x + facing * localCentre.y + Vec3(0.0f, 0.0f, localCentre.z);
    const Vec3 candidateHalf(
      std::fabs(right.x) * localHalf.x + std::fabs(facing.x) * localHalf.y,
      std::fabs(right.y) * localHalf.x + std::fabs(facing.y) * localHalf.y,
      localHalf.z
    );

    for (const RoomWallBox& box : roomWalls_) {
      if (
        std::fabs(candidateCentre.x - box.centre.x) >= candidateHalf.x + box.halfExtent.x ||
        std::fabs(candidateCentre.y - box.centre.y) >= candidateHalf.y + box.halfExtent.y ||
        std::fabs(candidateCentre.z - box.centre.z) >= candidateHalf.z + box.halfExtent.z
      ) {
        continue;
      }

      Object wall;
      wall.location = candidate.location;
      wall.location.position = {0.0f, 0.0f, 0.0f};
      wall.hitBox.minimum = box.centre - box.halfExtent;
      wall.hitBox.maximum = box.centre + box.halfExtent;
      wall.solid = true;
      if (wall.overlaps(candidate)) return true;
    }
    return false;
  }
'''
)

replace_once(
    "src/engine/character/CharacterPolicies.cpp",
    '''    const Vec3 currentPoint = positions[current];
    for (const auto& direction : directions) {
''',
    '''    const Vec3 currentPoint = positions[current];

    // A quantised XY/Z grid is an acceleration structure, not the destination
    // contract. Changing a level's bounds can shift grid sampling relative to
    // a staircase, so the quantised goal cell may resolve to a nearby stair
    // height even though the exact requested endpoint is physically reachable.
    // Once search reaches the goal neighbourhood, validate the real segment to
    // the exact destination instead of exhaustively searching for one specific
    // quantised cell/height combination.
    const int goalDeltaX = std::abs(goalGrid.x - current.x);
    const int goalDeltaY = std::abs(goalGrid.y - current.y);
    if (goalDeltaX <= 1 && goalDeltaY <= 1) {
      Vec3 exactGoal;
      if (segmentClear(
        world,
        character,
        levelId,
        currentPoint,
        destination,
        defaults,
        &exactGoal
      )) {
        found = true;
        foundGoal = current;
        break;
      }
    }

    for (const auto& direction : directions) {
'''
)

# The old direct-route regression now also indirectly protects against a room
# layout shifting the navigation grid around the existing staircase endpoints.
print("room navigation fixes applied")
