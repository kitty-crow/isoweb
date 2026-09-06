import { CONTROL_LAYOUT } from '../config';
import type { ControlElements } from '../dom/elements';

export class ControlLayout {
  constructor(
    private readonly viewport: HTMLElement,
    private readonly controls: ControlElements
  ) {}

  position(renderWidth: number, renderHeight: number): void {
    const c = CONTROL_LAYOUT;

    // Z-level controller is top-left.
    const levelX = c.topLeft;
    const levelUpTop = c.topControlTop;
    const levelResetTop = levelUpTop + c.panArrowSize + c.topControlGap;
    const levelDownTop = levelResetTop + c.resetDiskSize + c.topControlGap;
    this.setHitbox(this.controls.levelUp, levelX, levelUpTop, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.resetLevel, levelX, levelResetTop, c.resetDiskSize, c.resetDiskSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.levelDown, levelX, levelDownTop, c.panArrowSize, c.panArrowSize, 44, renderWidth, renderHeight);

    // Zoom controller is top-right.
    const zoomResetX = renderWidth - c.topRight - c.resetDiskSize;
    const zoomX = zoomResetX + (c.resetDiskSize - c.zoomControlSize) * 0.5;
    const zoomInTop = c.topControlTop;
    const zoomResetTop = zoomInTop + c.zoomControlSize + c.topControlGap;
    const zoomOutTop = zoomResetTop + c.resetDiskSize + c.topControlGap;
    this.setHitbox(this.controls.zoomIn, zoomX, zoomInTop, c.zoomControlSize, c.zoomControlSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.resetZoom, zoomResetX, zoomResetTop, c.resetDiskSize, c.resetDiskSize, 44, renderWidth, renderHeight);
    this.setHitbox(this.controls.zoomOut, zoomX, zoomOutTop, c.zoomControlSize, c.zoomControlSize, 44, renderWidth, renderHeight);

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
