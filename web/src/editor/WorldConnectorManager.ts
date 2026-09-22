import type { Vec3Tuple, WorldConnectorDefinition } from '../world/documents';
import { FunctionalCommand, type EditorCommand } from './CommandHistory';
import type { EditableWorldProject } from './SourceProjectIO';

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

function levelOrigin(project: EditableWorldProject, levelId: string): Vec3Tuple {
  const level = project.levels.find(candidate => candidate.id === levelId);
  if (!level) throw new Error(`Level ${levelId} does not exist`);
  return [...level.viewOrigin] as Vec3Tuple;
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

export function createWorldPortalOperation(
  project: EditableWorldProject,
  fromLevel: string,
  toLevel: string
): WorldConnectorOperation {
  if (fromLevel === toLevel) throw new Error('A portal must connect two different levels');
  const levels = new Set(project.levels.map(level => level.id));
  if (!levels.has(fromLevel) || !levels.has(toLevel)) {
    throw new Error('Portal endpoints must reference existing levels');
  }

  project.document.connectors ??= [];
  const connectorId = nextId(
    `${fromLevel}-${toLevel}`,
    project.document.connectors.map(connector => connector.id)
  );
  const connector: WorldConnectorDefinition = {
    id: connectorId,
    type: 'portal',
    fromLevel,
    toLevel,
    fromPosition: levelOrigin(project, fromLevel),
    toPosition: levelOrigin(project, toLevel),
    forwardTraversal: [],
    reverseTraversal: [],
    bidirectional: true
  };

  return {
    connectorId,
    command: new FunctionalCommand(
      'add world portal',
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
