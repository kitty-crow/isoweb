import type {
  DynamicBodyEntityDefinition, LevelDocument, WorldBehaviourDefinition
} from '../world/documents';
import { selectedLevel } from './AuthoringCommands';
import { FunctionalCommand, type EditorCommand } from './CommandHistory';
import type { EditorSelection } from './Selection';
import type { EditableSourceProject } from './SourceProjectIO';

export type MotionBehaviourKind = 'none' | 'vertical-cycle' | 'rotation';

function targetEntity(behaviour: WorldBehaviourDefinition): string[] {
  return behaviour.type === 'oscillating-gate'
    ? [behaviour.leftEntity, behaviour.rightEntity]
    : [behaviour.entity];
}

function entity(level: LevelDocument, selection: EditorSelection): DynamicBodyEntityDefinition {
  if (selection.kind !== 'entity') throw new Error('Select a behaviour body first');
  const value = level.entities.find(candidate => candidate.id === selection.id);
  if (!value || !('dynamicBody' in value.components)) {
    throw new Error('Behaviours can only be assigned to a dynamic body');
  }
  return value;
}

function nextId(prefix: string, existing: Iterable<string>): string {
  const used = new Set(existing);
  if (!used.has(prefix)) return prefix;
  for (let suffix = 2; suffix < 1_000_000; ++suffix) {
    const candidate = `${prefix}-${suffix}`;
    if (!used.has(candidate)) return candidate;
  }
  throw new Error(`Unable to allocate behaviour id for ${prefix}`);
}

export function localEntityBehaviours(
  level: LevelDocument,
  entityId: string
): WorldBehaviourDefinition[] {
  return (level.behaviours ?? []).filter(behaviour => targetEntity(behaviour).includes(entityId));
}

export function localMotionBehaviour(
  level: LevelDocument,
  entityId: string
): Extract<WorldBehaviourDefinition, { type: 'vertical-cycle' | 'rotation' }> | undefined {
  return (level.behaviours ?? []).find(
    behaviour =>
      (behaviour.type === 'vertical-cycle' || behaviour.type === 'rotation') &&
      behaviour.entity === entityId
  ) as Extract<WorldBehaviourDefinition, { type: 'vertical-cycle' | 'rotation' }> | undefined;
}

export function localHazardBehaviour(
  level: LevelDocument,
  entityId: string
): Extract<WorldBehaviourDefinition, { type: 'hazard' }> | undefined {
  return (level.behaviours ?? []).find(
    behaviour => behaviour.type === 'hazard' && behaviour.entity === entityId
  ) as Extract<WorldBehaviourDefinition, { type: 'hazard' }> | undefined;
}

export function createSetMotionBehaviourCommand(
  project: EditableSourceProject,
  selection: EditorSelection,
  kind: MotionBehaviourKind
): EditorCommand {
  const level = selectedLevel(project, selection);
  const body = entity(level, selection);
  const before = [...(level.behaviours ?? [])];
  const retained = before.filter(
    behaviour =>
      !((behaviour.type === 'vertical-cycle' || behaviour.type === 'rotation') &&
        behaviour.entity === body.id)
  );

  let replacement: WorldBehaviourDefinition | undefined;
  if (kind === 'vertical-cycle') {
    replacement = {
      id: nextId(`${body.id}-vertical`, retained.map(value => value.id)),
      type: 'vertical-cycle',
      entity: body.id,
      base: [...body.components.transform.position],
      upZ: 2,
      downZ: 0,
      period: 3,
      blockOnSafeContact: false
    };
  } else if (kind === 'rotation') {
    replacement = {
      id: nextId(`${body.id}-rotation`, retained.map(value => value.id)),
      type: 'rotation',
      entity: body.id,
      angularSpeed: 1,
      directionMultiplier: 1
    };
  }

  const after = replacement ? [...retained, replacement] : retained;
  return new FunctionalCommand(
    'set body motion',
    () => { level.behaviours = after; },
    () => { level.behaviours = before; }
  );
}

export function createSetHazardCommand(
  project: EditableSourceProject,
  selection: EditorSelection,
  enabled: boolean
): EditorCommand {
  const level = selectedLevel(project, selection);
  const body = entity(level, selection);
  const before = [...(level.behaviours ?? [])];
  const retained = before.filter(
    behaviour => !(behaviour.type === 'hazard' && behaviour.entity === body.id)
  );
  const after = enabled
    ? [
        ...retained,
        {
          id: nextId(`${body.id}-hazard`, retained.map(value => value.id)),
          type: 'hazard' as const,
          entity: body.id,
          face: 'any' as const,
          tolerance: 0.028,
          action: 'respawn' as const
        }
      ]
    : retained;

  return new FunctionalCommand(
    enabled ? 'enable body hazard' : 'disable body hazard',
    () => { level.behaviours = after; },
    () => { level.behaviours = before; }
  );
}
