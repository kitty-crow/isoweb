import type { IsowebModule } from '../runtime';

type JsonRecord = Record<string, unknown>;

const DEFAULT_SELECTION_TINT = [0.18, 0.48, 1.0] as const;

function record(value: unknown, name: string): JsonRecord {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`${name} must be an object.`);
  }
  return value as JsonRecord;
}

function finiteNumber(value: unknown, name: string): number {
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new Error(`${name} must be a finite number.`);
  }
  return value;
}

function integer(value: unknown, name: string): number {
  const result = finiteNumber(value, name);
  if (!Number.isInteger(result)) throw new Error(`${name} must be an integer.`);
  return result;
}

function boolean(value: unknown, name: string): boolean {
  if (typeof value !== 'boolean') throw new Error(`${name} must be a boolean.`);
  return value;
}

function selectionTint(value: unknown): [number, number, number] {
  if (value === undefined) return [...DEFAULT_SELECTION_TINT];
  if (!Array.isArray(value) || value.length !== 3) {
    throw new Error('selectionTint must contain exactly three numbers.');
  }
  return [
    finiteNumber(value[0], 'selectionTint[0]'),
    finiteNumber(value[1], 'selectionTint[1]'),
    finiteNumber(value[2], 'selectionTint[2]')
  ];
}

export async function loadDemoState(module: IsowebModule): Promise<void> {
  const stateUrl = new URL('./data/demo-state.json', window.location.href);
  const response = await fetch(stateUrl, { cache: 'no-store' });
  if (!response.ok) {
    throw new Error(`Failed to load demo state (${response.status}).`);
  }

  const root = record(await response.json(), 'demo state');
  const baseMovementSpeed = finiteNumber(root.baseMovementSpeed, 'baseMovementSpeed');
  const characters = root.characters;
  if (!Array.isArray(characters)) throw new Error('characters must be an array.');

  module._isoweb_state_begin(baseMovementSpeed);

  for (const [index, rawCharacter] of characters.entries()) {
    const character = record(rawCharacter, `characters[${index}]`);
    const position = record(character.position, `characters[${index}].position`);
    const forward = record(character.forward, `characters[${index}].forward`);
    const hitbox = record(character.hitbox, `characters[${index}].hitbox`);
    const tint = selectionTint(character.selectionTint);

    module._isoweb_state_add_character(
      finiteNumber(position.x, `characters[${index}].position.x`),
      finiteNumber(position.y, `characters[${index}].position.y`),
      finiteNumber(position.z, `characters[${index}].position.z`),
      finiteNumber(forward.x, `characters[${index}].forward.x`),
      finiteNumber(forward.y, `characters[${index}].forward.y`),
      finiteNumber(hitbox.width, `characters[${index}].hitbox.width`),
      finiteNumber(hitbox.depth, `characters[${index}].hitbox.depth`),
      finiteNumber(hitbox.height, `characters[${index}].hitbox.height`),
      integer(position.level, `characters[${index}].position.level`),
      boolean(character.solid, `characters[${index}].solid`) ? 1 : 0,
      boolean(character.npc, `characters[${index}].npc`) ? 1 : 0,
      boolean(character.controllable, `characters[${index}].controllable`) ? 1 : 0,
      finiteNumber(
        character.movementSpeedMultiplier,
        `characters[${index}].movementSpeedMultiplier`
      ),
      tint[0],
      tint[1],
      tint[2]
    );
  }
}
