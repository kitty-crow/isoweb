import type { Vec3Tuple, WorldConnectorDefinition } from '../world/documents';
import { FunctionalCommand, type EditorCommand } from './CommandHistory';
import type { EditableWorldProject } from './SourceProjectIO';
import { effectiveLevelOrigin } from './WorldPlacementManager';

export type WorldConnectorOperation = {
  command: EditorCommand;
  connectorId: string;
};

function nextId(prefix: string, existing: Iterable<string>): string {
  const used = new Set(existing);
  if (!used.has(prefix)) return prefix;
  for (let suffix = 2; suffix < 1_000_000; ++suffix) {
    const candidate = `${prefix}-${suffix}`;
    if (!used.has(candidate)) return candidate;
  }
  throw new Error(`Unable to allocate connector id for ${prefix}`);
}

function levelAnchor(project: EditableWorldProject, levelId: string): Vec3Tuple {
  const level = project.levels.find(candidate => candidate.id === levelId);
  if (!level) throw new Error(`Level ${levelId} does not exist`);
  return [...level.settings.boundsFocus] as Vec3Tuple;
}

function add(a: Vec3Tuple, b: Vec3Tuple): Vec3Tuple {
  return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

function subtract(a: Vec3Tuple, b: Vec3Tuple): Vec3Tuple {
  return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
}

function lerp(a: Vec3Tuple, b: Vec3Tuple, t: number): Vec3Tuple {
  return [
    a[0] + (b[0] - a[0]) * t,
    a[1] + (b[1] - a[1]) * t,
    a[2] + (b[2] - a[2]) * t
  ];
}

export function createSetDefaultLevelCommand(
  project: EditableWorldProject,
  levelId: string
): EditorCommand {
  if (!project.levels.some(level => level.id === levelId)) {
    throw new Error(`Level ${levelId} does not exist`);
  }
  const before = project.document.settings.defaultLevel;
  return new FunctionalCommand(
    'set start level',
    () => { project.document.settings.defaultLevel = levelId; },
    () => { project.document.settings.defaultLevel = before; }
  );
}

export function createWorldConnectionOperation(
  project: EditableWorldProject,
  fromLevel: string,
  toLevel: string,
  type: 'portal' | 'stairs'
): WorldConnectorOperation {
  if (fromLevel === toLevel) throw new Error('A world connection must link two different levels');
  const levels = new Set(project.levels.map(level => level.id));
  if (!levels.has(fromLevel) || !levels.has(toLevel)) {
    throw new Error('Connection endpoints must reference existing levels');
  }

  project.document.connectors ??= [];
  const connectorId = nextId(
    `${fromLevel}-${toLevel}-${type}`,
    project.document.connectors.map(connector => connector.id)
  );

  const fromPosition = levelAnchor(project, fromLevel);
  const toPosition = levelAnchor(project, toLevel);
  let forwardTraversal: Vec3Tuple[] = [];
  let reverseTraversal: Vec3Tuple[] = [];

  if (type === 'stairs') {
    const fromOrigin = effectiveLevelOrigin(project, fromLevel);
    const toOrigin = effectiveLevelOrigin(project, toLevel);
    const worldFrom = add(fromOrigin, fromPosition);
    const worldTo = add(toOrigin, toPosition);
    const physicalSamples: Vec3Tuple[] = [];
    const sampleCount = 10;
    for (let index = 1; index <= sampleCount; ++index) {
      physicalSamples.push(lerp(worldFrom, worldTo, index / (sampleCount + 1)));
    }
    forwardTraversal = physicalSamples.map(point => subtract(point, fromOrigin));
    reverseTraversal = [...physicalSamples].reverse().map(point => subtract(point, toOrigin));
  }

  const connector: WorldConnectorDefinition = {
    id: connectorId,
    type,
    fromLevel,
    toLevel,
    fromPosition,
    toPosition,
    forwardTraversal,
    reverseTraversal,
    bidirectional: true
  };

  return {
    connectorId,
    command: new FunctionalCommand(
      `add world ${type}`,
      () => {
        project.document.connectors ??= [];
        if (project.document.connectors.some(candidate => candidate.id === connector.id)) {
          throw new Error(`World connector ${connector.id} already exists`);
        }
        project.document.connectors.push(connector);
      },
      () => {
        const connectors = project.document.connectors ?? [];
        const index = connectors.findIndex(candidate => candidate.id === connector.id);
        if (index >= 0) connectors.splice(index, 1);
      }
    )
  };
}

export function createWorldPortalOperation(
  project: EditableWorldProject,
  fromLevel: string,
  toLevel: string
): WorldConnectorOperation {
  return createWorldConnectionOperation(project, fromLevel, toLevel, 'portal');
}

export function createDeleteWorldConnectorCommand(
  project: EditableWorldProject,
  connectorId: string
): EditorCommand {
  const connectors = project.document.connectors ?? [];
  const index = connectors.findIndex(connector => connector.id === connectorId);
  if (index < 0) throw new Error(`World connector ${connectorId} does not exist`);
  const connector = connectors[index];

  return new FunctionalCommand(
    'delete world connector',
    () => {
      const current = (project.document.connectors ?? [])
        .findIndex(candidate => candidate.id === connectorId);
      if (current >= 0) project.document.connectors!.splice(current, 1);
    },
    () => {
      project.document.connectors ??= [];
      if (!project.document.connectors.some(candidate => candidate.id === connectorId)) {
        project.document.connectors.splice(
          Math.min(index, project.document.connectors.length),
          0,
          connector
        );
      }
    }
  );
}
