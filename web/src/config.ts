export const CONTROL_LAYOUT = {
  rotateArrowWidth: 56,
  rotateArrowHeight: 44,
  resetDiskSize: 38,
  rotateLeftX: 18,
  rotateRowGap: 8,
  zoomControlSize: 32,
  zoomLeft: 18,
  zoomTop: 18,
  zoomGap: 6,
  controlBottom: 18,
  panArrowSize: 38,
  panXStep: 48,
  panYStep: 36,
  panPadRight: 18,
  panPadBottom: 16,
  levelRight: 18,
  levelTop: 18,
  levelGap: 6
} as const;

export const INPUT = {
  panButtonStep: 0.34,
  maxRenderPixels: 360000,
  zoomWheelThreshold: 70,
  pinchStepRatioIn: 1.14,
  pinchStepRatioOut: 0.88,
  yawWheelThreshold: 70,
  touchRotateStep: Math.PI / 8
} as const;

export const BASE_VIEW_HEIGHT = 6.15;
