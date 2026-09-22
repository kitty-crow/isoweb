import type { EditorCommand } from './CommandHistory';
import { EditorDocumentStore } from './DocumentStore';
import { validateEditorProject, type EditorProblem } from './EditorValidation';
import { createLevelProject, createWorldProject } from './ProjectFactory';
import { SelectionModel } from './Selection';
import {
  SourceProjectIO,
  type EditableSourceProject,
  type ProjectSource
} from './SourceProjectIO';

export type EditorKind = 'level' | 'world';

export class EditorCore {
  readonly store = new EditorDocumentStore();
  readonly selection = new SelectionModel();

  constructor(
    readonly kind: EditorKind,
    private readonly io = new SourceProjectIO()
  ) {}

  newProject(): EditableSourceProject {
    const project = this.kind === 'level' ? createLevelProject() : createWorldProject();
    this.store.load(project);
    this.selection.select({ kind: 'project', id: project.document.id });
    return project;
  }

  async open(source: ProjectSource): Promise<EditableSourceProject> {
    const project = this.kind === 'level'
      ? await this.io.openLevel(source)
      : await this.io.openWorld(source);
    this.store.load(project);
    this.selection.select({ kind: 'project', id: project.document.id });
    return project;
  }

  execute(command: EditorCommand): void {
    this.store.execute(command);
  }

  undo(): void { this.store.undo(); }
  redo(): void { this.store.redo(); }

  problems(): EditorProblem[] {
    return this.store.project ? validateEditorProject(this.store.project) : [];
  }

  saveBytes(): Uint8Array {
    const project = this.store.project;
    if (!project) throw new Error('No editable source project is open');
    const bytes = project.kind === 'level'
      ? this.io.saveLevelBytes(project)
      : this.io.saveWorldBytes(project);
    this.store.markSaved();
    return bytes;
  }

  saveBlob(): Blob {
    const bytes = this.saveBytes();
    return new Blob([bytes], { type: 'application/octet-stream' });
  }

  suggestedFilename(): string {
    const project = this.store.project;
    if (!project) return this.kind === 'level' ? 'level.isolevel' : 'world.isoworld';
    const safe = (project.document.name || project.document.id)
      .trim()
      .toLowerCase()
      .replace(/[^a-z0-9._-]+/g, '-')
      .replace(/^-+|-+$/g, '') || project.document.id;
    return `${safe}.${project.kind === 'level' ? 'isolevel' : 'isoworld'}`;
  }
}
