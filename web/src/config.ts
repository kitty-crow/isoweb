export const CONTROL_LAYOUT = {
  rotateArrowWidth: 56,
  rotateArrowHeight: 44,
  resetDiskSize: 38,
  rotateLeftX: 18,
  rotateRowGap: 8,
  zoomControlSize: 32,
  topLeft: 18,
  topRight: 18,
  topControlTop: 18,
  topControlGap: 6,
  controlBottom: 18,
  panArrowSize: 38,
  panXStep: 48,
  panYStep: 36,
  panPadRight: 18,
  panPadBottom: 16
} as const;

export const INPUT = {
  panButtonStep: 0.34,
  maxRenderPixels: 360000,
  zoomWheelThreshold: 70,
  pinchStepRatioIn: 1.14,
  pinchStepRatioOut: 0.88,
  yawWheelThreshold: 70,
  touchRotateStep: Math.PI / 8,
  joystickDeadzone: 0.16,
  joystickLevels: 16,
  joystickPanUnitsPerSecond: 2.72,
  joystickYawStepsPerSecondMin: 1.25,
  joystickYawStepsPerSecondMax: 4.5,
  joystickZoomStepsPerSecondMin: 1.0,
  joystickZoomStepsPerSecondMax: 4.0,
  joystickLevelStepsPerSecondMin: 0.85,
  joystickLevelStepsPerSecondMax: 2.4
} as const;

export const BASE_VIEW_HEIGHT = 6.15;
