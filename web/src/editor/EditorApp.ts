import { FunctionalCommand } from './CommandHistory';
import {
  createDeleteCommand, createDuplicateOperation, createLocalAddOperation,
  selectedLevel, type LocalAddKind
} from './AuthoringCommands';
import { EditorCore } from './EditorCore';
import { EditorLayoutViewport } from './EditorLayoutViewport';
import { EditorPlayPreview } from './EditorPlayPreview';
import {
  createDeleteRoomConnectionCommand,
  createRoomConnectionOperation
} from './RoomConnectionManager';
import type { EditorSelection, EditorSelectionKind } from './Selection';
import type { EditableSourceProject } from './SourceProjectIO';
import type { LevelDocument, Vec3Tuple } from '../world/documents';

type IndexedRecord = Record<string, unknown>;

export class EditorApp {
  private transientProblem = '';
  private layoutViewport: EditorLayoutViewport | null = null;
  private playPreview: EditorPlayPreview | null = null;

  constructor(
    private readonly core: EditorCore,
    private readonly productName: string
  ) {}

  start(): void {
    this.bindToolbar();
    this.bindRoomConnectionControls();
    this.bindHierarchyControls();
    this.layoutViewport = new EditorLayoutViewport(
      this.core,
      this.element<HTMLElement>('editor-layout')
    );
    this.playPreview = new EditorPlayPreview(
      this.core,
      this.element<HTMLButtonElement>('editor-play'),
      this.element<HTMLElement>('editor-layout-shell'),
      this.element<HTMLIFrameElement>('editor-runtime-frame')
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
      this.playPreview?.exit();
      this.transientProblem = '';
      this.core.newProject();
    });

    const input = this.element<HTMLInputElement>('editor-open-file');
    this.element<HTMLButtonElement>('editor-open').addEventListener('click', () => {
      if (!this.discardAllowed()) return;
      this.playPreview?.exit();
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

    this.element<HTMLButtonElement>('editor-play').addEventListener('click', () => {
      try {
        if (this.playPreview?.isPlaying) this.playPreview.exit();
        else this.playPreview?.enter();
        this.transientProblem = '';
      } catch (error) {
        this.transientProblem = error instanceof Error ? error.message : String(error);
        this.renderProblems();
      }
    });

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

  private bindRoomConnectionControls(): void {
    this.element<HTMLButtonElement>('editor-room-connect').addEventListener('click', () => {
      const project = this.core.store.project;
      const selection = this.core.selection.value;
      if (!project || selection?.kind !== 'room' || !selection.levelId) return;
      const target = this.element<HTMLSelectElement>('editor-room-target').value;
      if (!target) return;
      try {
        const operation = createRoomConnectionOperation(
          project,
          selection,
          selection.id,
          target
        );
        this.core.execute(operation.command);
        this.core.selection.select(operation.selection);
        this.transientProblem = '';
      } catch (error) {
        this.transientProblem = error instanceof Error ? error.message : String(error);
        this.renderProblems();
      }
    });

    this.element<HTMLButtonElement>('editor-room-disconnect').addEventListener('click', () => {
      const project = this.core.store.project;
      const selection = this.core.selection.value;
      if (!project || selection?.kind !== 'room-connection') return;
      try {
        const level = selectedLevel(project, selection);
        const connection = level.roomConnections?.find(candidate => candidate.id === selection.id);
        this.core.execute(createDeleteRoomConnectionCommand(project, selection));
        const fallback = connection?.a.roomId ?? level.id;
        this.core.selection.select(
          fallback === level.id
            ? { kind: 'level', id: level.id, levelId: level.id }
            : { kind: 'room', id: fallback, levelId: level.id }
        );
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
      'ground', 'entity', 'geometry', 'room', 'floor-hole', 'staircase', 'spawn', 'connector', 'light'
    ]);
    const canOperate = !!selection && localEditable.has(selection.kind);
    this.element<HTMLButtonElement>('editor-duplicate').disabled = !canOperate;
    this.element<HTMLButtonElement>('editor-delete').disabled = !canOperate;
    this.renderRoomConnectionControls(project);

    this.renderHierarchy();
    this.renderInspector();
    this.renderOverview();
    this.layoutViewport?.render();
    this.renderProblems();
  }

  private renderRoomConnectionControls(project: EditableSourceProject): void {
    const selection = this.core.selection.value;
    const target = this.element<HTMLSelectElement>('editor-room-target');
    const connect = this.element<HTMLButtonElement>('editor-room-connect');
    const disconnect = this.element<HTMLButtonElement>('editor-room-disconnect');

    target.replaceChildren();
    target.disabled = true;
    connect.disabled = true;
    disconnect.disabled = selection?.kind !== 'room-connection';

    if (selection?.kind !== 'room' || !selection.levelId) return;
    const level = selectedLevel(project, selection);
    for (const room of level.rooms ?? []) {
      if (room.id === selection.id) continue;
      const option = document.createElement('option');
      option.value = room.id;
      option.textContent = room.id;
      target.appendChild(option);
    }
    target.disabled = target.options.length === 0;
    connect.disabled = target.disabled;
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
      for (const connection of level.roomConnections ?? []) {
        add(
          `Opening · ${connection.a.roomId} ↔ ${connection.b.roomId}`,
          'room-connection',
          connection.id,
          level.id,
          2
        );
      }
      for (const hole of level.floorHoles ?? []) add(hole.id, 'floor-hole', hole.id, level.id, 2);
      for (const stair of level.staircases ?? []) add(stair.id, 'staircase', stair.id, level.id, 2);
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
      const position = record.position as Vec3Tuple;
      const type = String(record.type);
      const radial = type === 'sphere' || type === 'dodecahedron' || type === 'icosahedron';
      const polyScale = type === 'dodecahedron' || type === 'icosahedron' ? 1.55 : 1;
      const halfHeight = (): number => radial
        ? Number(record.size) * polyScale
        : Math.max(0.25, Number(record.height ?? 1)) / 2;
      const bottomZ = (): number => position[2] - halfHeight();

      this.addNumberProperty(
        root,
        radial ? 'Radius / scale' : 'Half-width',
        Number(record.size),
        value => {
          const beforeSize = Number(record.size);
          const beforeZ = position[2];
          const floor = bottomZ();
          const nextSize = Math.max(0.25, value);
          const nextHalfHeight = radial ? nextSize * polyScale : halfHeight();
          this.core.execute(new FunctionalCommand(
            'resize geometry',
            () => {
              record.size = nextSize;
              if (radial) position[2] = floor + nextHalfHeight;
            },
            () => {
              record.size = beforeSize;
              position[2] = beforeZ;
            }
          ));
        }
      );

      if (!radial) {
        this.addNumberProperty(root, 'Height', Number(record.height ?? 1), value => {
          const beforeHeight = Number(record.height ?? 1);
          const beforeZ = position[2];
          const floor = bottomZ();
          const nextHeight = Math.max(0.25, value);
          this.core.execute(new FunctionalCommand(
            'set geometry height',
            () => {
              record.height = nextHeight;
              position[2] = floor + nextHeight / 2;
            },
            () => {
              record.height = beforeHeight;
              position[2] = beforeZ;
            }
          ));
        });
      }

      this.addNumberProperty(root, 'Bottom Z', bottomZ(), value => {
        const before = position[2];
        const next = value + halfHeight();
        this.core.execute(new FunctionalCommand(
          'set geometry bottom Z',
          () => { position[2] = next; },
          () => { position[2] = before; }
        ));
      });
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

    if (selection.kind === 'floor-hole') {
      const minimum = record.minimum as [number, number];
      const maximum = record.maximum as [number, number];
      if (Array.isArray(minimum) && Array.isArray(maximum)) {
        for (const [axis, label] of [[0, 'Width'], [1, 'Depth']] as const) {
          this.addNumberProperty(root, label, maximum[axis] - minimum[axis], value => {
            const beforeMin = minimum[axis];
            const beforeMax = maximum[axis];
            const centre = (beforeMin + beforeMax) / 2;
            const half = Math.max(0.125, value / 2);
            this.core.execute(new FunctionalCommand(
              `resize floor hole ${label.toLowerCase()}`,
              () => {
                minimum[axis] = centre - half;
                maximum[axis] = centre + half;
              },
              () => {
                minimum[axis] = beforeMin;
                maximum[axis] = beforeMax;
              }
            ));
          });
        }
      }
    }

    if (selection.kind === 'staircase') {
      for (const [key, label, minimum] of [
        ['centreX', 'Centre X', -Infinity],
        ['startY', 'Start Y', -Infinity],
        ['endY', 'End Y', -Infinity],
        ['startZ', 'Start Z', -Infinity],
        ['endZ', 'End Z', -Infinity],
        ['width', 'Width', 0.25]
      ] as const) {
        this.addNumberProperty(root, label, Number(record[key]), value => {
          const before = Number(record[key]);
          this.core.execute(new FunctionalCommand(
            `set staircase ${label.toLowerCase()}`,
            () => { record[key] = Math.max(minimum, value); },
            () => { record[key] = before; }
          ));
        });
      }
    }

    if (selection.kind === 'room-connection') {
      for (const endpointKey of ['a', 'b'] as const) {
        const endpoint = record[endpointKey] as IndexedRecord | undefined;
        if (!endpoint) continue;
        this.addSelectProperty(
          root,
          endpointKey === 'a' ? 'Room A wall' : 'Room B wall',
          String(endpoint.side),
          [
            ['north', 'North'],
            ['south', 'South'],
            ['east', 'East'],
            ['west', 'West']
          ],
          value => {
            const before = endpoint.side;
            this.core.execute(new FunctionalCommand(
              'change room opening wall',
              () => { endpoint.side = value; },
              () => { endpoint.side = before; }
            ));
          }
        );
        this.addNumberProperty(root, endpointKey === 'a' ? 'Room A offset' : 'Room B offset', Number(endpoint.offset ?? 0), value => {
          const before = endpoint.offset;
          this.core.execute(new FunctionalCommand(
            'move room opening',
            () => { endpoint.offset = value; },
            () => { endpoint.offset = before; }
          ));
        });
        this.addNumberProperty(root, endpointKey === 'a' ? 'Room A width' : 'Room B width', Number(endpoint.width), value => {
          const before = endpoint.width;
          this.core.execute(new FunctionalCommand(
            'resize room opening',
            () => { endpoint.width = Math.max(0.25, value); },
            () => { endpoint.width = before; }
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
      const collider = components?.collider as IndexedRecord | undefined;
      if (collider && Array.isArray(collider.minimum) && Array.isArray(collider.maximum)) {
        const minimum = collider.minimum as Vec3Tuple;
        const maximum = collider.maximum as Vec3Tuple;
        const resizeCollider = (axis: 0 | 1 | 2, next: number, label: string): void => {
          const beforeMin = minimum[axis];
          const beforeMax = maximum[axis];
          const size = Math.max(0.25, next);
          const centre = (beforeMin + beforeMax) / 2;
          this.core.execute(new FunctionalCommand(
            `resize collider ${label}`,
            () => {
              if (axis === 2) {
                // Height changes keep the authored bottom face fixed.
                minimum[axis] = beforeMin;
                maximum[axis] = beforeMin + size;
              } else {
                const half = size / 2;
                minimum[axis] = centre - half;
                maximum[axis] = centre + half;
              }
            },
            () => {
              minimum[axis] = beforeMin;
              maximum[axis] = beforeMax;
            }
          ));
        };
        this.addNumberProperty(root, 'Collider width', maximum[0] - minimum[0], value => {
          resizeCollider(0, Math.max(0.25, value), 'width');
        });
        this.addNumberProperty(root, 'Collider depth', maximum[1] - minimum[1], value => {
          resizeCollider(1, Math.max(0.25, value), 'depth');
        });
        this.addNumberProperty(root, 'Collider height', maximum[2] - minimum[2], value => {
          resizeCollider(2, Math.max(0.25, value), 'height');
        });
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

  private addSelectProperty(
    root: HTMLElement,
    labelText: string,
    value: string,
    options: readonly (readonly [string, string])[],
    commit: (value: string) => void
  ): void {
    const label = document.createElement('label');
    label.className = 'editor-field';
    const title = document.createElement('span');
    title.textContent = labelText;
    const select = document.createElement('select');
    for (const [optionValue, optionLabel] of options) {
      const option = document.createElement('option');
      option.value = optionValue;
      option.textContent = optionLabel;
      select.appendChild(option);
    }
    select.value = value;
    select.addEventListener('change', () => {
      if (select.value === value) return;
      commit(select.value);
    });
    label.append(title, select);
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
      ['Floor holes', levels.reduce((sum, level) => sum + (level.floorHoles?.length ?? 0), 0)],
      ['Stairs', levels.reduce((sum, level) => sum + (level.staircases?.length ?? 0), 0)],
      ['Room openings', levels.reduce((sum, level) => sum + (level.roomConnections?.length ?? 0), 0)],
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
      case 'floor-hole': return level.floorHoles?.find(value => value.id === selection.id);
      case 'staircase': return level.staircases?.find(value => value.id === selection.id);
      case 'room-connection': return level.roomConnections?.find(value => value.id === selection.id);
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
