import {
  createDeleteWorldConnectorCommand,
  createSetDefaultLevelCommand,
  createWorldPortalOperation
} from '../web/src/editor/WorldConnectorManager';
import { createNewWorldLevelOperation } from '../web/src/editor/WorldLevelManager';
import { EditorCore } from '../web/src/editor/EditorCore';
import { PackageReader } from '../web/src/world/PackageReader';

const core = new EditorCore('world');
const project = core.newProject();
if (project.kind !== 'world') throw new Error('Expected world project');

const first = project.levels[0].id;
const addSecond = createNewWorldLevelOperation(project);
core.execute(addSecond.command);
const second = addSecond.levelId;

core.execute(createSetDefaultLevelCommand(project, second));
if (project.document.settings.defaultLevel !== second) {
  throw new Error('Set start level did not update the world default level');
}
core.undo();
if (project.document.settings.defaultLevel !== first) {
  throw new Error('Undo did not restore the previous start level');
}
core.redo();
if (project.document.settings.defaultLevel !== second) {
  throw new Error('Redo did not restore the selected start level');
}

const portal = createWorldPortalOperation(project, first, second);
core.execute(portal.command);
const connector = project.document.connectors?.find(value => value.id === portal.connectorId);
if (!connector ||
    connector.type !== 'portal' ||
    connector.fromLevel !== first ||
    connector.toLevel !== second ||
    connector.bidirectional !== true ||
    connector.forwardTraversal?.length !== 0 ||
    connector.reverseTraversal?.length !== 0) {
  throw new Error('Portal operation did not create a direct bidirectional world connector');
}

connector.fromPosition = [1, 2, 0];
connector.toPosition = [-3, 4, 0.5];

const preview = await new PackageReader().loadWorld(core.previewWorldBytes());
const previewConnector = preview.world.connectors?.find(value => value.id === portal.connectorId);
if (!previewConnector ||
    previewConnector.fromPosition[0] !== 1 ||
    previewConnector.toPosition[2] !== 0.5 ||
    preview.world.settings.defaultLevel !== second) {
  throw new Error('Start level or portal endpoints did not survive .isoworld preview packaging');
}

core.execute(createDeleteWorldConnectorCommand(project, portal.connectorId));
if (project.document.connectors?.some(value => value.id === portal.connectorId)) {
  throw new Error('Delete portal command did not remove the connector');
}
core.undo();
if (!project.document.connectors?.some(value => value.id === portal.connectorId)) {
  throw new Error('Undo did not restore the deleted portal');
}
core.redo();
if (project.document.connectors?.some(value => value.id === portal.connectorId)) {
  throw new Error('Redo did not remove the portal again');
}

let sameLevelRejected = false;
try {
  createWorldPortalOperation(project, first, first);
} catch {
  sameLevelRejected = true;
}
if (!sameLevelRejected) throw new Error('Portal creation allowed a self-link');

console.log('World connector smoke passed: start-level selection, direct portals, endpoint persistence, delete and undo/redo work.');
