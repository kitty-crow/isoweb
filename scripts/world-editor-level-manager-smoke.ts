import {
  createDeleteWorldLevelCommand,
  createDuplicateWorldLevelOperation,
  createImportWorldLevelOperation,
  createMoveWorldLevelCommand,
  createNewWorldLevelOperation
} from '../web/src/editor/WorldLevelManager';
import { EditorCore } from '../web/src/editor/EditorCore';
import { createLevelProject } from '../web/src/editor/ProjectFactory';
import { PackageReader } from '../web/src/world/PackageReader';
import { PackageWriter } from '../web/src/world/PackageWriter';

const core = new EditorCore('world');
const project = core.newProject();
if (project.kind !== 'world') throw new Error('Expected world project');

const firstId = project.levels[0].id;
project.levels[0].entities.push({
  id: 'shared-entity',
  components: {
    transform: { position: [0, 0, 0] },
    character: { controllable: false, npc: true }
  }
});
project.levels[0].spawns.push({
  id: 'shared-spawn',
  transform: { position: [0, 0, 0] },
  entityId: 'shared-entity'
});

const added = createNewWorldLevelOperation(project);
core.execute(added.command);
if (project.levels.length !== 2 || project.document.levels.length !== 2) {
  throw new Error('New level did not update world document and editable level collection together');
}

const duplicated = createDuplicateWorldLevelOperation(project, firstId);
core.execute(duplicated.command);
if (project.levels.length !== 3 ||
    project.levels[1].id !== duplicated.levelId ||
    project.levels[1].id === firstId) {
  throw new Error('Duplicate level did not allocate a stable unique world-level id');
}
const duplicatedLevel = project.levels[1];
if (duplicatedLevel.entities[0]?.id === 'shared-entity' ||
    duplicatedLevel.spawns[0]?.entityId !== duplicatedLevel.entities[0]?.id) {
  throw new Error('Duplicate level did not remap world-global entity ids and spawn references');
}

core.execute(createMoveWorldLevelCommand(project, duplicated.levelId, 1));
if (project.levels[2].id !== duplicated.levelId ||
    project.document.levels[2].id !== duplicated.levelId) {
  throw new Error('Level reorder did not keep source levels and world references aligned');
}
core.undo();
if (project.levels[1].id !== duplicated.levelId) {
  throw new Error('Undo did not restore level ordering');
}
core.redo();
if (project.levels[2].id !== duplicated.levelId) {
  throw new Error('Redo did not restore level ordering');
}

core.execute(createDeleteWorldLevelCommand(project, added.levelId));
if (project.levels.some(level => level.id === added.levelId)) {
  throw new Error('Delete level left source level behind');
}
core.undo();
if (!project.levels.some(level => level.id === added.levelId)) {
  throw new Error('Undo did not restore deleted level');
}

project.assets.set('canonical.png', {
  id: 'canonical.png',
  path: 'assets/by-id/canonical.png',
  mediaType: 'image/png',
  bytes: new Uint8Array([1, 2, 3, 4])
});
project.assets.set('collision.png', {
  id: 'collision.png',
  path: 'assets/by-id/collision.png',
  mediaType: 'image/png',
  bytes: new Uint8Array([7, 7, 7])
});

const imported = createLevelProject();
imported.document.id = firstId;
imported.document.name = 'Imported level';
imported.document.entities.push({
  id: 'shared-entity',
  components: {
    transform: { position: [1, 1, 0] },
    character: { controllable: false, npc: true }
  }
});
imported.document.spawns.push({
  id: 'imported-spawn',
  transform: { position: [1, 1, 0] },
  entityId: 'shared-entity'
});
imported.document.assets = {
  'duplicate.png': { source: 'assets/by-id/duplicate.png', mediaType: 'image/png' },
  'collision.png': { source: 'assets/by-id/collision.png', mediaType: 'image/png' }
};
imported.document.localMaterials['default-floor'].baseColourTexture = 'duplicate.png';

const importedBytes = new PackageWriter().writeLevelBytes(
  imported.document,
  new Map([
    ['duplicate.png', { bytes: new Uint8Array([1, 2, 3, 4]), mediaType: 'image/png' }],
    ['collision.png', { bytes: new Uint8Array([8, 8, 8]), mediaType: 'image/png' }]
  ])
);

const importOperation = await createImportWorldLevelOperation(project, importedBytes);
core.execute(importOperation.command);
const importedLevel = project.levels.find(level => level.id === importOperation.levelId);
if (!importedLevel || importedLevel.id === firstId) {
  throw new Error('Imported level id collision was not resolved');
}
const importedEntity = importedLevel.entities.find(entity =>
  entity.components.transform.position[0] === 1 &&
  entity.components.transform.position[1] === 1
);
if (!importedEntity || importedEntity.id === 'shared-entity' ||
    importedLevel.spawns.find(spawn => spawn.id === 'imported-spawn')?.entityId !== importedEntity.id) {
  throw new Error('Imported level did not remap colliding world-global entity ids');
}
if (importedLevel.localMaterials['default-floor'].baseColourTexture !== 'canonical.png') {
  throw new Error('Content-identical imported asset was not deduplicated and remapped');
}
if (!importedLevel.assets?.['canonical.png'] || importedLevel.assets['duplicate.png']) {
  throw new Error('Imported level asset declarations were not remapped after content deduplication');
}
if (!importedLevel.assets?.['collision.png-2'] || !project.assets.has('collision.png-2')) {
  throw new Error('Conflicting imported asset id was not safely renamed');
}
if (project.assets.has('duplicate.png')) {
  throw new Error('Content-identical asset was redundantly copied into the world');
}

const packaged = core.previewWorldBytes();
const reopened = await new PackageReader().loadWorld(packaged);
if (reopened.levels.length !== project.levels.length) {
  throw new Error('Multi-level world did not survive runtime-preview package round-trip');
}
const reopenedImport = reopened.levels.find(level => level.id === importOperation.levelId);
if (!reopenedImport?.assets?.['canonical.png'] || !reopenedImport.assets['collision.png-2']) {
  throw new Error('Imported level asset remapping did not survive .isoworld packaging');
}

const connectorLevel = project.levels[0].id;
project.document.connectors = [{
  id: 'protected-link',
  type: 'stairs',
  fromLevel: connectorLevel,
  toLevel: importedLevel.id,
  fromPosition: [0, 0, 0],
  toPosition: [0, 0, 0]
}];
let protectedDeleteRejected = false;
try {
  createDeleteWorldLevelCommand(project, connectorLevel);
} catch (error) {
  protectedDeleteRejected = String(error).includes('protected-link');
}
if (!protectedDeleteRejected) {
  throw new Error('Deleting a level referenced by a world connector was not blocked');
}

console.log('World level manager smoke passed: create, duplicate, reorder, safe delete, entity-id remapping, .isolevel import, content deduplication, collision remap and .isoworld round-trip work.');
