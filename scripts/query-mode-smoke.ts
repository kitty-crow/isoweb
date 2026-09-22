import { parseBrowserMode } from '../web/src/browserMode';

const cases = [
  ['', { detailedZoom:false, detailedYaw:false, stats:false, threaded:false, webgl:false }],
  ['?dzoom', { detailedZoom:true, detailedYaw:false, stats:false, threaded:false, webgl:false }],
  ['?dyaw', { detailedZoom:false, detailedYaw:true, stats:false, threaded:false, webgl:false }],
  ['?stats', { detailedZoom:false, detailedYaw:false, stats:true, threaded:false, webgl:false }],
  ['?threaded', { detailedZoom:false, detailedYaw:false, stats:false, threaded:true, webgl:false }],
  ['?webgl', { detailedZoom:false, detailedYaw:false, stats:false, threaded:false, webgl:true }],
  ['?threaded&webgl', { detailedZoom:false, detailedYaw:false, stats:false, threaded:true, webgl:true }],
  ['?dzoom&dyaw&threaded&webgl&stats',
    { detailedZoom:true, detailedYaw:true, stats:true, threaded:true, webgl:true }],
  ['?stats&webgl&threaded', { detailedZoom:false, detailedYaw:false, stats:true, threaded:true, webgl:true }],
  ['?webgl&stats&threaded', { detailedZoom:false, detailedYaw:false, stats:true, threaded:true, webgl:true }],
  ['?dzoom=0&dyaw=no&stats=0&threaded=false&webgl=anything',
    { detailedZoom:true, detailedYaw:true, stats:true, threaded:true, webgl:true }],
  ['?unrelated=1', { detailedZoom:false, detailedYaw:false, stats:false, threaded:false, webgl:false }]
] as const;

for (const [query, expected] of cases) {
  const actual = parseBrowserMode(query);
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(`Query mode mismatch for ${query}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
  }
}

console.log('Query mode parser passed: all public boolean flags are presence-only, composable, order-independent, and unaffected by unrelated parameters.');
