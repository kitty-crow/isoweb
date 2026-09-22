import type { LevelDocument } from '../world/documents';
import type { EmbeddedPackageAsset, EmbeddedPackageAssetMap } from '../world/PackageAssets';
import { assetContentHash } from './AssetIdentity';
import { FunctionalCommand, type EditorCommand } from './CommandHistory';
import { createLevelDocument } from './ProjectFactory';
import type {
  EditableLevelProject, EditableWorldProject, ProjectSource
} from './SourceProjectIO';
import { SourceProjectIO } from './SourceProjectIO';

export type WorldLevelOperation = {
  command: EditorCommand;
  levelId: string;
};

type ImportedLevelPlan = {
  level: LevelDocument;
  assetsToAdd: EmbeddedPackageAsset[];
};

function clone<T>(value: T): T {
  return JSON.parse(JSON.stringify(value)) as T;
}

function safeStem(value: string): string {
  const stem = value.trim().toLowerCase()
    .replace(/[^a-z0-9._-]+/g, '-')
    .replace(/^-+|-+$/g, '')
    .replace(/\.\.+/g, '-');
  return stem && stem !== '.' && stem !== '..' ? stem : 'level';
}

function nextId(prefix: string, existing: Iterable<string>): string {
  const used = new Set(existing);
  if (!used.has(prefix)) return prefix;
  for (let suffix = 2; suffix < 1_000_000; ++suffix) {
    const candidate = `${prefix}-${suffix}`;
    if (!used.has(candidate)) return candidate;
  }
  throw new Error(`Unable to allocate level id for ${prefix}`);
}

function levelPath(levelId: string, existingPaths: Iterable<string>): string {
  const used = new Set(existingPaths);
  const stem = safeStem(levelId);
  let candidate = `levels/${stem}.json`;
  for (let suffix = 2; used.has(candidate); ++suffix) {
    candidate = `levels/${stem}-${suffix}.json`;
  }
  return candidate;
}

function worldLevelIndex(project: EditableWorldProject, levelId: string): number {
  return project.levels.findIndex(level => level.id === levelId);
}

function referencedByWorldConnector(project: EditableWorldProject, levelId: string): string | undefined {
  return project.document.connectors?.find(
    connector => connector.fromLevel === levelId || connector.toLevel === levelId
  )?.id;
}

function walkSpriteResources(level: LevelDocument, replace: (id: string) => string): void {
  for (const entity of level.entities) {
    if (!('character' in entity.components)) continue;
    const sprites = entity.components.character.sprites;
    if (!sprites) continue;
    const directionals = [
      sprites.still,
      sprites.moving,
      ...Object.values(sprites.actions ?? {})
    ];
    for (const directional of directionals) {
      if (!directional) continue;
      for (const animation of Object.values(directional)) {
        if (animation) animation.resource = replace(animation.resource);
      }
    }
  }
}

function remapLevelAsset(level: LevelDocument, from: string, to: string): void {
  if (from === to) return;

  if (level.assets?.[from]) {
    const definition = level.assets[from];
    delete level.assets[from];
    level.assets[to] = definition;
  }

  for (const material of Object.values(level.localMaterials)) {
    if (material.baseColourTexture === from) material.baseColourTexture = to;
  }

  for (const prefab of Object.values(level.localPrefabs ?? {})) {
    for (const entity of prefab.entities) {
      if (!('character' in entity.components)) continue;
      const sprites = entity.components.character.sprites;
      if (!sprites) continue;
      for (const directional of [
        sprites.still,
        sprites.moving,
        ...Object.values(sprites.actions ?? {})
      ]) {
        if (!directional) continue;
        for (const animation of Object.values(directional)) {
          if (animation?.resource === from) animation.resource = to;
        }
      }
    }
  }

  walkSpriteResources(level, id => id === from ? to : id);
}

async function importedLevelPlan(
  project: EditableWorldProject,
  imported: EditableLevelProject
): Promise<ImportedLevelPlan> {
  const level = clone(imported.document);
  const existingLevelIds = project.levels.map(candidate => candidate.id);
  level.id = nextId(level.id, existingLevelIds);
  if (level.id !== imported.document.id && level.name) level.name = `${level.name} (imported)`;

  const hashToExisting = new Map<string, string>();
  for (const [id, asset] of project.assets) {
    const hash = await assetContentHash(asset.bytes);
    if (!hashToExisting.has(hash)) hashToExisting.set(hash, id);
  }

  const reservedIds = new Set(project.assets.keys());
  const assetsToAdd: EmbeddedPackageAsset[] = [];
  for (const [incomingId, asset] of imported.assets) {
    const hash = await assetContentHash(asset.bytes);
    const identicalId = hashToExisting.get(hash);
    if (identicalId) {
      remapLevelAsset(level, incomingId, identicalId);
      continue;
    }

    const finalId = nextId(incomingId, reservedIds);
    reservedIds.add(finalId);
    remapLevelAsset(level, incomingId, finalId);
    const nextAsset: EmbeddedPackageAsset = {
      ...asset,
      id: finalId,
      path: `assets/by-id/${encodeURIComponent(finalId)}`
    };
    assetsToAdd.push(nextAsset);
    hashToExisting.set(hash, finalId);
  }

  return { level, assetsToAdd };
}

export function createNewWorldLevelOperation(project: EditableWorldProject): WorldLevelOperation {
  const ids = project.levels.map(level => level.id);
  const id = nextId('level', ids);
  const level = createLevelDocument(id, `Level ${project.levels.length + 1}`);
  const reference = {
    id,
    path: levelPath(id, project.document.levels.map(candidate => candidate.path)),
    placement: { position: [0, 0, 0] as [number, number, number] }
  };

  return {
    levelId: id,
    command: new FunctionalCommand(
      'add level',
      () => {
        project.levels.push(level);
        project.document.levels.push(reference);
      },
      () => {
        const index = worldLevelIndex(project, id);
        if (index >= 0) project.levels.splice(index, 1);
        const refIndex = project.document.levels.findIndex(candidate => candidate.id === id);
        if (refIndex >= 0) project.document.levels.splice(refIndex, 1);
      }
    )
  };
}

export function createDuplicateWorldLevelOperation(
  project: EditableWorldProject,
  sourceLevelId: string
): WorldLevelOperation {
  const sourceIndex = worldLevelIndex(project, sourceLevelId);
  if (sourceIndex < 0) throw new Error(`Level ${sourceLevelId} does not exist`);
  const source = project.levels[sourceIndex];
  const level = clone(source);
  level.id = nextId(source.id, project.levels.map(candidate => candidate.id));
  level.name = `${source.name ?? source.id} copy`;
  const sourceReference = project.document.levels.find(candidate => candidate.id === sourceLevelId);
  const reference = {
    id: level.id,
    path: levelPath(level.id, project.document.levels.map(candidate => candidate.path)),
    placement: {
      position: [...(sourceReference?.placement?.position ?? [0, 0, 0])] as [number, number, number],
      quarterTurns: sourceReference?.placement?.quarterTurns ?? 0
    }
  };
  const insertIndex = sourceIndex + 1;

  return {
    levelId: level.id,
    command: new FunctionalCommand(
      'duplicate level',
      () => {
        project.levels.splice(insertIndex, 0, level);
        project.document.levels.splice(insertIndex, 0, reference);
      },
      () => {
        const index = worldLevelIndex(project, level.id);
        if (index >= 0) project.levels.splice(index, 1);
        const refIndex = project.document.levels.findIndex(candidate => candidate.id === level.id);
        if (refIndex >= 0) project.document.levels.splice(refIndex, 1);
      }
    )
  };
}

export function createDeleteWorldLevelCommand(
  project: EditableWorldProject,
  levelId: string
): EditorCommand {
  if (project.levels.length <= 1) throw new Error('A world must contain at least one level');
  const index = worldLevelIndex(project, levelId);
  if (index < 0) throw new Error(`Level ${levelId} does not exist`);
  const connectorId = referencedByWorldConnector(project, levelId);
  if (connectorId) {
    throw new Error(`Level ${levelId} is referenced by world connector ${connectorId}`);
  }

  const level = project.levels[index];
  const referenceIndex = project.document.levels.findIndex(candidate => candidate.id === levelId);
  const reference = project.document.levels[referenceIndex];
  const previousDefault = project.document.settings.defaultLevel;

  return new FunctionalCommand(
    'delete level',
    () => {
      const current = worldLevelIndex(project, levelId);
      if (current >= 0) project.levels.splice(current, 1);
      const refCurrent = project.document.levels.findIndex(candidate => candidate.id === levelId);
      if (refCurrent >= 0) project.document.levels.splice(refCurrent, 1);
      if (project.document.settings.defaultLevel === levelId) {
        project.document.settings.defaultLevel = project.levels[0].id;
      }
    },
    () => {
      if (worldLevelIndex(project, levelId) < 0) {
        project.levels.splice(Math.min(index, project.levels.length), 0, level);
      }
      if (!project.document.levels.some(candidate => candidate.id === levelId)) {
        project.document.levels.splice(
          Math.min(referenceIndex, project.document.levels.length),
          0,
          reference
        );
      }
      project.document.settings.defaultLevel = previousDefault;
    }
  );
}

export function createMoveWorldLevelCommand(
  project: EditableWorldProject,
  levelId: string,
  direction: -1 | 1
): EditorCommand {
  const from = worldLevelIndex(project, levelId);
  if (from < 0) throw new Error(`Level ${levelId} does not exist`);
  const to = from + direction;
  if (to < 0 || to >= project.levels.length) {
    throw new Error(direction < 0 ? 'Level is already first' : 'Level is already last');
  }

  const apply = (a: number, b: number): void => {
    const [level] = project.levels.splice(a, 1);
    project.levels.splice(b, 0, level);
    const refIndex = project.document.levels.findIndex(candidate => candidate.id === levelId);
    const [reference] = project.document.levels.splice(refIndex, 1);
    project.document.levels.splice(b, 0, reference);
  };

  return new FunctionalCommand(
    direction < 0 ? 'move level up' : 'move level down',
    () => apply(worldLevelIndex(project, levelId), to),
    () => apply(worldLevelIndex(project, levelId), from)
  );
}

export async function createImportWorldLevelOperation(
  project: EditableWorldProject,
  source: ProjectSource,
  io = new SourceProjectIO()
): Promise<WorldLevelOperation> {
  const imported = await io.openLevel(source);
  const plan = await importedLevelPlan(project, imported);
  const reference = {
    id: plan.level.id,
    path: levelPath(
      plan.level.id,
      project.document.levels.map(candidate => candidate.path)
    ),
    placement: { position: [0, 0, 0] as [number, number, number] }
  };

  return {
    levelId: plan.level.id,
    command: new FunctionalCommand(
      'import level',
      () => {
        project.levels.push(plan.level);
        project.document.levels.push(reference);
        for (const asset of plan.assetsToAdd) project.assets.set(asset.id, asset);
      },
      () => {
        const index = worldLevelIndex(project, plan.level.id);
        if (index >= 0) project.levels.splice(index, 1);
        const refIndex = project.document.levels.findIndex(candidate => candidate.id === plan.level.id);
        if (refIndex >= 0) project.document.levels.splice(refIndex, 1);
        for (const asset of plan.assetsToAdd) project.assets.delete(asset.id);
      }
    )
  };
}
