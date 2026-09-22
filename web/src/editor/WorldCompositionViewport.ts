import type { LevelDocument, Vec3Tuple, WorldConnectorDefinition } from '../world/documents';
import { FunctionalCommand } from './CommandHistory';
import type { EditorCore } from './EditorCore';
import { createSetLevelPlacementCommand, effectiveLevelOrigin, levelPlacement } from './WorldPlacementManager';
import type { EditableWorldProject } from './SourceProjectIO';

type Bounds2 = { minX: number; minY: number; maxX: number; maxY: number };

type DragState = {
  levelId: string;
  startX: number;
  startY: number;
  before: Vec3Tuple;
  element: HTMLElement;
};

const MIN_CARD_METRES = 2;

function include(bounds: Bounds2, x: number, y: number): void {
  bounds.minX = Math.min(bounds.minX, x);
  bounds.minY = Math.min(bounds.minY, y);
  bounds.maxX = Math.max(bounds.maxX, x);
  bounds.maxY = Math.max(bounds.maxY, y);
}

function levelBounds(level: LevelDocument): Bounds2 {
  const bounds: Bounds2 = {
    minX: Number.POSITIVE_INFINITY,
    minY: Number.POSITIVE_INFINITY,
    maxX: Number.NEGATIVE_INFINITY,
    maxY: Number.NEGATIVE_INFINITY
  };

  for (const ground of level.ground) {
    const halfX = ground.size[0] / 2;
    const halfY = ground.size[1] / 2;
    include(bounds, ground.centre[0] - halfX, ground.centre[1] - halfY);
    include(bounds, ground.centre[0] + halfX, ground.centre[1] + halfY);
  }
  for (const room of level.rooms ?? []) {
    include(bounds, room.centre[0] - room.width / 2, room.centre[1] - room.depth / 2);
    include(bounds, room.centre[0] + room.width / 2, room.centre[1] + room.depth / 2);
  }
  for (const geometry of level.geometry) {
    include(bounds, geometry.position[0] - geometry.size, geometry.position[1] - geometry.size);
    include(bounds, geometry.position[0] + geometry.size, geometry.position[1] + geometry.size);
  }
  for (const entity of level.entities) {
    const p = entity.components.transform.position;
    include(bounds, p[0] - 0.5, p[1] - 0.5);
    include(bounds, p[0] + 0.5, p[1] + 0.5);
  }

  if (!Number.isFinite(bounds.minX)) {
    const [x, y] = level.settings.boundsFocus;
    return {
      minX: x - MIN_CARD_METRES / 2,
      minY: y - MIN_CARD_METRES / 2,
      maxX: x + MIN_CARD_METRES / 2,
      maxY: y + MIN_CARD_METRES / 2
    };
  }
  return bounds;
}

function add(a: Vec3Tuple, b: Vec3Tuple): Vec3Tuple {
  return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

export class WorldCompositionViewport {
  private readonly scale = 30;
  private drag: DragState | null = null;

  constructor(
    private readonly core: EditorCore,
    private readonly root: HTMLElement
  ) {
    this.root.addEventListener('pointermove', event => this.pointerMove(event));
    this.root.addEventListener('pointerup', event => this.pointerUp(event));
    this.root.addEventListener('pointercancel', event => this.pointerUp(event));
  }

  render(): void {
    const project = this.core.store.project;
    if (!project || project.kind !== 'world') return;

    this.root.replaceChildren();

    const grid = document.createElement('div');
    grid.className = 'editor-layout-grid';
    this.root.appendChild(grid);

    const origin = document.createElement('div');
    origin.className = 'editor-layout-origin';
    origin.setAttribute('aria-hidden', 'true');
    this.root.appendChild(origin);

    for (const connector of project.document.connectors ?? []) {
      this.renderConnector(project, connector);
    }

    const levels = [...project.levels].sort(
      (a, b) => effectiveLevelOrigin(project, a.id)[2] - effectiveLevelOrigin(project, b.id)[2]
    );
    for (const level of levels) this.renderLevel(project, level);
  }

  private renderConnector(
    project: EditableWorldProject,
    connector: WorldConnectorDefinition
  ): void {
    const fromOrigin = effectiveLevelOrigin(project, connector.fromLevel);
    const toOrigin = effectiveLevelOrigin(project, connector.toLevel);
    const from = add(fromOrigin, connector.fromPosition);
    const to = add(toOrigin, connector.toPosition);

    const x1 = this.root.clientWidth / 2 + from[0] * this.scale;
    const y1 = this.root.clientHeight / 2 - from[1] * this.scale;
    const x2 = this.root.clientWidth / 2 + to[0] * this.scale;
    const y2 = this.root.clientHeight / 2 - to[1] * this.scale;
    const length = Math.hypot(x2 - x1, y2 - y1);
    const angle = Math.atan2(y2 - y1, x2 - x1);

    const line = document.createElement('div');
    line.className = 'world-connector-line';
    line.dataset.type = connector.type;
    line.style.left = `${x1}px`;
    line.style.top = `${y1}px`;
    line.style.width = `${Math.max(2, length)}px`;
    line.style.transform = `rotate(${angle}rad)`;
    this.root.appendChild(line);

    const label = document.createElement('button');
    label.type = 'button';
    label.className = 'world-connector-label';
    label.dataset.type = connector.type;
    label.dataset.selected = this.core.selection.value?.kind === 'connector' &&
      this.core.selection.value.id === connector.id ? 'true' : 'false';
    label.textContent = connector.type === 'stairs' ? `Stairs · ${connector.id}` : `Portal · ${connector.id}`;
    label.style.left = `${(x1 + x2) / 2}px`;
    label.style.top = `${(y1 + y2) / 2}px`;
    label.addEventListener('click', () => {
      this.core.selection.select({ kind: 'connector', id: connector.id });
    });
    this.root.appendChild(label);
  }

  private renderLevel(project: EditableWorldProject, level: LevelDocument): void {
    const bounds = levelBounds(level);
    const origin = effectiveLevelOrigin(project, level.id);
    const centreX = origin[0] + (bounds.minX + bounds.maxX) / 2;
    const centreY = origin[1] + (bounds.minY + bounds.maxY) / 2;
    const width = Math.max(MIN_CARD_METRES, bounds.maxX - bounds.minX);
    const depth = Math.max(MIN_CARD_METRES, bounds.maxY - bounds.minY);

    const item = document.createElement('button');
    item.type = 'button';
    item.className = 'world-level-card';
    item.dataset.id = level.id;
    item.dataset.selected = this.core.selection.value?.kind === 'level' &&
      this.core.selection.value.id === level.id ? 'true' : 'false';
    item.dataset.default = project.document.settings.defaultLevel === level.id ? 'true' : 'false';
    item.style.left = `${this.root.clientWidth / 2 + centreX * this.scale}px`;
    item.style.top = `${this.root.clientHeight / 2 - centreY * this.scale}px`;
    item.style.width = `${Math.max(90, width * this.scale)}px`;
    item.style.height = `${Math.max(64, depth * this.scale)}px`;
    item.style.zIndex = String(20 + Math.max(-10, Math.min(10, Math.round(origin[2]))));

    const name = document.createElement('strong');
    name.textContent = level.name || level.id;
    const meta = document.createElement('span');
    meta.textContent = `Z ${origin[2].toFixed(2)} m${project.document.settings.defaultLevel === level.id ? ' · start' : ''}`;
    item.append(name, meta);

    item.addEventListener('click', event => {
      event.stopPropagation();
      this.core.selection.select({ kind: 'level', id: level.id, levelId: level.id });
    });
    item.addEventListener('pointerdown', event => {
      event.preventDefault();
      event.stopPropagation();
      item.setPointerCapture(event.pointerId);
      this.core.selection.select({ kind: 'level', id: level.id, levelId: level.id });
      this.drag = {
        levelId: level.id,
        startX: event.clientX,
        startY: event.clientY,
        before: levelPlacement(project, level.id),
        element: item
      };
    });

    this.root.appendChild(item);
  }

  private pointerMove(event: PointerEvent): void {
    const state = this.drag;
    if (!state) return;
    const dx = this.snap((event.clientX - state.startX) / this.scale);
    const dy = this.snap(-(event.clientY - state.startY) / this.scale);
    state.element.style.transform = `translate(-50%, -50%) translate(${dx * this.scale}px, ${-dy * this.scale}px)`;
  }

  private pointerUp(event: PointerEvent): void {
    const state = this.drag;
    if (!state) return;
    this.drag = null;
    if (state.element.hasPointerCapture(event.pointerId)) {
      state.element.releasePointerCapture(event.pointerId);
    }

    const dx = this.snap((event.clientX - state.startX) / this.scale);
    const dy = this.snap(-(event.clientY - state.startY) / this.scale);
    state.element.style.transform = 'translate(-50%, -50%)';
    if (dx === 0 && dy === 0) return;

    const project = this.core.store.project;
    if (!project || project.kind !== 'world') return;
    this.core.execute(createSetLevelPlacementCommand(
      project,
      state.levelId,
      [state.before[0] + dx, state.before[1] + dy, state.before[2]],
      'move level in world'
    ));
    this.core.selection.select({ kind: 'level', id: state.levelId, levelId: state.levelId });
  }

  private snap(value: number): number {
    const step = Number(
      document.querySelector<HTMLSelectElement>('#world-grid-snap')?.value ?? '0.5'
    );
    if (!Number.isFinite(step) || step <= 0) return value;
    return Math.round(value / step) * step;
  }
}
