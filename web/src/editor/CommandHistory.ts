export interface EditorCommand {
  readonly label: string;
  apply(): void;
  revert(): void;
}

export class FunctionalCommand implements EditorCommand {
  constructor(
    readonly label: string,
    private readonly applyFn: () => void,
    private readonly revertFn: () => void
  ) {}

  apply(): void { this.applyFn(); }
  revert(): void { this.revertFn(); }
}

export class CommandHistory {
  private readonly undoStack: EditorCommand[] = [];
  private readonly redoStack: EditorCommand[] = [];

  execute(command: EditorCommand): void {
    command.apply();
    this.undoStack.push(command);
    this.redoStack.length = 0;
  }

  undo(): EditorCommand | undefined {
    const command = this.undoStack.pop();
    if (!command) return undefined;
    command.revert();
    this.redoStack.push(command);
    return command;
  }

  redo(): EditorCommand | undefined {
    const command = this.redoStack.pop();
    if (!command) return undefined;
    command.apply();
    this.undoStack.push(command);
    return command;
  }

  clear(): void {
    this.undoStack.length = 0;
    this.redoStack.length = 0;
  }

  get canUndo(): boolean { return this.undoStack.length > 0; }
  get canRedo(): boolean { return this.redoStack.length > 0; }
  get undoLabel(): string | undefined { return this.undoStack.at(-1)?.label; }
  get redoLabel(): string | undefined { return this.redoStack.at(-1)?.label; }
}
