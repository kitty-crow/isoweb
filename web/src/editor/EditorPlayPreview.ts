import type { EditorCore } from './EditorCore';

export class EditorPlayPreview {
  private objectUrl: string | null = null;
  private playing = false;

  constructor(
    private readonly core: EditorCore,
    private readonly button: HTMLButtonElement,
    private readonly layout: HTMLElement,
    private readonly frame: HTMLIFrameElement
  ) {
    this.button.addEventListener('click', () => {
      if (this.playing) this.exit();
      else this.enter();
    });
  }

  get isPlaying(): boolean { return this.playing; }

  enter(): void {
    if (this.playing) return;
    const problems = this.core.problems().filter(problem => problem.severity === 'error');
    if (problems.length > 0) {
      throw new Error(`Cannot enter Play while the project has validation errors: ${problems[0].message}`);
    }

    const bytes = this.core.previewWorldBytes();
    this.objectUrl = URL.createObjectURL(
      new Blob([bytes], { type: 'application/octet-stream' })
    );

    const runtimeUrl = new URL('./', document.baseURI);
    runtimeUrl.searchParams.set('world', this.objectUrl);

    this.playing = true;
    this.button.textContent = 'Edit';
    this.button.setAttribute('aria-pressed', 'true');
    this.layout.hidden = true;
    this.frame.hidden = false;
    this.frame.src = runtimeUrl.toString();
    document.body.dataset.editorMode = 'play';
  }

  exit(): void {
    if (!this.playing) return;
    this.playing = false;
    this.button.textContent = 'Play';
    this.button.setAttribute('aria-pressed', 'false');
    this.frame.src = 'about:blank';
    this.frame.hidden = true;
    this.layout.hidden = false;
    delete document.body.dataset.editorMode;
    if (this.objectUrl) URL.revokeObjectURL(this.objectUrl);
    this.objectUrl = null;
  }

  dispose(): void {
    this.exit();
  }
}
