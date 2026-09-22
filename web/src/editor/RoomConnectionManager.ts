import type {
  LevelDocument, RoomConnectionDefinition, RoomDefinition, RoomSide
} from '../world/documents';
import { FunctionalCommand, type EditorCommand } from './CommandHistory';
import type { EditableSourceProject } from './SourceProjectIO';
import { selectedLevel } from './AuthoringCommands';
import type { EditorSelection } from './Selection';

export type RoomConnectionOperation = {
  command: EditorCommand;
  selection: EditorSelection;
};

function nextId(prefix: string, existing: Iterable<string>): string {
  const used = new Set(existing);
  if (!used.has(prefix)) return prefix;
  for (let suffix = 2; suffix < 1_000_000; ++suffix) {
    const candidate = `${prefix}-${suffix}`;
    if (!used.has(candidate)) return candidate;
  }
  throw new Error(`Unable to allocate room connection id for ${prefix}`);
}

function room(level: LevelDocument, id: string): RoomDefinition {
  const value = level.rooms?.find(candidate => candidate.id === id);
  if (!value) throw new Error(`Room ${id} does not exist in level ${level.id}`);
  return value;
}

function clampOffset(offset: number, span: number, width: number): number {
  const allowance = Math.max(0, span / 2 - width / 2);
  return Math.max(-allowance, Math.min(allowance, offset));
}

function portalPair(
  from: RoomDefinition,
  to: RoomDefinition
): {
  a: { roomId: string; side: RoomSide; offset: number; width: number };
  b: { roomId: string; side: RoomSide; offset: number; width: number };
} {
  const dx = to.centre[0] - from.centre[0];
  const dy = to.centre[1] - from.centre[1];

  if (Math.abs(dx) >= Math.abs(dy)) {
    const aSide: RoomSide = dx >= 0 ? 'east' : 'west';
    const bSide: RoomSide = dx >= 0 ? 'west' : 'east';
    const width = Math.max(0.25, Math.min(1, from.depth * 0.8, to.depth * 0.8));
    return {
      a: {
        roomId: from.id,
        side: aSide,
        offset: clampOffset(to.centre[1] - from.centre[1], from.depth, width),
        width
      },
      b: {
        roomId: to.id,
        side: bSide,
        offset: clampOffset(from.centre[1] - to.centre[1], to.depth, width),
        width
      }
    };
  }

  const aSide: RoomSide = dy >= 0 ? 'north' : 'south';
  const bSide: RoomSide = dy >= 0 ? 'south' : 'north';
  const width = Math.max(0.25, Math.min(1, from.width * 0.8, to.width * 0.8));
  return {
    a: {
      roomId: from.id,
      side: aSide,
      offset: clampOffset(to.centre[0] - from.centre[0], from.width, width),
      width
    },
    b: {
      roomId: to.id,
      side: bSide,
      offset: clampOffset(from.centre[0] - to.centre[0], to.width, width),
      width
    }
  };
}

export function createRoomConnectionOperation(
  project: EditableSourceProject,
  selection: EditorSelection | null,
  fromRoomId: string,
  toRoomId: string
): RoomConnectionOperation {
  if (fromRoomId === toRoomId) throw new Error('A room opening must connect two different rooms');
  const level = selectedLevel(project, selection);
  const from = room(level, fromRoomId);
  const to = room(level, toRoomId);
  level.roomConnections ??= [];

  const duplicate = level.roomConnections.find(connection =>
    (connection.a.roomId === fromRoomId && connection.b.roomId === toRoomId) ||
    (connection.a.roomId === toRoomId && connection.b.roomId === fromRoomId)
  );
  if (duplicate) {
    throw new Error(`Rooms ${fromRoomId} and ${toRoomId} are already connected by ${duplicate.id}`);
  }

  const id = nextId(
    `${fromRoomId}-${toRoomId}`,
    level.roomConnections.map(connection => connection.id)
  );
  const portals = portalPair(from, to);
  const connection: RoomConnectionDefinition = {
    id,
    a: portals.a,
    b: portals.b,
    openPassage: true
  };

  return {
    selection: { kind: 'room-connection', id, levelId: level.id },
    command: new FunctionalCommand(
      'connect rooms',
      () => {
        level.roomConnections ??= [];
        if (level.roomConnections.some(candidate => candidate.id === id)) {
          throw new Error(`Room connection ${id} already exists`);
        }
        level.roomConnections.push(connection);
      },
      () => {
        const index = level.roomConnections?.findIndex(candidate => candidate.id === id) ?? -1;
        if (index >= 0) level.roomConnections!.splice(index, 1);
      }
    )
  };
}

export function createDeleteRoomConnectionCommand(
  project: EditableSourceProject,
  selection: EditorSelection
): EditorCommand {
  const level = selectedLevel(project, selection);
  const connections = level.roomConnections ?? [];
  const index = connections.findIndex(candidate => candidate.id === selection.id);
  if (index < 0) throw new Error(`Room connection ${selection.id} does not exist`);
  const connection = connections[index];

  return new FunctionalCommand(
    'delete room connection',
    () => {
      const current = level.roomConnections?.findIndex(candidate => candidate.id === connection.id) ?? -1;
      if (current >= 0) level.roomConnections!.splice(current, 1);
    },
    () => {
      level.roomConnections ??= [];
      if (!level.roomConnections.some(candidate => candidate.id === connection.id)) {
        level.roomConnections.splice(
          Math.min(index, level.roomConnections.length),
          0,
          connection
        );
      }
    }
  );
}
