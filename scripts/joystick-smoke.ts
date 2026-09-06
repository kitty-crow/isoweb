import { INPUT } from '../web/src/config';
import { quantiseAxisStick, quantisePanStick } from '../web/src/controls/CentreJoystick';

function assert(condition: boolean, message: string): void {
  if (!condition) throw new Error(message);
}

function near(a: number, b: number, tolerance = 1e-8): boolean {
  return Math.abs(a - b) <= tolerance;
}

const neutral = quantisePanStick(INPUT.joystickDeadzone * 0.9, 0);
assert(neutral.level === 0 && neutral.x === 0 && neutral.y === 0, 'deadzone must stay neutral');

const fullRight = quantisePanStick(1, 0);
assert(fullRight.level === 16, 'full pan pull must be strength 16');
assert(near(fullRight.x, 1) && near(fullRight.y, 0), 'right pan direction must remain cardinal');

const diagonal = quantisePanStick(0.8, 0.8);
const diagonalComponent = Math.SQRT1_2;
assert(diagonal.level === 16, 'clamped diagonal must reach strength 16');
assert(
  near(diagonal.x, diagonalComponent) && near(diagonal.y, diagonalComponent),
  'pan direction must snap to the nearest 45 degree direction'
);

const almostRight = quantisePanStick(0.9, 0.25);
assert(near(almostRight.y, 0), 'angle below 22.5 degrees must snap to horizontal');

for (let expectedLevel = 1; expectedLevel <= INPUT.joystickLevels; expectedLevel++) {
  const activeFraction = (expectedLevel - 0.5) / INPUT.joystickLevels;
  const magnitude = INPUT.joystickDeadzone + activeFraction * (1 - INPUT.joystickDeadzone);
  const quantised = quantiseAxisStick(magnitude, true);
  assert(quantised.level === expectedLevel, `expected joystick strength level ${expectedLevel}`);
  assert(
    near(quantised.strength, expectedLevel / INPUT.joystickLevels),
    `strength ${expectedLevel} must be exactly one of 16 levels`
  );
}

const yawLeft = quantiseAxisStick(-0.75, true);
assert(yawLeft.x < 0 && yawLeft.y === 0, 'yaw/zoom sticks must be horizontal-only');

const levelUp = quantiseAxisStick(-0.75, false);
assert(levelUp.x === 0 && levelUp.y < 0, 'level stick must be vertical-only');

console.log('Joystick quantisation smoke test passed: 8 directions, 16 strength levels, locked axes.');
