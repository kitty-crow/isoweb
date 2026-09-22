import type {
  ConnectorDefinition, EntityDefinition, GroundRectangle, LevelDocument,
  PointLightDefinition, PrimitiveGeometryDefinition, RoomDefinition, SpawnDefinition,
  Vec3Tuple
} from '../world/documents';
import {
  createLocalAddOperation, selectedLevel, type LocalAddKind
} from './AuthoringCommands';
import { FunctionalCommand } from './CommandHistory';
import type { EditorCore } from './EditorCore';
import type { EditorSelection } from './Selection';

type Positioned = {
  position: Vec3Tuple;
  width: number;
  height: number;
  rotation?: number;
};

const MIN_SIZE = 0.25;

export class EditorLayoutViewport {
  private readonly scale = 32;
  private dragState:
    | {
        kind: 'move';
        selection: EditorSelection;
        startX: number;
        startY: number;
        before: Vec3Tuple;
        ref: Vec3Tuple;
        element: HTMLElement;
      }
    | {
        kind: 'resize';
        selection: EditorSelection;
        startX: number;
        startY: number;
        before: [number, number];
        applyLive: (width: number, height: number) => void;
        restore: () => void;
        element: HTMLElement;
      }
    | {
        kind: 'rotate';
        selection: EditorSelection;
        centreX: number;
        centreY: number;
        before: Vec3Tuple;
        ref: Vec3Tuple;
        element: HTMLElement;
      }
    | null = null;

  constructor(
    private readonly core: EditorCore,
    private readonly root: HTMLElement
  ) {
    this.bindPalette();
    this.root.addEventListener('dragover', event => {
      if (event.dataTransfer?.types.includes('application/x-isoweb-add-kind')) {
        event.preventDefault();
        event.dataTransfer.dropEffect = 'copy';
      }
    });
    this.root.addEventListener('drop', event => this.dropPaletteItem(event));
    this.root.addEventListener('pointermove', event => this.pointerMove(event));
    this.root.addEventListener('pointerup', event => this.pointerUp(event));
    this.root.addEventListener('pointercancel', event => this.pointerUp(event));
  }

  render(): void {
    const project = this.core.store.project;
    if (!project) return;
    const level = selectedLevel(project, this.core.selection.value);

    this.root.replaceChildren();
    const grid = document.createElement('div');
    grid.className = 'editor-layout-grid';
    this.root.appendChild(grid);

    const origin = document.createElement('div');
    origin.className = 'editor-layout-origin';
    origin.setAttribute('aria-hidden', 'true');
    this.root.appendChild(origin);

    for (const ground of level.ground) {
      this.addItem(level, { kind: 'ground', id: ground.id, levelId: level.id }, ground);
    }
    for (const room of level.rooms ?? []) {
      this.addItem(level, { kind: 'room', id: room.id, levelId: level.id }, room);
    }
    for (const geometry of level.geometry) {
      this.addItem(level, { kind: 'geometry', id: geometry.id, levelId: level.id }, geometry);
    }
    for (const entity of level.entities) {
      this.addItem(level, { kind: 'entity', id: entity.id, levelId: level.id }, entity);
    }
    for (const spawn of level.spawns) {
      this.addItem(level, { kind: 'spawn', id: spawn.id, levelId: level.id }, spawn);
    }
    for (const connector of level.connectors) {
      this.addItem(level, { kind: 'connector', id: connector.id, levelId: level.id }, connector);
    }
    for (const light of level.lights) {
      this.addItem(level, { kind: 'light', id: light.id, levelId: level.id }, light);
    }

    if (level.ground.length + (level.rooms?.length ?? 0) + level.geometry.length +
        level.entities.length + level.spawns.length + level.connectors.length + level.lights.length === 0) {
      const empty = document.createElement('p');
      empty.className = 'editor-layout-empty';
      empty.textContent = 'Drag an item from the palette and drop it here.';
      this.root.appendChild(empty);
    }
  }

  private bindPalette(): void {
    for (const element of document.querySelectorAll<HTMLElement>('[data-editor-add-kind]')) {
      const kind = element.dataset.editorAddKind as LocalAddKind | undefined;
      if (!kind) continue;
      element.draggable = true;
      element.addEventListener('dragstart', event => {
        event.dataTransfer?.setData('application/x-isoweb-add-kind', kind);
        if (event.dataTransfer) event.dataTransfer.effectAllowed = 'copy';
      });
      element.addEventListener('click', () => this.addAt(kind, { x: 0, y: 0 }));
    }
  }

  private dropPaletteItem(event: DragEvent): void {
    const kind = event.dataTransfer?.getData('application/x-isoweb-add-kind') as LocalAddKind;
    if (!kind) return;
    event.preventDefault();
    const position = this.clientToWorld(event.clientX, event.clientY);
    this.addAt(kind, position);
  }

  private addAt(kind: LocalAddKind, position: { x: number; y: number }): void {
    const project = this.core.store.project;
    if (!project) return;
    const snapped = {
      x: this.snap(position.x),
      y: this.snap(position.y)
    };
    const operation = createLocalAddOperation(
      project,
      this.core.selection.value,
      kind,
      snapped
    );
    this.core.execute(operation.command);
    this.core.selection.select(operation.selection);
  }

  private addItem(level: LevelDocument, selection: EditorSelection, target: unknown): void {
    const layout = this.layoutFor(selection, target);
    if (!layout) return;

    const item = document.createElement('button');
    item.type = 'button';
    item.className = 'editor-layout-item';
    item.dataset.kind = selection.kind;
    item.dataset.id = selection.id;
    item.setAttribute('aria-label', `${selection.kind} ${selection.id}`);
    item.title = `${selection.kind}: ${selection.id}`;
    this.positionElement(item, layout);

    const selected = this.sameSelection(this.core.selection.value, selection);
    if (selected) item.dataset.selected = 'true';

    const label = document.createElement('span');
    label.className = 'editor-layout-label';
    label.textContent = selection.id;
    item.appendChild(label);

    item.addEventListener('click', event => {
      event.stopPropagation();
      this.core.selection.select(selection);
    });
    item.addEventListener('pointerdown', event => {
      if ((event.target as HTMLElement).classList.contains('editor-layout-handle')) return;
      event.stopPropagation();
      this.beginMove(event, selection, target, item);
    });

    if (selected) {
      const resize = this.resizeBinding(selection, target);
      if (resize) {
        const handle = document.createElement('span');
        handle.className = 'editor-layout-handle editor-layout-resize';
        handle.setAttribute('aria-hidden', 'true');
        handle.addEventListener('pointerdown', event => {
          event.preventDefault();
          event.stopPropagation();
          this.beginResize(event, selection, resize, item);
        });
        item.appendChild(handle);
      }

      const rotate = this.rotationBinding(selection, target);
      if (rotate) {
        const handle = document.createElement('span');
        handle.className = 'editor-layout-handle editor-layout-rotate';
        handle.setAttribute('aria-hidden', 'true');
        handle.addEventListener('pointerdown', event => {
          event.preventDefault();
          event.stopPropagation();
          this.beginRotate(event, selection, rotate, item);
        });
        item.appendChild(handle);
      }
    }

    this.root.appendChild(item);
  }

  private layoutFor(selection: EditorSelection, target: unknown): Positioned | null {
    if (selection.kind === 'ground') {
      const ground = target as GroundRectangle;
      return {
        position: ground.centre,
        width: ground.size[0],
        height: ground.size[1]
      };
    }
    if (selection.kind === 'room') {
      const room = target as RoomDefinition;
      return {
        position: room.centre,
        width: room.width,
        height: room.depth
      };
    }
    if (selection.kind === 'geometry') {
      const geometry = target as PrimitiveGeometryDefinition;
      return {
        position: geometry.position,
        width: geometry.size,
        height: geometry.size
      };
    }
    if (selection.kind === 'entity') {
      const entity = target as EntityDefinition;
      const forward = entity.components.transform.forward;
      return {
        position: entity.components.transform.position,
        width: 0.8,
        height: 0.8,
        rotation: forward ? Math.atan2(forward[0], forward[1]) : 0
      };
    }
    if (selection.kind === 'spawn') {
      const spawn = target as SpawnDefinition;
      const forward = spawn.transform.forward;
      return {
        position: spawn.transform.position,
        width: 0.65,
        height: 0.65,
        rotation: forward ? Math.atan2(forward[0], forward[1]) : 0
      };
    }
    if (selection.kind === 'light') {
      const light = target as PointLightDefinition;
      return { position: light.position, width: 0.6, height: 0.6 };
    }
    if (selection.kind === 'connector') {
      const connector = target as ConnectorDefinition;
      const dx = connector.toPosition[0] - connector.fromPosition[0];
      const dy = connector.toPosition[1] - connector.fromPosition[1];
      const length = Math.max(0.25, Math.hypot(dx, dy));
      return {
        position: [
          (connector.fromPosition[0] + connector.toPosition[0]) / 2,
          (connector.fromPosition[1] + connector.toPosition[1]) / 2,
          (connector.fromPosition[2] + connector.toPosition[2]) / 2
        ],
        width: length,
        height: 0.2,
        rotation: Math.atan2(dy, dx)
      };
    }
    return null;
  }

  private positionElement(element: HTMLElement, layout: Positioned): void {
    const centreX = this.root.clientWidth / 2 + layout.position[0] * this.scale;
    const centreY = this.root.clientHeight / 2 - layout.position[1] * this.scale;
    element.style.left = `${centreX}px`;
    element.style.top = `${centreY}px`;
    element.style.width = `${Math.max(16, layout.width * this.scale)}px`;
    element.style.height = `${Math.max(16, layout.height * this.scale)}px`;
    element.style.transform = `translate(-50%, -50%) rotate(${layout.rotation ?? 0}rad)`;
  }

  private beginMove(
    event: PointerEvent,
    selection: EditorSelection,
    target: unknown,
    element: HTMLElement
  ): void {
    const ref = this.positionBinding(selection, target);
    if (!ref) return;
    element.setPointerCapture(event.pointerId);
    this.dragState = {
      kind: 'move',
      selection,
      startX: event.clientX,
      startY: event.clientY,
      before: [...ref] as Vec3Tuple,
      ref,
      element
    };
  }

  private beginResize(
    event: PointerEvent,
    selection: EditorSelection,
    binding: {
      read: () => [number, number];
      write: (width: number, height: number) => void;
    },
    element: HTMLElement
  ): void {
    element.setPointerCapture(event.pointerId);
    const before = binding.read();
    this.dragState = {
      kind: 'resize',
      selection,
      startX: event.clientX,
      startY: event.clientY,
      before,
      applyLive: binding.write,
      restore: () => binding.write(before[0], before[1]),
      element
    };
  }

  private beginRotate(
    event: PointerEvent,
    selection: EditorSelection,
    ref: Vec3Tuple,
    element: HTMLElement
  ): void {
    const rect = element.getBoundingClientRect();
    element.setPointerCapture(event.pointerId);
    this.dragState = {
      kind: 'rotate',
      selection,
      centreX: rect.left + rect.width / 2,
      centreY: rect.top + rect.height / 2,
      before: [...ref] as Vec3Tuple,
      ref,
      element
    };
  }

  private pointerMove(event: PointerEvent): void {
    const state = this.dragState;
    if (!state) return;

    if (state.kind === 'move') {
      const dx = (event.clientX - state.startX) / this.scale;
      const dy = -(event.clientY - state.startY) / this.scale;
      state.ref[0] = this.snap(state.before[0] + dx);
      state.ref[1] = this.snap(state.before[1] + dy);
      this.repositionFromSelection(state.selection, state.element);
      return;
    }

    if (state.kind === 'resize') {
      const dx = (event.clientX - state.startX) * 2 / this.scale;
      const dy = (event.clientY - state.startY) * 2 / this.scale;
      const width = Math.max(MIN_SIZE, this.snapSize(state.before[0] + dx));
      const height = Math.max(MIN_SIZE, this.snapSize(state.before[1] + dy));
      state.applyLive(width, height);
      this.repositionFromSelection(state.selection, state.element);
      return;
    }

    const angle = Math.atan2(
      event.clientX - state.centreX,
      state.centreY - event.clientY
    );
    state.ref[0] = Math.sin(angle);
    state.ref[1] = Math.cos(angle);
    state.ref[2] = 0;
    this.repositionFromSelection(state.selection, state.element);
  }

  private pointerUp(event: PointerEvent): void {
    const state = this.dragState;
    if (!state) return;
    this.dragState = null;
    if (state.element.hasPointerCapture(event.pointerId)) {
      state.element.releasePointerCapture(event.pointerId);
    }

    if (state.kind === 'move') {
      const after = [...state.ref] as Vec3Tuple;
      if (after[0] === state.before[0] && after[1] === state.before[1]) return;
      state.ref.splice(0, 3, ...state.before);
      this.core.execute(new FunctionalCommand(
        `move ${state.selection.kind}`,
        () => state.ref.splice(0, 3, ...after),
        () => state.ref.splice(0, 3, ...state.before)
      ));
      this.core.selection.select(state.selection);
      return;
    }

    if (state.kind === 'resize') {
      const project = this.core.store.project;
      if (!project) return;
      const target = this.resolveTarget(selectedLevel(project, state.selection), state.selection);
      const binding = this.resizeBinding(state.selection, target);
      if (!binding) return;
      const after = binding.read();
      if (after[0] === state.before[0] && after[1] === state.before[1]) return;
      state.restore();
      this.core.execute(new FunctionalCommand(
        `resize ${state.selection.kind}`,
        () => binding.write(after[0], after[1]),
        () => binding.write(state.before[0], state.before[1])
      ));
      this.core.selection.select(state.selection);
      return;
    }

    const after = [...state.ref] as Vec3Tuple;
    if (after[0] === state.before[0] && after[1] === state.before[1]) return;
    state.ref.splice(0, 3, ...state.before);
    this.core.execute(new FunctionalCommand(
      `rotate ${state.selection.kind}`,
      () => state.ref.splice(0, 3, ...after),
      () => state.ref.splice(0, 3, ...state.before)
    ));
    this.core.selection.select(state.selection);
  }

  private positionBinding(selection: EditorSelection, target: unknown): Vec3Tuple | null {
    if (selection.kind === 'ground') return (target as GroundRectangle).centre;
    if (selection.kind === 'room') return (target as RoomDefinition).centre;
    if (selection.kind === 'geometry') return (target as PrimitiveGeometryDefinition).position;
    if (selection.kind === 'entity') return (target as EntityDefinition).components.transform.position;
    if (selection.kind === 'spawn') return (target as SpawnDefinition).transform.position;
    if (selection.kind === 'light') return (target as PointLightDefinition).position;
    return null;
  }

  private rotationBinding(selection: EditorSelection, target: unknown): Vec3Tuple | null {
    if (selection.kind === 'entity') {
      const transform = (target as EntityDefinition).components.transform;
      transform.forward ??= [0, 1, 0];
      return transform.forward;
    }
    if (selection.kind === 'spawn') {
      const transform = (target as SpawnDefinition).transform;
      transform.forward ??= [0, 1, 0];
      return transform.forward;
    }
    return null;
  }

  private resizeBinding(
    selection: EditorSelection,
    target: unknown
  ): {
    read: () => [number, number];
    write: (width: number, height: number) => void;
  } | null {
    if (selection.kind === 'ground') {
      const ground = target as GroundRectangle;
      return {
        read: () => [ground.size[0], ground.size[1]],
        write: (width, height) => {
          ground.size[0] = width;
          ground.size[1] = height;
        }
      };
    }
    if (selection.kind === 'room') {
      const room = target as RoomDefinition;
      return {
        read: () => [room.width, room.depth],
        write: (width, height) => {
          room.width = width;
          room.depth = height;
        }
      };
    }
    if (selection.kind === 'geometry') {
      const geometry = target as PrimitiveGeometryDefinition;
      return {
        read: () => [geometry.size, geometry.size],
        write: (width, height) => {
          geometry.size = Math.max(MIN_SIZE, Math.max(width, height));
        }
      };
    }
    return null;
  }

  private repositionFromSelection(selection: EditorSelection, element: HTMLElement): void {
    const project = this.core.store.project;
    if (!project) return;
    const level = selectedLevel(project, selection);
    const target = this.resolveTarget(level, selection);
    const layout = target ? this.layoutFor(selection, target) : null;
    if (layout) this.positionElement(element, layout);
  }

  private resolveTarget(level: LevelDocument, selection: EditorSelection): unknown {
    switch (selection.kind) {
      case 'ground': return level.ground.find(value => value.id === selection.id);
      case 'room': return level.rooms?.find(value => value.id === selection.id);
      case 'geometry': return level.geometry.find(value => value.id === selection.id);
      case 'entity': return level.entities.find(value => value.id === selection.id);
      case 'spawn': return level.spawns.find(value => value.id === selection.id);
      case 'connector': return level.connectors.find(value => value.id === selection.id);
      case 'light': return level.lights.find(value => value.id === selection.id);
      default: return undefined;
    }
  }

  private clientToWorld(clientX: number, clientY: number): { x: number; y: number } {
    const rect = this.root.getBoundingClientRect();
    return {
      x: (clientX - rect.left - rect.width / 2) / this.scale,
      y: -(clientY - rect.top - rect.height / 2) / this.scale
    };
  }

  private snap(value: number): number {
    const step = Number(
      document.querySelector<HTMLSelectElement>('#editor-grid-snap')?.value ?? '0.5'
    );
    if (!Number.isFinite(step) || step <= 0) return value;
    return Math.round(value / step) * step;
  }

  private snapSize(value: number): number {
    return Math.max(MIN_SIZE, Math.abs(this.snap(value)));
  }

  private sameSelection(a: EditorSelection | null, b: EditorSelection): boolean {
    return !!a && a.kind === b.kind && a.id === b.id && a.levelId === b.levelId;
  }
}
