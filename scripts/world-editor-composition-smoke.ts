import { EditorCore } from '../web/src/editor/EditorCore';
import {
  createRotateLevelCommand,
  createSetLevelPlacementCommand,
  createStackRelativeCommand,
  levelQuarterTurns,
  worldPointForLevel
} from '../web/src/editor/WorldPlacementManager';
import {
  createWorldConnectionOperation
} from '../web/src/editor/WorldConnectorManager';
import {
  createDuplicateWorldLevelOperation
} from '../web/src/editor/WorldLevelManager';
import { WorldCompiler } from '../web/src/world/WorldCompiler';
import { PackageReader } from '../web/src/world/PackageReader';

const core = new EditorCore('world');
const project = core.newProject();
if (project.kind !== 'world') throw new Error('Expected world project');

const lower = project.levels[0];
lower.name = 'Lower';
lower.viewOrigin = [0, 0, 0];
lower.ground[0].centre = [2, 1, 0];
lower.ground[0].size = [8, 4];
lower.rooms = [
  {
    id: 'a',
    centre: [0, 0, 0],
    width: 4,
    depth: 2,
    floorZ: 0,
    wallHeight: 2.4,
    wallThickness: 0.15,
    wallMaterial: lower.settings.wallMaterial
  },
  {
    id: 'b',
    centre: [4, 0, 0],
    width: 4,
    depth: 2,
    floorZ: 0,
    wallHeight: 2.4,
    wallThickness: 0.15,
    wallMaterial: lower.settings.wallMaterial
  }
];
lower.roomConnections = [{
  id: 'door',
  a: { roomId: 'a', side: 'east', offset: 0, width: 1 },
  b: { roomId: 'b', side: 'west', offset: 0, width: 1 },
  openPassage: true
}];
lower.floorHoles = [{
  id: 'hole',
  type: 'rectangle',
  minimum: [1, 2],
  maximum: [3, 4]
}];
lower.staircases = [{
  id: 'stairs',
  centreX: 2,
  startY: 1,
  endY: 3,
  startZ: 0,
  endZ: 2,
  width: 1
}];
lower.entities.push({
  id: 'probe',
  components: {
    transform: { position: [1, 0, 0], forward: [1, 0, 0] },
    character: { controllable: false, npc: true }
  }
});

lower.settings.boundsFocus = [1, 0, 0];

const duplicate = createDuplicateWorldLevelOperation(project, lower.id);
core.execute(duplicate.command);
const upperId = duplicate.levelId;
const upper = project.levels.find(level => level.id === upperId)!;
upper.name = 'Upper';
upper.settings.boundsFocus = [-1, 2, 0];
// Runtime entity IDs are world-global; this probe only exists to verify the
// transformed lower-level entity contract.
upper.entities = upper.entities.filter(entity => entity.id !== 'probe');

core.execute(createSetLevelPlacementCommand(project, lower.id, [10, 20, 3]));
core.execute(createRotateLevelCommand(project, lower.id, 1));
if (levelQuarterTurns(project, lower.id) !== 1) {
  throw new Error('World level rotation command did not persist a quarter turn');
}

core.execute(createSetLevelPlacementCommand(project, upperId, [10, 20, 9]));
const stairs = createWorldConnectionOperation(project, lower.id, upperId, 'stairs');
core.execute(stairs.command);
const worldStairs = project.document.connectors!.find(value => value.id === stairs.connectorId)!;
const beforeTraversal = JSON.stringify(worldStairs.forwardTraversal);
core.execute(createRotateLevelCommand(project, lower.id, 1));
if (levelQuarterTurns(project, lower.id) !== 2) throw new Error('Second rotation did not apply');
if (JSON.stringify(worldStairs.forwardTraversal) === beforeTraversal) {
  throw new Error('Inter-level stairs were not regenerated after rotating a level');
}
core.undo();
if (levelQuarterTurns(project, lower.id) !== 1) throw new Error('Undo did not restore level rotation');

const compiled = new WorldCompiler().compilePackage({
  manifest: project.manifest,
  world: project.document,
  levels: project.levels,
  assets: project.assets
});
const compiledLower = compiled.levels.find(level => level.id === lower.id)!;

const assertTuple = (label: string, actual: readonly number[], expected: readonly number[]) => {
  if (actual.length !== expected.length ||
      actual.some((value, index) => Math.abs(value - expected[index]) > 1e-6)) {
    throw new Error(`${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
  }
};

assertTuple('compiled world origin', compiledLower.viewOrigin, [10, 20, 3]);
assertTuple('rotated ground centre', compiledLower.ground[0].centre, [-1, 2, 0]);
if (compiledLower.ground[0].width !== 4 || compiledLower.ground[0].depth !== 8) {
  throw new Error('Quarter-turn did not swap ground width/depth');
}
if (compiledLower.rooms[0].width !== 2 || compiledLower.rooms[0].depth !== 4) {
  throw new Error('Quarter-turn did not swap room width/depth');
}
if (compiledLower.roomConnections[0].a.side !== 0 ||
    compiledLower.roomConnections[0].b.side !== 1) {
  throw new Error('Room doorway sides did not rotate east/west to north/south');
}
const hole = compiledLower.floorHoles[0];
assertTuple(
  'rotated floor hole',
  [hole.minimumX, hole.maximumX, hole.minimumY, hole.maximumY],
  [-4, -2, 1, 3]
);
const stair = compiledLower.staircases[0];
assertTuple(
  'rotated staircase run',
  [stair.startX, stair.startY, stair.endX, stair.endY],
  [-1, 2, -3, 2]
);
const entity = compiledLower.entities.find(value => value.id === 'probe')!;
assertTuple('rotated entity', entity.position, [0, 1, 0]);
assertTuple('rotated entity facing', entity.forward, [0, 1]);

const compiledConnection = compiled.world.connectors.find(value => value.id === stairs.connectorId)!;
const expectedFrom = worldPointForLevel(project, lower.id, worldStairs.fromPosition);
const fromOrigin = compiledLower.viewOrigin;
assertTuple(
  'compiled connector source',
  compiledConnection.fromPosition,
  [
    expectedFrom[0] - fromOrigin[0],
    expectedFrom[1] - fromOrigin[1],
    expectedFrom[2] - fromOrigin[2]
  ]
);

if (lower.roomConnections[0].a.side !== 'east' ||
    lower.staircases[0].centreX !== 2 ||
    lower.entities.find(value => value.id === 'probe')!.components.transform.position[0] !== 1) {
  throw new Error('World compilation mutated source .isolevel content');
}

core.execute(createStackRelativeCommand(project, upperId, lower.id, 'above', 3.5));
const lowerWorld = worldPointForLevel(project, lower.id, [0, 0, 0]);
const upperWorld = worldPointForLevel(project, upperId, [0, 0, 0]);
if (Math.abs(upperWorld[2] - lowerWorld[2] - 3.5) > 1e-6) {
  throw new Error('Stack above did not place the level at the requested Z gap');
}

const saved = core.saveBytes();
const reopened = await new PackageReader().loadWorld(saved);
const reopenedRef = reopened.world.levels.find(value => value.id === lower.id);
if (reopenedRef?.placement?.quarterTurns !== 1 ||
    reopenedRef.placement.position[0] !== 10 ||
    reopenedRef.placement.position[1] !== 20) {
  throw new Error('World placement/rotation did not survive .isoworld round-trip');
}

console.log('World composition smoke passed: whole-level XY/Z placement, stacking, cardinal rotation, doorway/hole/stair/entity transforms, attached inter-level stairs and source isolation work.');
