import {
  createDeleteCommand,
  createDuplicateOperation,
  createLocalAddOperation
} from '../web/src/editor/AuthoringCommands';
import { EditorCore } from '../web/src/editor/EditorCore';
import {
  createDeleteRoomConnectionCommand,
  createRoomConnectionOperation
} from '../web/src/editor/RoomConnectionManager';
import { PackageReader } from '../web/src/world/PackageReader';

const core = new EditorCore('level');
const project = core.newProject();
if (project.kind !== 'level') throw new Error('Expected a level project');

const addKinds = [
  'ground',
  'cube',
  'sphere',
  'cone',
  'pyramid',
  'dodecahedron',
  'icosahedron',
  'room',
  'floor-hole',
  'staircase',
  'character',
  'dynamic-body',
  'spawn',
  'connector',
  'light'
] as const;

for (const kind of addKinds) {
  const operation = createLocalAddOperation(project, core.selection.value, kind);
  core.execute(operation.command);
  core.selection.select(operation.selection);
}

if (project.document.ground.length !== 2) throw new Error('Ground authoring did not add a region');
if (project.document.geometry.length !== 6) throw new Error('Primitive authoring did not expose every current primitive');
if ((project.document.rooms ?? []).length !== 1) throw new Error('Room authoring failed');
if ((project.document.floorHoles ?? []).length !== 1) throw new Error('Floor-hole authoring failed');
if ((project.document.staircases ?? []).length !== 1) throw new Error('Staircase authoring failed');
if (project.document.entities.length !== 2) throw new Error('Entity authoring failed');
if (project.document.spawns.length !== 1) throw new Error('Spawn authoring failed');
if (project.document.connectors.length !== 1) throw new Error('Local connector authoring failed');
if (project.document.lights.length !== 2) throw new Error('Point-light authoring failed');

const placedRoom = createLocalAddOperation(project, core.selection.value, 'room', { x: 3.5, y: -2, z: 1 });
core.execute(placedRoom.command);
const placedRoomValue = (project.document.rooms ?? []).find(value => value.id === placedRoom.selection.id);
if (!placedRoomValue ||
    placedRoomValue.centre[0] !== 3.5 ||
    placedRoomValue.centre[1] !== -2 ||
    placedRoomValue.centre[2] !== 1) {
  throw new Error('Explicit drag/drop placement coordinates were not preserved');
}

const rooms = project.document.rooms ?? [];
if (rooms.length !== 2) throw new Error('Room setup for opening authoring is invalid');
const opening = createRoomConnectionOperation(
  project,
  { kind: 'room', id: rooms[0].id, levelId: project.document.id },
  rooms[0].id,
  rooms[1].id
);
core.execute(opening.command);
if ((project.document.roomConnections ?? []).length !== 1) {
  throw new Error('Room opening authoring failed');
}
const openingValue = project.document.roomConnections![0];
if (openingValue.a.roomId !== rooms[0].id ||
    openingValue.b.roomId !== rooms[1].id ||
    openingValue.a.width <= 0 ||
    openingValue.b.width <= 0) {
  throw new Error('Room opening endpoints were not authored correctly');
}

let referencedRoomDeleteRejected = false;
try {
  createDeleteCommand(project, { kind: 'room', id: rooms[0].id, levelId: project.document.id });
} catch (error) {
  referencedRoomDeleteRejected = String(error).includes(openingValue.id);
}
if (!referencedRoomDeleteRejected) {
  throw new Error('Deleting a room referenced by an opening was not blocked');
}

core.execute(createDeleteRoomConnectionCommand(project, opening.selection));
if ((project.document.roomConnections ?? []).length !== 0) {
  throw new Error('Room opening delete failed');
}
core.undo();
if ((project.document.roomConnections ?? []).length !== 1) {
  throw new Error('Undo did not restore room opening');
}

const cube = project.document.geometry.find(value => value.type === 'cube');
if (!cube) throw new Error('Cube was not created');
core.selection.select({ kind: 'geometry', id: cube.id, levelId: project.document.id });
const duplicate = createDuplicateOperation(project, core.selection.value);
core.execute(duplicate.command);
core.selection.select(duplicate.selection);
if (project.document.geometry.length !== 7 || duplicate.selection.id === cube.id) {
  throw new Error('Duplicate did not allocate a stable distinct id');
}

core.execute(createDeleteCommand(project, duplicate.selection));
if (project.document.geometry.length !== 6) throw new Error('Delete did not remove selected geometry');
core.undo();
if (project.document.geometry.length !== 7) throw new Error('Undo did not restore deleted geometry');
core.redo();
if (project.document.geometry.length !== 6) throw new Error('Redo did not remove geometry again');

const problems = core.problems();
if (problems.length !== 0) {
  throw new Error(`Authored level is invalid: ${JSON.stringify(problems)}`);
}

const previewBytes = core.previewWorldBytes();
if (!core.store.dirty) {
  throw new Error('Runtime preview packaging unexpectedly cleared dirty authored state');
}
const preview = await new PackageReader().loadWorld(previewBytes);
if (preview.world.settings.defaultLevel !== project.document.id ||
    preview.levels.length !== 1 ||
    preview.levels[0].id !== project.document.id ||
    (preview.levels[0].floorHoles ?? []).length !== 1 ||
    (preview.levels[0].staircases ?? []).length !== 1 ||
    (preview.levels[0].roomConnections ?? []).length !== 1) {
  throw new Error('Level Builder runtime preview did not wrap the current source level correctly');
}

const bytes = core.saveBytes();
const reopened = new EditorCore('level');
const reopenedProject = await reopened.open(bytes);
if (reopenedProject.kind !== 'level' ||
    reopenedProject.document.geometry.length !== 6 ||
    (reopenedProject.document.rooms ?? []).length !== 2 ||
    (reopenedProject.document.floorHoles ?? []).length !== 1 ||
    (reopenedProject.document.staircases ?? []).length !== 1 ||
    (reopenedProject.document.roomConnections ?? []).length !== 1 ||
    reopenedProject.document.entities.length !== 2 ||
    reopenedProject.document.connectors.length !== 1) {
  throw new Error('Authored level did not survive source package save/reopen');
}

const worldCore = new EditorCore('world');
const world = worldCore.newProject();
if (world.kind !== 'world') throw new Error('Expected a world project');
const worldCube = createLocalAddOperation(world, worldCore.selection.value, 'cube');
worldCore.execute(worldCube.command);
if (world.levels[0].geometry.length !== 1) {
  throw new Error('World Editor did not reuse local Level Builder authoring commands');
}
const worldPreview = await new PackageReader().loadWorld(worldCore.previewWorldBytes());
if (worldPreview.world.id !== world.document.id ||
    worldPreview.levels[0].geometry.length !== 1) {
  throw new Error('World Editor runtime preview did not preserve the current in-memory source world');
}

console.log('Level authoring smoke passed: rooms, openings, floor holes, stairs, geometry and gameplay objects are undoable, persist in source packages, and build a real-runtime preview world.');
