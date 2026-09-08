from pathlib import Path

p = Path('src/engine/world/World.cpp')
s = p.read_text()
old = '''bool World::renderPositionFor(const Character& character, Vec3& position) const {
  // Preserve the existing liminal projection when a Character occupies a
  // connector touching the active level.
  if (mapLiminalPosition(character.location, activeLevelId(), position)) return true;

  // Ordinary Characters on resident lower preview levels are translated into
'''
new = '''bool World::renderPositionFor(const Character& character, Vec3& position) const {
  // Liminal Characters are only visible through connector endpoint mapping.
  // Do not reinterpret a Character on an unrelated connector as an ordinary
  // lower-level preview entity merely because its simulation level is resident.
  if (!character.location.liminalObjectId.empty()) {
    return mapLiminalPosition(character.location, activeLevelId(), position);
  }

  // Ordinary Characters on resident lower preview levels are translated into
'''
if s.count(old) != 1:
    raise SystemExit(f'expected one renderPositionFor anchor, found {s.count(old)}')
p.write_text(s.replace(old, new, 1))
