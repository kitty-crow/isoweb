import { assetContentHash, findDuplicateAssetId } from '../web/src/editor/AssetIdentity';
import { SourceProjectIO } from '../web/src/editor/SourceProjectIO';
import type {
  DynamicBodyEntityDefinition, LevelDocument, WorldDocument
} from '../web/src/world/documents';
import { migrateLevelSource, migrateWorldSource } from '../web/src/world/migrations';
import { PackageWriter } from '../web/src/world/PackageWriter';
import { validateLevelDocument, validateWorldDocument } from '../web/src/world/validation';

const entity: DynamicBodyEntityDefinition = {
  id: 'crate',
  components: {
    transform: {
      position: [1, 2, 0],
      rotation: [0, 0, 0.25],
      scale: [1, 1, 1]
    },
    collider: {
      type: 'box',
      minimum: [-0.5, -0.5, 0],
      maximum: [0.5, 0.5, 1],
      solid: true
    },
    dynamicBody: {}
  }
};

const level: LevelDocument = {
  schemaVersion: 1,
  id: 'editor-smoke',
  name: 'Editor Smoke Level',
  coordinateSystem: { units: 'metres', upAxis: 'z', handedness: 'right' },
  viewOrigin: [0, 0, 0],
  ground: [{
    id: 'ground',
    type: 'rectangle',
    centre: [0, 0, 0],
    size: [10, 10],
    material: 'floor'
  }],
  geometry: [],
  entities: [entity],
  lights: [{ id: 'light', type: 'point', position: [0, 0, 4] }],
  spawns: [{
    id: 'start',
    transform: { position: [0, 0, 0], rotation: [0, 0, 0], scale: [1, 1, 1] },
    entityId: 'crate',
    tags: ['default']
  }],
  connectors: [{
    id: 'doorway',
    type: 'door',
    fromPosition: [0, -1, 0],
    toPosition: [0, 1, 0],
    forwardTraversal: [[0, 0, 0]],
    bidirectional: true
  }],
  localMaterials: {
    floor: {
      id: 'floor',
      baseColour: [0.2, 0.3, 0.4],
      alphaMode: 'opaque',
      opacity: 1,
      textureMode: 'tile-world',
      worldUnitsPerTile: 1
    }
  },
  localPrefabs: {
    crate: {
      id: 'crate',
      name: 'Crate',
      entities: [{ ...entity, id: 'prefab-crate' }]
    }
  },
  settings: {
    boundsFocus: [0, 0, 0],
    floorDarkMaterial: 'floor',
    floorLightMaterial: 'floor',
    wallMaterial: 'floor'
  },
  editor: {
    hiddenIds: [],
    lockedIds: [],
    metadata: { viewport: 'authoring' }
  }
};

const world: WorldDocument = {
  schemaVersion: 1,
  id: 'editor-smoke-world',
  name: 'Editor Smoke World',
  settings: { defaultLevel: level.id },
  levels: [{ id: level.id, path: 'levels/editor-smoke.json' }],
  assets: {},
  materials: {},
  prefabs: {
    crate: {
      id: 'crate',
      entities: [{ ...entity, id: 'world-prefab-crate' }]
    }
  },
  connectors: [],
  metadata: {},
  editor: { hiddenIds: [], lockedIds: [] }
};

if (migrateLevelSource(level) !== level || migrateWorldSource(world) !== world) {
  throw new Error('v1 migration boundary did not preserve current source documents');
}

for (const future of [
  () => migrateLevelSource({ ...level, schemaVersion: 2 }),
  () => migrateWorldSource({ ...world, schemaVersion: 2 })
]) {
  let rejected = false;
  try { future(); } catch { rejected = true; }
  if (!rejected) throw new Error('Future source schema version was accepted');
}

validateLevelDocument(level);
validateWorldDocument(world);

let rejectedSpawn = false;
try {
  validateLevelDocument({
    ...level,
    spawns: [{
      id: 'broken',
      transform: { position: [0, 0, 0] },
      entityId: 'missing'
    }]
  });
} catch { rejectedSpawn = true; }
if (!rejectedSpawn) throw new Error('Missing spawn entity reference was accepted');

let rejectedConnector = false;
try {
  validateLevelDocument({
    ...level,
    connectors: [{
      id: 'broken',
      type: 'door',
      fromPosition: [0, 0, 0],
      toPosition: [1, 1, 0],
      forwardTraversal: [[0, Number.NaN, 0]]
    }]
  });
} catch { rejectedConnector = true; }
if (!rejectedConnector) throw new Error('Invalid connector traversal was accepted');

let rejectedPrefab = false;
try {
  validateWorldDocument({
    ...world,
    prefabs: {
      broken: {
        id: 'wrong-id',
        entities: []
      }
    }
  });
} catch { rejectedPrefab = true; }
if (!rejectedPrefab) throw new Error('Mismatched prefab registry id was accepted');

const writer = new PackageWriter();
const io = new SourceProjectIO();
const levelBytes = writer.writeLevelBytes(level);
const openedLevel = await io.openLevel(levelBytes);
if (openedLevel.document.id !== level.id || openedLevel.document.spawns[0]?.id !== 'start') {
  throw new Error('Editable level project did not round-trip');
}
const resavedLevel = await io.openLevel(io.saveLevelBytes(openedLevel));
if (resavedLevel.document.connectors[0]?.id !== 'doorway') {
  throw new Error('Editable level save did not preserve connector data');
}

const worldBytes = writer.writeWorldBytes(world, [level]);
const openedWorld = await io.openWorld(worldBytes);
if (openedWorld.document.id !== world.id || openedWorld.levels[0]?.id !== level.id) {
  throw new Error('Editable world project did not round-trip');
}
const resavedWorld = await io.openWorld(io.saveWorldBytes(openedWorld));
if (resavedWorld.document.prefabs.crate?.id !== 'crate') {
  throw new Error('Editable world save did not preserve prefab data');
}

const bytesA = new TextEncoder().encode('same asset');
const bytesB = new TextEncoder().encode('different asset');
const hashA = await assetContentHash(bytesA);
if (!hashA.startsWith('sha256:') || hashA.length !== 71) {
  throw new Error('Asset content hash has the wrong format');
}
const duplicate = await findDuplicateAssetId(bytesA, [
  ['first', bytesA] as const,
  ['second', bytesB] as const
]);
if (duplicate !== 'first') throw new Error('Hash-based duplicate asset detection failed');

console.log('Editor source contract smoke passed: typed source data, migration, validation, project I/O and asset hashing are coherent.');
