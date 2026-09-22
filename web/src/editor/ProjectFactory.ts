import type { LevelDocument, PackageManifest, WorldDocument } from '../world/documents';
import { CURRENT_SCHEMA_VERSION } from '../world/schemas/version';
import type { EditableLevelProject, EditableWorldProject } from './SourceProjectIO';

export function createLevelDocument(id = 'level-1', name = 'Untitled Level'): LevelDocument {
  return {
    schemaVersion: CURRENT_SCHEMA_VERSION,
    id,
    name,
    coordinateSystem: { units: 'metres', upAxis: 'z', handedness: 'right' },
    viewOrigin: [0, 0, 0],
    ground: [{
      id: 'ground',
      type: 'rectangle',
      centre: [0, 0, 0],
      size: [12, 12],
      material: 'default-floor'
    }],
    geometry: [],
    entities: [],
    lights: [{ id: 'default-light', type: 'point', position: [4, -4, 6] }],
    spawns: [],
    connectors: [],
    localMaterials: {
      'default-floor': { id: 'default-floor', baseColour: [0.33, 0.36, 0.39] },
      'default-wall': { id: 'default-wall', baseColour: [0.55, 0.57, 0.60] }
    },
    localPrefabs: {},
    assets: {},
    settings: {
      boundsFocus: [0, 0, 0],
      floorDarkMaterial: 'default-floor',
      floorLightMaterial: 'default-floor',
      wallMaterial: 'default-wall'
    },
    editor: { hiddenIds: [], lockedIds: [] }
  };
}

function sourceManifest(
  format: PackageManifest['format'],
  id: string,
  name: string,
  entry: string
): PackageManifest & { representation: 'source' } {
  return {
    format,
    representation: 'source',
    schemaVersion: CURRENT_SCHEMA_VERSION,
    id,
    name,
    entry,
    createdWith: {
      application: format === 'isolevel' ? 'isoweb-level-builder' : 'isoweb-world-editor',
      version: '0.1.0'
    },
    assets: []
  };
}

export function createLevelProject(): EditableLevelProject {
  const document = createLevelDocument();
  return {
    kind: 'level',
    manifest: sourceManifest('isolevel', document.id, document.name ?? document.id, 'level.json'),
    document,
    assets: new Map()
  };
}

export function createWorldProject(): EditableWorldProject {
  const level = createLevelDocument();
  const world: WorldDocument = {
    schemaVersion: CURRENT_SCHEMA_VERSION,
    id: 'world-1',
    name: 'Untitled World',
    settings: { defaultLevel: level.id },
    levels: [{ id: level.id, path: `levels/${level.id}.json` }],
    assets: {},
    materials: {},
    prefabs: {},
    connectors: [],
    behaviours: [],
    metadata: {},
    editor: { hiddenIds: [], lockedIds: [] }
  };

  return {
    kind: 'world',
    manifest: sourceManifest('isoworld', world.id, world.name ?? world.id, 'world.json'),
    document: world,
    levels: [level],
    assets: new Map()
  };
}
