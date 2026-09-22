import type {
  Vec3Tuple, WorldLevelPlacement, WorldLevelReference
} from '../world/documents';
import { FunctionalCommand, type EditorCommand } from './CommandHistory';
import type { EditableWorldProject } from './SourceProjectIO';

export type QuarterTurn = 0 | 1 | 2 | 3;

function reference(project: EditableWorldProject, levelId: string): WorldLevelReference {
  const value = project.document.levels.find(candidate => candidate.id === levelId);
  if (!value) throw new Error(`World level ${levelId} does not exist`);
  return value;
}

function normaliseQuarterTurns(value: number): QuarterTurn {
  return (((Math.trunc(value) % 4) + 4) % 4) as QuarterTurn;
}

export function levelPlacement(
  project: EditableWorldProject,
  levelId: string
): Vec3Tuple {
  return [...(reference(project, levelId).placement?.position ?? [0, 0, 0])] as Vec3Tuple;
}

export function levelQuarterTurns(
  project: EditableWorldProject,
  levelId: string
): QuarterTurn {
  return normaliseQuarterTurns(reference(project, levelId).placement?.quarterTurns ?? 0);
}

export function rotateLocalPoint(point: Vec3Tuple, quarterTurns: number): Vec3Tuple {
  const q = normaliseQuarterTurns(quarterTurns);
  switch (q) {
    case 1: return [-point[1], point[0], point[2]];
    case 2: return [-point[0], -point[1], point[2]];
    case 3: return [point[1], -point[0], point[2]];
    default: return [...point] as Vec3Tuple;
  }
}

export function effectiveLevelOrigin(
  project: EditableWorldProject,
  levelId: string
): Vec3Tuple {
  const level = project.levels.find(candidate => candidate.id === levelId);
  if (!level) throw new Error(`World level ${levelId} does not exist`);
  const placement = levelPlacement(project, levelId);
  return [
    level.viewOrigin[0] + placement[0],
    level.viewOrigin[1] + placement[1],
    level.viewOrigin[2] + placement[2]
  ];
}

export function worldPointForLevel(
  project: EditableWorldProject,
  levelId: string,
  localPoint: Vec3Tuple
): Vec3Tuple {
  const origin = effectiveLevelOrigin(project, levelId);
  const rotated = rotateLocalPoint(localPoint, levelQuarterTurns(project, levelId));
  return [
    origin[0] + rotated[0],
    origin[1] + rotated[1],
    origin[2] + rotated[2]
  ];
}

function placementSnapshot(
  project: EditableWorldProject,
  levelId: string
): Required<Pick<WorldLevelPlacement, 'position'>> & { quarterTurns: QuarterTurn } {
  return {
    position: levelPlacement(project, levelId),
    quarterTurns: levelQuarterTurns(project, levelId)
  };
}

function assignPlacement(
  target: WorldLevelReference,
  value: { position: Vec3Tuple; quarterTurns: QuarterTurn }
): void {
  target.placement = {
    position: [...value.position] as Vec3Tuple,
    quarterTurns: value.quarterTurns
  };
}

export function createSetLevelPlacementCommand(
  project: EditableWorldProject,
  levelId: string,
  next: Vec3Tuple,
  label = 'place level'
): EditorCommand {
  const target = reference(project, levelId);
  const before = placementSnapshot(project, levelId);
  const after = {
    position: [...next] as Vec3Tuple,
    quarterTurns: before.quarterTurns
  };
  return new FunctionalCommand(
    label,
    () => assignPlacement(target, after),
    () => assignPlacement(target, before)
  );
}

export function createRotateLevelCommand(
  project: EditableWorldProject,
  levelId: string,
  direction: -1 | 1
): EditorCommand {
  const target = reference(project, levelId);
  const before = placementSnapshot(project, levelId);
  const after = {
    position: [...before.position] as Vec3Tuple,
    quarterTurns: normaliseQuarterTurns(before.quarterTurns + direction)
  };
  return new FunctionalCommand(
    direction > 0 ? 'rotate level left' : 'rotate level right',
    () => assignPlacement(target, after),
    () => assignPlacement(target, before)
  );
}

export function createMoveLevelPlacementCommand(
  project: EditableWorldProject,
  levelId: string,
  delta: Vec3Tuple
): EditorCommand {
  const before = levelPlacement(project, levelId);
  return createSetLevelPlacementCommand(
    project,
    levelId,
    [before[0] + delta[0], before[1] + delta[1], before[2] + delta[2]],
    'move level'
  );
}

export function createStackRelativeCommand(
  project: EditableWorldProject,
  levelId: string,
  targetLevelId: string,
  direction: 'above' | 'below',
  gap: number
): EditorCommand {
  if (levelId === targetLevelId) throw new Error('Choose a different reference level');
  if (!Number.isFinite(gap) || gap <= 0) throw new Error('Stack gap must be positive');

  const current = effectiveLevelOrigin(project, levelId);
  const target = effectiveLevelOrigin(project, targetLevelId);
  const source = project.levels.find(candidate => candidate.id === levelId);
  if (!source) throw new Error(`World level ${levelId} does not exist`);

  const desiredEffectiveZ = target[2] + (direction === 'above' ? gap : -gap);
  const currentPlacement = levelPlacement(project, levelId);
  const next: Vec3Tuple = [
    currentPlacement[0] + (target[0] - current[0]),
    currentPlacement[1] + (target[1] - current[1]),
    desiredEffectiveZ - source.viewOrigin[2]
  ];
  return createSetLevelPlacementCommand(
    project,
    levelId,
    next,
    direction === 'above' ? 'stack level above' : 'stack level below'
  );
}
