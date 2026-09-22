import {
  createDeleteCommand,
  createDuplicateOperation,
  createLocalAddOperation
} from '../web/src/editor/AuthoringCommands';
import { EditorCore } from '../web/src/editor/EditorCore';

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
  'character',
  'dynamic-body',
  'spawn',
  'connector'
] as const;

for (const kind of addKinds) {
  const operation = createLocalAddOperation(project, core.selection.value, kind);
  core.execute(operation.command);
  core.selection.select(operation.selection);
}

if (project.document.ground.length !== 2) throw new Error('Ground authoring did not add a region');
if (project.document.geometry.length !== 6) throw new Error('Primitive authoring did not expose every current primitive');
if ((project.document.rooms ?? []).length !== 1) throw new Error('Room authoring failed');
if (project.document.entities.length !== 2) throw new Error('Entity authoring failed');
if (project.document.spawns.length !== 1) throw new Error('Spawn authoring failed');
if (project.document.connectors.length !== 1) throw new Error('Local connector authoring failed');

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

const bytes = core.saveBytes();
const reopened = new EditorCore('level');
const reopenedProject = await reopened.open(bytes);
if (reopenedProject.kind !== 'level' ||
    reopenedProject.document.geometry.length !== 6 ||
    (reopenedProject.document.rooms ?? []).length !== 1 ||
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

console.log('Level authoring smoke passed: ground, primitives, rooms, entities, spawns and local connectors are undoable and persist in source packages.');
