import { FunctionalCommand } from './CommandHistory';
import {
  createDeleteCommand, createDuplicateOperation, createLocalAddOperation,
  selectedLevel, type LocalAddKind
} from './AuthoringCommands';
import { EditorCore } from './EditorCore';
import { EditorLayoutViewport } from './EditorLayoutViewport';
import type { EditorSelection, EditorSelectionKind } from './Selection';
import type { EditableSourceProject } from './SourceProjectIO';
import type { LevelDocument, Vec3Tuple } from '../world/documents';

type IndexedRecord = Record<string, unknown>;

export class EditorApp {
  private transientProblem = '';
  private layoutViewport: EditorLayoutViewport | null = null;

  constructor(
    private readonly core: EditorCore,
    private readonly productName: string
  ) {}

  start(): void {
    this.bindToolbar();
    this.bindHierarchyControls();
    this.layoutViewport = new EditorLayoutViewport(
      this.core,
      this.element<HTMLElement>('editor-layout')
    );
    this.core.store.subscribe(() => this.render());
    this.core.selection.subscribe(() => this.render());
    window.addEventListener('beforeunload', event => {
      if (!this.core.store.dirty) return;
      event.preventDefault();
      event.returnValue = '';
    });
    this.core.newProject();
    this.render();
  }

  private element<T extends HTMLElement>(id: string): T {
    const value = document.getElementById(id);
    if (!value) throw new Error(`Missing editor element #${id}`);
    return value as T;
  }

  private bindToolbar(): void {
    this.element<HTMLButtonElement>('editor-new').addEventListener('click', () => {
      if (!this.discardAllowed()) return;
      this.transientProblem = '';
      this.core.newProject();
    });

    const input = this.element<HTMLInputElement>('editor-open-file');
    this.element<HTMLButtonElement>('editor-open').addEventListener('click', () => {
      if (!this.discardAllowed()) return;
      input.value = '';
      input.click();
    });
    input.addEventListener('change', () => {
      const file = input.files?.[0];
      if (!file) return;
      void this.openFile(file);
    });

    this.element<HTMLButtonElement>('editor-save').addEventListener('click', () => this.save());
    this.element<HTMLButtonElement>('editor-undo').addEventListener('click', () => this.core.undo());
    this.element<HTMLButtonElement>('editor-redo').addEventListener('click', () => this.core.redo());

    this.element<HTMLButtonElement>('editor-add').addEventListener('click', () => {
      const project = this.core.store.project;
      if (!project) return;
      try {
        const kind = this.element<HTMLSelectElement>('editor-add-kind').value as LocalAddKind;
        const operation = createLocalAddOperation(project, this.core.selection.value, kind);
        this.core.execute(operation.command);
        this.core.selection.select(operation.selection);
        this.transientProblem = '';
      } catch (error) {
        this.transientProblem = error instanceof Error ? error.message : String(error);
        this.renderProblems();
      }
    });

    this.element<HTMLButtonElement>('editor-duplicate').addEventListener('click', () => {
      const project = this.core.store.project;
      const selection = this.core.selection.value;
      if (!project || !selection) return;
      try {
        const operation = createDuplicateOperation(project, selection);
        this.core.execute(operation.command);
        this.core.selection.select(operation.selection);
        this.transientProblem = '';
      } catch (error) {
        this.transientProblem = error instanceof Error ? error.message : String(error);
        this.renderProblems();
      }
    });

    this.element<HTMLButtonElement>('editor-delete').addEventListener('click', () => {
      const project = this.core.store.project;
      const selection = this.core.selection.value;
      if (!project || !selection) return;
      try {
        const level = selectedLevel(project, selection);
        this.core.execute(createDeleteCommand(project, selection));
        this.core.selection.select({ kind: 'level', id: level.id, levelId: level.id });
        this.transientProblem = '';
      } catch (error) {
        this.transientProblem = error instanceof Error ? error.message : String(error);
        this.renderProblems();
      }
    });
  }

  private bindHierarchyControls(): void {
    this.element<HTMLInputElement>('editor-hierarchy-search')
      .addEventListener('input', () => this.renderHierarchy());
    this.element<HTMLSelectElement>('editor-hierarchy-filter')
      .addEventListener('change', () => this.renderHierarchy());
  }

  private discardAllowed(): boolean {
    return !this.core.store.dirty ||
      window.confirm('Discard unsaved changes to the current source project?');
  }

  private async openFile(file: File): Promise<void> {
    try {
      await this.core.open(file);
      this.transientProblem = '';
    } catch (error) {
      this.transientProblem = error instanceof Error ? error.message : String(error);
      this.renderProblems();
    }
  }

  private save(): void {
    try {
      const blob = this.core.saveBlob();
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = this.core.suggestedFilename();
      link.click();
      queueMicrotask(() => URL.revokeObjectURL(url));
      this.transientProblem = '';
      this.render();
    } catch (error) {
      this.transientProblem = error instanceof Error ? error.message : String(error);
      this.renderProblems();
    }
  }

  private render(): void {
    const snapshot = this.core.store.snapshot();
    const project = snapshot.project;
    if (!project) return;

    const title = project.document.name || project.document.id;
    this.element('editor-product').textContent = this.productName;
    this.element('editor-project-title').textContent = `${title}${snapshot.dirty ? ' •' : ''}`;
    document.title = `${title} · ${this.productName} · IsoWeb`;

    const undo = this.element<HTMLButtonElement>('editor-undo');
    undo.disabled = !snapshot.canUndo;
    undo.title = snapshot.undoLabel ? `Undo ${snapshot.undoLabel}` : 'Undo';
    const redo = this.element<HTMLButtonElement>('editor-redo');
    redo.disabled = !snapshot.canRedo;
    redo.title = snapshot.redoLabel ? `Redo ${snapshot.redoLabel}` : 'Redo';

    const selection = this.core.selection.value;
    const localEditable = new Set([
      'ground', 'entity', 'geometry', 'room', 'spawn', 'connector', 'light'
    ]);
    const canOperate = !!selection && localEditable.has(selection.kind);
    this.element<HTMLButtonElement>('editor-duplicate').disabled = !canOperate;
    this.element<HTMLButtonElement>('editor-delete').disabled = !canOperate;

    this.renderHierarchy();
    this.renderInspector();
    this.renderOverview();
    this.layoutViewport?.render();
    this.renderProblems();
  }

  private renderHierarchy(): void {
    const project = this.core.store.project;
    if (!project) return;
    const root = this.element('editor-hierarchy');
    root.replaceChildren();

    const search = this.element<HTMLInputElement>('editor-hierarchy-search').value.trim().toLowerCase();
    const filter = this.element<HTMLSelectElement>('editor-hierarchy-filter').value;

    const add = (
      label: string,
      kind: EditorSelectionKind,
      id: string,
      levelId?: string,
      depth = 0
    ): void => {
      if (filter !== 'all' && filter !== kind) return;
      if (search && !label.toLowerCase().includes(search) && !id.toLowerCase().includes(search)) return;
      const button = document.createElement('button');
      button.type = 'button';
      button.className = 'editor-tree-item';
      button.style.setProperty('--tree-depth', String(depth));
      button.textContent = label;
      const current = this.core.selection.value;
      if (current?.kind === kind && current.id === id && current.levelId === levelId) {
        button.dataset.selected = 'true';
      }
      button.addEventListener('click', () => this.core.selection.select({ kind, id, levelId }));
      root.appendChild(button);
    };

    add(project.document.name || project.document.id, 'project', project.document.id);
    const levels = project.kind === 'level' ? [project.document] : project.levels;
    for (const level of levels) {
      add(level.name || level.id, 'level', level.id, level.id, 1);
      for (const ground of level.ground) add(ground.id, 'ground', ground.id, level.id, 2);
      for (const entity of level.entities) add(entity.id, 'entity', entity.id, level.id, 2);
      for (const geometry of level.geometry) add(geometry.id, 'geometry', geometry.id, level.id, 2);
      for (const room of level.rooms ?? []) add(room.id, 'room', room.id, level.id, 2);
      for (const spawn of level.spawns) add(spawn.id, 'spawn', spawn.id, level.id, 2);
      for (const connector of level.connectors) add(connector.id, 'connector', connector.id, level.id, 2);
      for (const light of level.lights) add(light.id, 'light', light.id, level.id, 2);
      for (const id of Object.keys(level.localMaterials)) add(id, 'material', id, level.id, 2);
      for (const id of Object.keys(level.localPrefabs ?? {})) add(id, 'prefab', id, level.id, 2);
    }
    if (project.kind === 'world') {
      for (const id of Object.keys(project.document.materials)) add(id, 'material', id, undefined, 1);
      for (const id of Object.keys(project.document.prefabs)) add(id, 'prefab', id, undefined, 1);
    }
  }

  private renderInspector(): void {
    const root = this.element('editor-inspector');
    root.replaceChildren();
    const project = this.core.store.project;
    if (!project) return;
    const selection = this.core.selection.value ?? { kind: 'project', id: project.document.id };
    const target = this.resolveSelection(project, selection);

    const heading = document.createElement('h2');
    heading.textContent = this.selectionTitle(selection, target);
    root.appendChild(heading);

    if (!target) {
      const empty = document.createElement('p');
      empty.textContent = 'The selected source object no longer exists.';
      root.appendChild(empty);
      return;
    }

    if (selection.kind === 'project' || selection.kind === 'level') {
      this.addTextProperty(root, 'Name', (target as { name?: string }).name ?? '', value => {
        const record = target as { name?: string };
        const before = record.name;
        this.core.execute(new FunctionalCommand(
          'rename',
          () => { record.name = value || undefined; },
          () => { record.name = before; }
        ));
      });
    }

    const tuple = this.editableTuple(selection, target);
    if (tuple) this.addTupleProperty(root, tuple.label, tuple.value);

    if (selection.kind === 'material') {
      const material = target as IndexedRecord;
      if (Array.isArray(material.baseColour)) {
        this.addTupleProperty(root, 'Base colour', material.baseColour as Vec3Tuple);
      }
    }

    const record = target as IndexedRecord;
    if (selection.kind === 'ground' && Array.isArray(record.size)) {
      this.addNumberProperty(root, 'Width', record.size[0] as number, value => {
        const size = record.size as [number, number];
        const before = size[0];
        this.core.execute(new FunctionalCommand(
          'resize ground width',
          () => { size[0] = Math.max(0.25, value); },
          () => { size[0] = before; }
        ));
      });
      this.addNumberProperty(root, 'Depth', record.size[1] as number, value => {
        const size = record.size as [number, number];
        const before = size[1];
        this.core.execute(new FunctionalCommand(
          'resize ground depth',
          () => { size[1] = Math.max(0.25, value); },
          () => { size[1] = before; }
        ));
      });
    }

    if (selection.kind === 'geometry') {
      this.addNumberProperty(root, 'Size', Number(record.size), value => {
        const before = Number(record.size);
        this.core.execute(new FunctionalCommand(
          'resize geometry',
          () => { record.size = Math.max(0.25, value); },
          () => { record.size = before; }
        ));
      });
      if (record.height !== undefined) {
        this.addNumberProperty(root, 'Height', Number(record.height), value => {
          const before = Number(record.height);
          this.core.execute(new FunctionalCommand(
            'set geometry height',
            () => { record.height = Math.max(0, value); },
            () => { record.height = before; }
          ));
        });
      }
    }

    if (selection.kind === 'room') {
      for (const [key, label, minimum] of [
        ['width', 'Width', 0.25],
        ['depth', 'Depth', 0.25],
        ['floorZ', 'Floor Z', -Infinity],
        ['wallHeight', 'Wall height', 0],
        ['wallThickness', 'Wall thickness', 0.01]
      ] as const) {
        this.addNumberProperty(root, label, Number(record[key]), value => {
          const before = Number(record[key]);
          this.core.execute(new FunctionalCommand(
            `set room ${label.toLowerCase()}`,
            () => { record[key] = Math.max(minimum, value); },
            () => { record[key] = before; }
          ));
        });
      }
    }

    if (selection.kind === 'connector') {
      if (Array.isArray(record.fromPosition)) {
        this.addTupleProperty(root, 'From', record.fromPosition as Vec3Tuple);
      }
      if (Array.isArray(record.toPosition)) {
        this.addTupleProperty(root, 'To', record.toPosition as Vec3Tuple);
      }
    }

    if (selection.kind === 'entity') {
      const components = record.components as IndexedRecord | undefined;
      const transform = components?.transform as IndexedRecord | undefined;
      if (Array.isArray(transform?.forward)) {
        this.addTupleProperty(root, 'Facing', transform.forward as Vec3Tuple);
      }
    }

    if (selection.kind === 'spawn') {
      const transform = record.transform as IndexedRecord | undefined;
      if (Array.isArray(transform?.forward)) {
        this.addTupleProperty(root, 'Facing', transform.forward as Vec3Tuple);
      }
    }

    const id = (target as { id?: unknown }).id;
    if (typeof id === 'string') {
      const field = document.createElement('div');
      field.className = 'editor-field';
      const label = document.createElement('span');
      label.textContent = 'ID';
      const code = document.createElement('code');
      code.textContent = id;
      field.append(label, code);
      root.appendChild(field);
    }
  }

  private addTextProperty(
    root: HTMLElement,
    labelText: string,
    value: string,
    commit: (value: string) => void
  ): void {
    const label = document.createElement('label');
    label.className = 'editor-field';
    const title = document.createElement('span');
    title.textContent = labelText;
    const input = document.createElement('input');
    input.type = 'text';
    input.value = value;
    input.addEventListener('change', () => {
      if (input.value === value) return;
      commit(input.value);
    });
    label.append(title, input);
    root.appendChild(label);
  }

  private addNumberProperty(
    root: HTMLElement,
    labelText: string,
    value: number,
    commit: (value: number) => void
  ): void {
    const label = document.createElement('label');
    label.className = 'editor-field';
    const title = document.createElement('span');
    title.textContent = labelText;
    const input = document.createElement('input');
    input.type = 'number';
    input.step = 'any';
    input.value = String(value);
    input.addEventListener('change', () => {
      const next = Number(input.value);
      if (!Number.isFinite(next) || next === value) {
        input.value = String(value);
        return;
      }
      commit(next);
    });
    label.append(title, input);
    root.appendChild(label);
  }

  private addTupleProperty(root: HTMLElement, labelText: string, tuple: Vec3Tuple): void {
    const group = document.createElement('fieldset');
    group.className = 'editor-vector';
    const legend = document.createElement('legend');
    legend.textContent = labelText;
    group.appendChild(legend);

    ['X', 'Y', 'Z'].forEach((axis, index) => {
      const label = document.createElement('label');
      const span = document.createElement('span');
      span.textContent = axis;
      const input = document.createElement('input');
      input.type = 'number';
      input.step = 'any';
      input.value = String(tuple[index]);
      input.addEventListener('change', () => {
        const next = Number(input.value);
        if (!Number.isFinite(next) || next === tuple[index]) {
          input.value = String(tuple[index]);
          return;
        }
        const before = tuple[index];
        this.core.execute(new FunctionalCommand(
          `set ${labelText} ${axis}`,
          () => { tuple[index] = next; },
          () => { tuple[index] = before; }
        ));
      });
      label.append(span, input);
      group.appendChild(label);
    });

    root.appendChild(group);
  }

  private editableTuple(
    selection: EditorSelection,
    target: unknown
  ): { label: string; value: Vec3Tuple } | null {
    const record = target as IndexedRecord;
    if (selection.kind === 'level' && Array.isArray(record.viewOrigin)) {
      return { label: 'View origin', value: record.viewOrigin as Vec3Tuple };
    }
    if (selection.kind === 'entity') {
      const components = record.components as IndexedRecord | undefined;
      const transform = components?.transform as IndexedRecord | undefined;
      if (Array.isArray(transform?.position)) {
        return { label: 'Position', value: transform.position as Vec3Tuple };
      }
    }
    if (selection.kind === 'spawn') {
      const transform = record.transform as IndexedRecord | undefined;
      if (Array.isArray(transform?.position)) {
        return { label: 'Position', value: transform.position as Vec3Tuple };
      }
    }
    if (selection.kind === 'ground' && Array.isArray(record.centre)) {
      return { label: 'Centre', value: record.centre as Vec3Tuple };
    }
    if (['geometry', 'light'].includes(selection.kind) && Array.isArray(record.position)) {
      return { label: 'Position', value: record.position as Vec3Tuple };
    }
    if (selection.kind === 'room' && Array.isArray(record.centre)) {
      return { label: 'Centre', value: record.centre as Vec3Tuple };
    }
    return null;
  }

  private renderOverview(): void {
    const project = this.core.store.project;
    if (!project) return;
    const root = this.element('editor-overview');
    root.replaceChildren();

    const levels = project.kind === 'level' ? [project.document] : project.levels;
    const stats = [
      ['Levels', levels.length],
      ['Ground', levels.reduce((sum, level) => sum + level.ground.length, 0)],
      ['Entities', levels.reduce((sum, level) => sum + level.entities.length, 0)],
      ['Geometry', levels.reduce((sum, level) => sum + level.geometry.length, 0)],
      ['Rooms', levels.reduce((sum, level) => sum + (level.rooms?.length ?? 0), 0)],
      ['Spawns', levels.reduce((sum, level) => sum + level.spawns.length, 0)],
      ['Connectors', levels.reduce((sum, level) => sum + level.connectors.length, 0)]
    ];

    for (const [label, value] of stats) {
      const item = document.createElement('div');
      item.className = 'editor-stat';
      const strong = document.createElement('strong');
      strong.textContent = String(value);
      const span = document.createElement('span');
      span.textContent = String(label);
      item.append(strong, span);
      root.appendChild(item);
    }
  }

  private renderProblems(): void {
    const root = this.element('editor-problems');
    root.replaceChildren();
    const problems = this.core.problems();
    if (this.transientProblem) {
      problems.unshift({
        severity: 'error',
        category: 'schema',
        message: this.transientProblem
      });
    }

    if (problems.length === 0) {
      const okay = document.createElement('p');
      okay.className = 'editor-problem-ok';
      okay.textContent = 'No validation problems.';
      root.appendChild(okay);
      return;
    }

    for (const problem of problems) {
      const item = document.createElement('p');
      item.className = 'editor-problem';
      item.dataset.severity = problem.severity;
      item.textContent = `${problem.category}: ${problem.message}`;
      root.appendChild(item);
    }
  }

  private resolveSelection(
    project: EditableSourceProject,
    selection: EditorSelection
  ): unknown {
    if (selection.kind === 'project') return project.document;
    const level = this.findLevel(project, selection.levelId ?? selection.id);
    if (selection.kind === 'level') return level;
    if (!level) {
      if (project.kind === 'world' && selection.kind === 'material') {
        return project.document.materials[selection.id];
      }
      if (project.kind === 'world' && selection.kind === 'prefab') {
        return project.document.prefabs[selection.id];
      }
      return undefined;
    }

    switch (selection.kind) {
      case 'ground': return level.ground.find(value => value.id === selection.id);
      case 'entity': return level.entities.find(value => value.id === selection.id);
      case 'geometry': return level.geometry.find(value => value.id === selection.id);
      case 'room': return level.rooms?.find(value => value.id === selection.id);
      case 'spawn': return level.spawns.find(value => value.id === selection.id);
      case 'connector': return level.connectors.find(value => value.id === selection.id);
      case 'light': return level.lights.find(value => value.id === selection.id);
      case 'material': return level.localMaterials[selection.id];
      case 'prefab': return level.localPrefabs?.[selection.id];
      default: return undefined;
    }
  }

  private findLevel(project: EditableSourceProject, id: string): LevelDocument | undefined {
    return project.kind === 'level'
      ? (project.document.id === id ? project.document : undefined)
      : project.levels.find(level => level.id === id);
  }

  private selectionTitle(selection: EditorSelection, target: unknown): string {
    const record = target as { id?: string; name?: string } | undefined;
    const label = record?.name || record?.id || selection.id;
    return `${selection.kind[0].toUpperCase()}${selection.kind.slice(1)} · ${label}`;
  }
}
