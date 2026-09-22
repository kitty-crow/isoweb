import type { Vec3Tuple, WorldLevelReference } from '../world/documents';
import { FunctionalCommand, type EditorCommand } from './CommandHistory';
import type { EditableWorldProject } from './SourceProjectIO';

function reference(project: EditableWorldProject, levelId: string): WorldLevelReference {
  const value = project.document.levels.find(candidate => candidate.id === levelId);
  if (!value) throw new Error(`World level ${levelId} does not exist`);
  return value;
}

export function levelPlacement(
  project: EditableWorldProject,
  levelId: string
): Vec3Tuple {
  return [...(reference(project, levelId).placement?.position ?? [0, 0, 0])] as Vec3Tuple;
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

export function createSetLevelPlacementCommand(
  project: EditableWorldProject,
  levelId: string,
  next: Vec3Tuple,
  label = 'place level'
): EditorCommand {
  const target = reference(project, levelId);
  const before = levelPlacement(project, levelId);
  const after = [...next] as Vec3Tuple;
  return new FunctionalCommand(
    label,
    () => { target.placement = { position: [...after] as Vec3Tuple }; },
    () => { target.placement = { position: [...before] as Vec3Tuple }; }
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
