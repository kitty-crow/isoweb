import {
  validateLevelDocument, validateLoadedWorldPackage, validateWorldDocument
} from '../world/validation';
import type { EditableSourceProject } from './SourceProjectIO';

export type EditorProblem = {
  severity: 'error' | 'warning';
  category: 'schema' | 'semantic' | 'runtime-capability';
  message: string;
};

function message(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export function validateEditorProject(project: EditableSourceProject): EditorProblem[] {
  try {
    if (project.kind === 'level') {
      validateLevelDocument(project.document);
    } else {
      validateWorldDocument(project.document);
      for (const level of project.levels) validateLevelDocument(level);
      validateLoadedWorldPackage({
        manifest: project.manifest,
        world: project.document,
        levels: project.levels,
        assets: project.assets
      });
    }
  } catch (error) {
    return [{ severity: 'error', category: 'semantic', message: message(error) }];
  }

  return [];
}
