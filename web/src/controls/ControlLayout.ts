import { CONTROL_LAYOUT } from '../config';
import type { ControlElements } from '../dom/elements';

export class ControlLayout {
  constructor(
    private readonly viewport: HTMLElement,
    private readonly controls: ControlElements
  ) {}

  position(renderWidth: number, renderHeight: number): void {
    const c = CONTROL_LAYOUT;

    // Zoom stays on the top-left. Its controls form one horizontal row: - • +.
    const zoomOutX = c.topLeft;
    const zoomResetX = zoomOutX + c.zoomControlSize + c.topControlGap;
    const zoomInX = zoomResetX + c.resetDiskSize + c.topControlGap;
    const zoomResetTop = c.topControlTop;
    const zoomControlTop = zoomResetTop + (c.resetDiskSize - c.zoomControlSize) * 0.5;
    this.setHitbox(this.controls.zoomOut, zoomOutX, zoomControlTop, c.zoomControlSize, c.zoomControlSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.resetZoom, zoomResetX, zoomResetTop, c.resetDiskSize, c.resetDiskSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.zoomIn, zoomInX, zoomControlTop, c.zoomControlSize, c.zoomControlSize, 44, renderWidth, renderHeight);

    // Z-level stays on the top-right and remains vertical.
    const levelX = renderWidth - c.topRight - c.panArrowSize;
    const levelUpTop = c.topControlTop;
    const levelResetTop = levelUpTop + c.panArrowSize + c.topControlGap;
    const levelDownTop = levelResetTop + c.resetDiskSize + c.topControlGap;
    this.setHitbox(this.controls.levelUp, levelX, levelUpTop, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.resetLevel, levelX, levelResetTop, c.resetDiskSize, c.resetDiskSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.levelDown, levelX, levelDownTop, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);

    const yawRowTop = renderHeight - c.controlBottom - c.rotateArrowHeight;
    const counterClockwiseX = c.rotateLeftX;
    const resetYawX = counterClockwiseX + c.rotateArrowWidth + c.rotateRowGap;
    const clockwiseX = resetYawX + c.resetDiskSize + c.rotateRowGap;
    const resetYawTop = yawRowTop + (c.rotateArrowHeight - c.resetDiskSize) * 0.5;
    this.setHitbox(this.controls.counterClockwise, counterClockwiseX, yawRowTop, c.rotateArrowWidth, c.rotateArrowHeight, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.resetYaw, resetYawX, resetYawTop, c.resetDiskSize, c.resetDiskSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.clockwise, clockwiseX, yawRowTop, c.rotateArrowWidth, c.rotateArrowHeight, 44, renderWidth, renderHeight);

    const centreX = renderWidth - c.panPadRight - c.panArrowSize - c.panXStep;
    const centreY = renderHeight - c.panPadBottom - c.panArrowSize - c.panYStep;
    this.setHitbox(this.controls.panLeft, centreX - c.panXStep, centreY, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.resetCamera, centreX, centreY, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.panRight, centreX + c.panXStep, centreY, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.panUp, centreX, centreY - c.panYStep, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.panDown, centreX, centreY + c.panYStep, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);
  }

  private setHitbox(
    control: HTMLButtonElement,
    renderX: number,
    renderY: number,
    renderWidth: number,
    renderHeight: number,
    minimumCssSize: number,
    framebufferWidth: number,
    framebufferHeight: number
  ): void {
    const rect = this.viewport.getBoundingClientRect();
    const scaleX = rect.width / framebufferWidth;
    const scaleY = rect.height / framebufferHeight;
    let left = renderX * scaleX;
    let top = renderY * scaleY;
    let width = renderWidth * scaleX;
    let height = renderHeight * scaleY;

    if (width < minimumCssSize) {
      left -= (minimumCssSize - width) * 0.5;
      width = minimumCssSize;
    }
    if (height < minimumCssSize) {
      top -= (minimumCssSize - height) * 0.5;
      height = minimumCssSize;
    }

    control.style.left = `${left}px`;
    control.style.top = `${top}px`;
    control.style.width = `${width}px`;
    control.style.height = `${height}px`;
  }
}
