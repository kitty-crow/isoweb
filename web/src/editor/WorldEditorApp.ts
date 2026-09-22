import { FunctionalCommand } from './CommandHistory';
import { EditorCore } from './EditorCore';
import { EditorPlayPreview } from './EditorPlayPreview';
import {
  createDeleteWorldConnectorCommand,
  createSetDefaultLevelCommand,
  createWorldConnectionOperation
} from './WorldConnectorManager';
import { WorldCompositionViewport } from './WorldCompositionViewport';
import {
  createDeleteWorldLevelCommand,
  createDuplicateWorldLevelOperation,
  createImportWorldLevelOperation
} from './WorldLevelManager';
import {
  createSetLevelPlacementCommand,
  createStackRelativeCommand,
  effectiveLevelOrigin,
  levelPlacement
} from './WorldPlacementManager';
import type { EditableWorldProject } from './SourceProjectIO';
import type { Vec3Tuple, WorldConnectorDefinition } from '../world/documents';

export class WorldEditorApp {
  private transientProblem = '';
  private viewport: WorldCompositionViewport | null = null;
  private preview: EditorPlayPreview | null = null;

  constructor(private readonly core: EditorCore) {}

  start(): void {
    this.bindToolbar();
    this.bindLevelControls();
    this.bindConnectionControls();

    this.viewport = new WorldCompositionViewport(
      this.core,
      this.element<HTMLElement>('world-composition')
    );
    this.preview = new EditorPlayPreview(
      this.core,
      this.element<HTMLButtonElement>('editor-play'),
      this.element<HTMLElement>('world-composition-shell'),
      this.element<HTMLIFrameElement>('editor-runtime-frame')
    );

    this.core.store.subscribe(() => this.render());
    this.core.selection.subscribe(() => this.render());
    window.addEventListener('beforeunload', event => {
      if (!this.core.store.dirty) return;
      event.preventDefault();
      event.returnValue = '';
    });

    const project = this.core.newProject();
    if (project.kind === 'world' && project.levels[0]) {
      this.core.selection.select({
        kind: 'level',
        id: project.levels[0].id,
        levelId: project.levels[0].id
      });
    }
    this.render();
  }

  private element<T extends HTMLElement>(id: string): T {
    const value = document.getElementById(id);
    if (!value) throw new Error(`Missing World Editor element #${id}`);
    return value as T;
  }

  private project(): EditableWorldProject {
    const project = this.core.store.project;
    if (!project || project.kind !== 'world') throw new Error('No world project is open');
    return project;
  }

  private selectedLevelId(): string | undefined {
    const project = this.project();
    const selection = this.core.selection.value;
    const candidate = selection?.kind === 'level'
      ? selection.id
      : selection?.levelId ?? project.document.settings.defaultLevel;
    return project.levels.some(level => level.id === candidate) ? candidate : undefined;
  }

  private bindToolbar(): void {
    this.element<HTMLButtonElement>('editor-new').addEventListener('click', () => {
      if (!this.discardAllowed()) return;
      this.preview?.exit();
      this.transientProblem = '';
      const project = this.core.newProject();
      if (project.kind === 'world' && project.levels[0]) {
        this.core.selection.select({
          kind: 'level',
          id: project.levels[0].id,
          levelId: project.levels[0].id
        });
      }
    });

    const openInput = this.element<HTMLInputElement>('editor-open-file');
    this.element<HTMLButtonElement>('editor-open').addEventListener('click', () => {
      if (!this.discardAllowed()) return;
      this.preview?.exit();
      openInput.value = '';
      openInput.click();
    });
    openInput.addEventListener('change', () => {
      const file = openInput.files?.[0];
      if (file) void this.openWorld(file);
    });

    this.element<HTMLButtonElement>('editor-save').addEventListener('click', () => this.save());
    this.element<HTMLButtonElement>('editor-undo').addEventListener('click', () => this.core.undo());
    this.element<HTMLButtonElement>('editor-redo').addEventListener('click', () => this.core.redo());

    this.element<HTMLButtonElement>('editor-play').addEventListener('click', () => {
      try {
        if (this.preview?.isPlaying) this.preview.exit();
        else this.preview?.enter();
        this.transientProblem = '';
      } catch (error) {
        this.captureProblem(error);
      }
    });
  }

  private bindLevelControls(): void {
    const importInput = this.element<HTMLInputElement>('world-level-import-file');
    this.element<HTMLButtonElement>('world-level-import').addEventListener('click', () => {
      importInput.value = '';
      importInput.click();
    });
    importInput.addEventListener('change', () => {
      const file = importInput.files?.[0];
      if (file) void this.importLevel(file);
    });

    this.element<HTMLButtonElement>('world-level-duplicate').addEventListener('click', () => {
      const levelId = this.selectedLevelId();
      if (!levelId) return;
      try {
        const project = this.project();
        const operation = createDuplicateWorldLevelOperation(project, levelId);
        this.core.execute(operation.command);
        this.core.selection.select({
          kind: 'level',
          id: operation.levelId,
          levelId: operation.levelId
        });
        this.transientProblem = '';
      } catch (error) {
        this.captureProblem(error);
      }
    });

    this.element<HTMLButtonElement>('world-level-delete').addEventListener('click', () => {
      const levelId = this.selectedLevelId();
      if (!levelId) return;
      try {
        const project = this.project();
        const fallback = project.levels.find(level => level.id !== levelId);
        this.core.execute(createDeleteWorldLevelCommand(project, levelId));
        if (fallback) {
          this.core.selection.select({
            kind: 'level',
            id: fallback.id,
            levelId: fallback.id
          });
        }
        this.transientProblem = '';
      } catch (error) {
        this.captureProblem(error);
      }
    });

    this.element<HTMLButtonElement>('world-level-set-default').addEventListener('click', () => {
      const levelId = this.selectedLevelId();
      if (!levelId) return;
      try {
        this.core.execute(createSetDefaultLevelCommand(this.project(), levelId));
        this.core.selection.select({ kind: 'level', id: levelId, levelId });
        this.transientProblem = '';
      } catch (error) {
        this.captureProblem(error);
      }
    });

    this.element<HTMLButtonElement>('world-stack-above').addEventListener('click', () => {
      this.stackSelected('above');
    });
    this.element<HTMLButtonElement>('world-stack-below').addEventListener('click', () => {
      this.stackSelected('below');
    });
  }

  private bindConnectionControls(): void {
    this.element<HTMLButtonElement>('world-connector-add').addEventListener('click', () => {
      const fromLevel = this.selectedLevelId();
      const toLevel = this.element<HTMLSelectElement>('world-connector-target').value;
      const type = this.element<HTMLSelectElement>('world-connector-type').value as 'portal' | 'stairs';
      if (!fromLevel || !toLevel) return;
      try {
        const operation = createWorldConnectionOperation(
          this.project(),
          fromLevel,
          toLevel,
          type
        );
        this.core.execute(operation.command);
        this.core.selection.select({ kind: 'connector', id: operation.connectorId });
        this.transientProblem = '';
      } catch (error) {
        this.captureProblem(error);
      }
    });

    this.element<HTMLButtonElement>('world-connector-delete').addEventListener('click', () => {
      const selection = this.core.selection.value;
      if (selection?.kind !== 'connector' || selection.levelId) return;
      try {
        const project = this.project();
        const connector = project.document.connectors?.find(value => value.id === selection.id);
        if (!connector) return;
        this.core.execute(createDeleteWorldConnectorCommand(project, connector.id));
        this.core.selection.select({
          kind: 'level',
          id: connector.fromLevel,
          levelId: connector.fromLevel
        });
        this.transientProblem = '';
      } catch (error) {
        this.captureProblem(error);
      }
    });
  }

  private async openWorld(file: File): Promise<void> {
    try {
      const project = await this.core.open(file);
      if (project.kind === 'world' && project.levels[0]) {
        this.core.selection.select({
          kind: 'level',
          id: project.document.settings.defaultLevel,
          levelId: project.document.settings.defaultLevel
        });
      }
      this.transientProblem = '';
    } catch (error) {
      this.captureProblem(error);
    }
  }

  private async importLevel(file: File): Promise<void> {
    try {
      const operation = await createImportWorldLevelOperation(this.project(), file);
      this.core.execute(operation.command);
      this.core.selection.select({
        kind: 'level',
        id: operation.levelId,
        levelId: operation.levelId
      });
      this.transientProblem = '';
    } catch (error) {
      this.captureProblem(error);
    }
  }

  private stackSelected(direction: 'above' | 'below'): void {
    const levelId = this.selectedLevelId();
    const target = this.element<HTMLSelectElement>('world-stack-target').value;
    const gap = Number(this.element<HTMLInputElement>('world-stack-gap').value);
    if (!levelId || !target) return;
    try {
      this.core.execute(createStackRelativeCommand(
        this.project(),
        levelId,
        target,
        direction,
        gap
      ));
      this.core.selection.select({ kind: 'level', id: levelId, levelId });
      this.transientProblem = '';
    } catch (error) {
      this.captureProblem(error);
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
      this.captureProblem(error);
    }
  }

  private discardAllowed(): boolean {
    return !this.core.store.dirty ||
      window.confirm('Discard unsaved changes to the current world?');
  }

  private captureProblem(error: unknown): void {
    this.transientProblem = error instanceof Error ? error.message : String(error);
    this.renderProblems();
  }

  private render(): void {
    const project = this.project();
    const snapshot = this.core.store.snapshot();

    this.element('editor-project-title').textContent =
      `${project.document.name || project.document.id}${snapshot.dirty ? ' •' : ''}`;
    document.title = `${project.document.name || project.document.id} · World Editor · IsoWeb`;

    const undo = this.element<HTMLButtonElement>('editor-undo');
    undo.disabled = !snapshot.canUndo;
    undo.title = snapshot.undoLabel ? `Undo ${snapshot.undoLabel}` : 'Undo';
    const redo = this.element<HTMLButtonElement>('editor-redo');
    redo.disabled = !snapshot.canRedo;
    redo.title = snapshot.redoLabel ? `Redo ${snapshot.redoLabel}` : 'Redo';

    this.renderLevelList(project);
    this.renderConnectionList(project);
    this.renderLevelControls(project);
    this.renderInspector(project);
    this.renderOverview(project);
    this.viewport?.render();
    this.renderProblems();
  }

  private renderLevelList(project: EditableWorldProject): void {
    const root = this.element('world-level-list');
    root.replaceChildren();
    for (const level of project.levels) {
      const button = document.createElement('button');
      button.type = 'button';
      button.className = 'world-list-item';
      const origin = effectiveLevelOrigin(project, level.id);
      const selected = this.core.selection.value?.kind === 'level' &&
        this.core.selection.value.id === level.id;
      if (selected) button.dataset.selected = 'true';

      const strong = document.createElement('strong');
      strong.textContent = `${project.document.settings.defaultLevel === level.id ? '★ ' : ''}${level.name || level.id}`;
      const meta = document.createElement('span');
      meta.textContent = `X ${origin[0].toFixed(1)} · Y ${origin[1].toFixed(1)} · Z ${origin[2].toFixed(1)}`;
      button.append(strong, meta);
      button.addEventListener('click', () => {
        this.core.selection.select({ kind: 'level', id: level.id, levelId: level.id });
      });
      root.appendChild(button);
    }
  }

  private renderConnectionList(project: EditableWorldProject): void {
    const root = this.element('world-connection-list');
    root.replaceChildren();
    for (const connector of project.document.connectors ?? []) {
      const button = document.createElement('button');
      button.type = 'button';
      button.className = 'world-list-item';
      if (this.core.selection.value?.kind === 'connector' &&
          this.core.selection.value.id === connector.id) {
        button.dataset.selected = 'true';
      }
      const strong = document.createElement('strong');
      strong.textContent = connector.type === 'stairs' ? 'Stairs' : 'Portal';
      const meta = document.createElement('span');
      meta.textContent = `${connector.fromLevel} → ${connector.toLevel}`;
      button.append(strong, meta);
      button.addEventListener('click', () => {
        this.core.selection.select({ kind: 'connector', id: connector.id });
      });
      root.appendChild(button);
    }
  }

  private renderLevelControls(project: EditableWorldProject): void {
    const levelId = this.selectedLevelId();
    const selected = !!levelId;

    this.element<HTMLButtonElement>('world-level-duplicate').disabled = !selected;
    this.element<HTMLButtonElement>('world-level-delete').disabled =
      !selected || project.levels.length <= 1;
    this.element<HTMLButtonElement>('world-level-set-default').disabled =
      !selected || project.document.settings.defaultLevel === levelId;

    const fillTargets = (select: HTMLSelectElement): void => {
      const previous = select.value;
      select.replaceChildren();
      for (const level of project.levels) {
        if (level.id === levelId) continue;
        const option = document.createElement('option');
        option.value = level.id;
        option.textContent = level.name || level.id;
        select.appendChild(option);
      }
      if (Array.from(select.options).some(option => option.value === previous)) {
        select.value = previous;
      }
      select.disabled = !selected || select.options.length === 0;
    };

    const stackTarget = this.element<HTMLSelectElement>('world-stack-target');
    const connectorTarget = this.element<HTMLSelectElement>('world-connector-target');
    fillTargets(stackTarget);
    fillTargets(connectorTarget);

    this.element<HTMLButtonElement>('world-stack-above').disabled = stackTarget.disabled;
    this.element<HTMLButtonElement>('world-stack-below').disabled = stackTarget.disabled;
    this.element<HTMLButtonElement>('world-connector-add').disabled = connectorTarget.disabled;

    const connectorSelected = this.core.selection.value?.kind === 'connector' &&
      !this.core.selection.value.levelId;
    this.element<HTMLButtonElement>('world-connector-delete').disabled = !connectorSelected;
  }

  private renderInspector(project: EditableWorldProject): void {
    const root = this.element('world-inspector');
    root.replaceChildren();
    const selection = this.core.selection.value;

    if (selection?.kind === 'connector' && !selection.levelId) {
      const connector = project.document.connectors?.find(value => value.id === selection.id);
      if (connector) this.renderConnectorInspector(root, connector);
      return;
    }

    const levelId = this.selectedLevelId();
    if (!levelId) {
      const p = document.createElement('p');
      p.textContent = 'Select a level or connection.';
      root.appendChild(p);
      return;
    }

    const level = project.levels.find(value => value.id === levelId)!;
    const heading = document.createElement('h2');
    heading.textContent = level.name || level.id;
    root.appendChild(heading);

    const note = document.createElement('p');
    note.className = 'world-inspector-note';
    note.textContent =
      'World placement only. Edit rooms, geometry and gameplay content in Level Builder.';
    root.appendChild(note);

    const placement = levelPlacement(project, levelId);
    this.addVectorEditor(root, 'World offset', placement, next => {
      this.core.execute(createSetLevelPlacementCommand(project, levelId, next));
      this.core.selection.select({ kind: 'level', id: levelId, levelId });
    });

    const effective = effectiveLevelOrigin(project, levelId);
    const effectiveField = document.createElement('div');
    effectiveField.className = 'editor-field';
    effectiveField.innerHTML =
      `<span>Effective origin</span><code>${effective.map(v => v.toFixed(2)).join(', ')}</code>`;
    root.appendChild(effectiveField);

    const summary = document.createElement('div');
    summary.className = 'world-source-summary';
    summary.innerHTML =
      `<strong>Source level</strong><span>${level.ground.length} ground · ${level.rooms?.length ?? 0} rooms · ${level.geometry.length} geometry · ${level.entities.length} entities</span>`;
    root.appendChild(summary);
  }

  private renderConnectorInspector(
    root: HTMLElement,
    connector: WorldConnectorDefinition
  ): void {
    const heading = document.createElement('h2');
    heading.textContent = connector.type === 'stairs' ? 'Inter-level stairs' : 'Portal';
    root.appendChild(heading);

    const relation = document.createElement('p');
    relation.className = 'world-inspector-note';
    relation.textContent = `${connector.fromLevel} → ${connector.toLevel}`;
    root.appendChild(relation);

    this.addVectorEditor(root, 'From position', connector.fromPosition, next => {
      const before = [...connector.fromPosition] as Vec3Tuple;
      this.core.execute(new FunctionalCommand(
        'move connection source',
        () => { connector.fromPosition.splice(0, 3, ...next); },
        () => { connector.fromPosition.splice(0, 3, ...before); }
      ));
    });
    this.addVectorEditor(root, 'To position', connector.toPosition, next => {
      const before = [...connector.toPosition] as Vec3Tuple;
      this.core.execute(new FunctionalCommand(
        'move connection target',
        () => { connector.toPosition.splice(0, 3, ...next); },
        () => { connector.toPosition.splice(0, 3, ...before); }
      ));
    });

    if (connector.type === 'stairs') {
      const p = document.createElement('p');
      p.className = 'world-inspector-note';
      p.textContent =
        'Traversal samples were generated from the current level placement. Recreate the stairs after moving levels if you want the flight regenerated automatically.';
      root.appendChild(p);
    }
  }

  private addVectorEditor(
    root: HTMLElement,
    labelText: string,
    value: Vec3Tuple,
    commit: (next: Vec3Tuple) => void
  ): void {
    const fieldset = document.createElement('fieldset');
    fieldset.className = 'editor-vector';
    const legend = document.createElement('legend');
    legend.textContent = labelText;
    fieldset.appendChild(legend);

    (['X', 'Y', 'Z'] as const).forEach((axis, index) => {
      const label = document.createElement('label');
      const span = document.createElement('span');
      span.textContent = axis;
      const input = document.createElement('input');
      input.type = 'number';
      input.step = 'any';
      input.value = String(value[index]);
      input.addEventListener('change', () => {
        const numeric = Number(input.value);
        if (!Number.isFinite(numeric) || numeric === value[index]) return;
        const next = [...value] as Vec3Tuple;
        next[index] = numeric;
        commit(next);
      });
      label.append(span, input);
      fieldset.appendChild(label);
    });
    root.appendChild(fieldset);
  }

  private renderOverview(project: EditableWorldProject): void {
    const root = this.element('world-overview');
    root.textContent =
      `${project.levels.length} levels · ${project.document.connectors?.length ?? 0} inter-level connections`;
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
      const p = document.createElement('p');
      p.className = 'editor-problem-ok';
      p.textContent = 'No validation problems.';
      root.appendChild(p);
      return;
    }
    for (const problem of problems) {
      const p = document.createElement('p');
      p.className = 'editor-problem';
      p.dataset.severity = problem.severity;
      p.textContent = `${problem.category}: ${problem.message}`;
      root.appendChild(p);
    }
  }
}
