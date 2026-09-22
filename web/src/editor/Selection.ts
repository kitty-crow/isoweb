export type EditorSelectionKind =
  | 'project'
  | 'level'
  | 'entity'
  | 'geometry'
  | 'ground'
  | 'room'
  | 'spawn'
  | 'connector'
  | 'light'
  | 'material'
  | 'prefab';

export type EditorSelection = {
  kind: EditorSelectionKind;
  id: string;
  levelId?: string;
};

type SelectionListener = (selection: EditorSelection | null) => void;

export class SelectionModel {
  private current: EditorSelection | null = null;
  private readonly listeners = new Set<SelectionListener>();

  get value(): EditorSelection | null { return this.current; }

  select(selection: EditorSelection): void {
    this.current = { ...selection };
    this.emit();
  }

  clear(): void {
    if (!this.current) return;
    this.current = null;
    this.emit();
  }

  subscribe(listener: SelectionListener): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private emit(): void {
    for (const listener of this.listeners) listener(this.value);
  }
}
