import { FunctionalCommand } from '../web/src/editor/CommandHistory';
import { EditorCore } from '../web/src/editor/EditorCore';

const levelCore = new EditorCore('level');
const levelProject = levelCore.newProject();
if (levelProject.kind !== 'level' || levelProject.document.ground.length !== 1) {
  throw new Error('Level Builder factory did not create a valid roomless source level');
}
if (levelCore.problems().length !== 0) {
  throw new Error(`New level project is invalid: ${JSON.stringify(levelCore.problems())}`);
}

const originalName = levelProject.document.name;
levelCore.execute(new FunctionalCommand(
  'rename level',
  () => { levelProject.document.name = 'Edited Level'; },
  () => { levelProject.document.name = originalName; }
));
if (!levelCore.store.dirty || levelProject.document.name !== 'Edited Level') {
  throw new Error('Editor command did not mutate the canonical source document');
}
levelCore.undo();
if (levelProject.document.name !== originalName || !levelCore.store.canRedo) {
  throw new Error('Undo did not restore authored state');
}
levelCore.redo();
if (levelProject.document.name !== 'Edited Level' || !levelCore.store.canUndo) {
  throw new Error('Redo did not restore authored mutation');
}

levelCore.selection.select({ kind: 'level', id: levelProject.document.id, levelId: levelProject.document.id });
if (levelCore.selection.value?.id !== levelProject.document.id) {
  throw new Error('Stable-id editor selection failed');
}

const levelBytes = levelCore.saveBytes();
if (levelCore.store.dirty) throw new Error('Saving did not clear dirty state');

const reopenedLevel = new EditorCore('level');
const reopenedLevelProject = await reopenedLevel.open(levelBytes);
if (reopenedLevelProject.kind !== 'level' ||
    reopenedLevelProject.document.name !== 'Edited Level' ||
    reopenedLevel.problems().length !== 0) {
  throw new Error('Level Builder source save/reopen lost authored state');
}

const worldCore = new EditorCore('world');
const worldProject = worldCore.newProject();
if (worldProject.kind !== 'world' || worldProject.levels.length !== 1 ||
    worldProject.document.settings.defaultLevel !== worldProject.levels[0].id) {
  throw new Error('World Editor factory did not create a coherent editable world');
}
if (worldCore.problems().length !== 0) {
  throw new Error(`New world project is invalid: ${JSON.stringify(worldCore.problems())}`);
}

const originalWorldName = worldProject.document.name;
worldCore.execute(new FunctionalCommand(
  'rename world',
  () => { worldProject.document.name = 'Edited World'; },
  () => { worldProject.document.name = originalWorldName; }
));
const worldBytes = worldCore.saveBytes();
const reopenedWorld = new EditorCore('world');
const reopenedWorldProject = await reopenedWorld.open(worldBytes);
if (reopenedWorldProject.kind !== 'world' ||
    reopenedWorldProject.document.name !== 'Edited World' ||
    reopenedWorldProject.levels.length !== 1 ||
    reopenedWorld.problems().length !== 0) {
  throw new Error('World Editor source save/reopen lost authored state');
}

console.log('Shared Editor Core smoke passed: canonical source state, selection, undo/redo, validation and source save/reopen work.');
