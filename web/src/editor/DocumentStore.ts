import type { EditableSourceProject } from './SourceProjectIO';
import { CommandHistory, type EditorCommand } from './CommandHistory';

export type EditorStoreSnapshot = {
  project: EditableSourceProject | null;
  dirty: boolean;
  canUndo: boolean;
  canRedo: boolean;
  undoLabel?: string;
  redoLabel?: string;
};

type StoreListener = (snapshot: EditorStoreSnapshot) => void;

export class EditorDocumentStore {
  private projectValue: EditableSourceProject | null = null;
  private dirtyValue = false;
  private readonly history = new CommandHistory();
  private readonly listeners = new Set<StoreListener>();

  get project(): EditableSourceProject | null { return this.projectValue; }
  get dirty(): boolean { return this.dirtyValue; }
  get canUndo(): boolean { return this.history.canUndo; }
  get canRedo(): boolean { return this.history.canRedo; }

  load(project: EditableSourceProject): void {
    this.projectValue = project;
    this.dirtyValue = false;
    this.history.clear();
    this.emit();
  }

  execute(command: EditorCommand): void {
    if (!this.projectValue) throw new Error('No editable source project is open');
    this.history.execute(command);
    this.dirtyValue = true;
    this.emit();
  }

  undo(): void {
    if (!this.history.undo()) return;
    this.dirtyValue = true;
    this.emit();
  }

  redo(): void {
    if (!this.history.redo()) return;
    this.dirtyValue = true;
    this.emit();
  }

  markSaved(): void {
    if (!this.projectValue || !this.dirtyValue) return;
    this.dirtyValue = false;
    this.emit();
  }

  subscribe(listener: StoreListener): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  snapshot(): EditorStoreSnapshot {
    return {
      project: this.projectValue,
      dirty: this.dirtyValue,
      canUndo: this.history.canUndo,
      canRedo: this.history.canRedo,
      undoLabel: this.history.undoLabel,
      redoLabel: this.history.redoLabel
    };
  }

  private emit(): void {
    const snapshot = this.snapshot();
    for (const listener of this.listeners) listener(snapshot);
  }
}
