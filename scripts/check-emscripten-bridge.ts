const generatedPath = 'site/assets/isoweb.js';

interface BridgeIssue {
  id: string;
  missing: string[];
}

function findMatchingBrace(source: string, openIndex: number): number {
  let depth = 0;
  let quote = '';
  let escaped = false;

  for (let index = openIndex; index < source.length; index += 1) {
    const character = source[index];

    if (quote) {
      if (escaped) {
        escaped = false;
      } else if (character === '\\') {
        escaped = true;
      } else if (character === quote) {
        quote = '';
      }
      continue;
    }

    if (character === '"' || character === "'" || character === '`') {
      quote = character;
      continue;
    }

    if (character === '{') depth += 1;
    if (character === '}') {
      depth -= 1;
      if (depth === 0) return index;
    }
  }

  throw new Error(`Unterminated ASM_CONSTS function starting at byte ${openIndex}.`);
}

function inspectAsmConstArguments(source: string): BridgeIssue[] {
  const marker = 'var ASM_CONSTS={';
  const start = source.indexOf(marker);
  if (start < 0) throw new Error('Generated JavaScript does not contain ASM_CONSTS.');

  const issues: BridgeIssue[] = [];
  const header = /(\d+):\(([^)]*)\)=>\{/g;
  header.lastIndex = start + marker.length;

  for (;;) {
    const match = header.exec(source);
    if (!match) break;

    const openBrace = header.lastIndex - 1;
    const closeBrace = findMatchingBrace(source, openBrace);
    const body = source.slice(openBrace + 1, closeBrace);
    const declared = new Set(match[2].split(',').map(value => value.trim()).filter(Boolean));
    const referenced = new Set(body.match(/\$\d+\b/g) ?? []);
    const missing = [...referenced].filter(reference => !declared.has(reference));

    if (missing.length > 0) issues.push({ id: match[1], missing });
    header.lastIndex = closeBrace + 1;
  }

  return issues;
}

function selfTest(): void {
  const deliberatelyBroken = 'var ASM_CONSTS={1:($0,$1)=>{return $0+$2}};';
  const issues = inspectAsmConstArguments(deliberatelyBroken);
  if (issues.length !== 1 || issues[0].missing.join(',') !== '$2') {
    throw new Error('ASM_CONSTS regression checker failed its own negative fixture.');
  }
}

selfTest();

const generated = await Bun.file(generatedPath).text();
const issues = inspectAsmConstArguments(generated);

if (issues.length > 0) {
  for (const issue of issues) {
    console.error(`ASM_CONSTS[${issue.id}] references undeclared argument(s): ${issue.missing.join(', ')}`);
  }
  process.exit(1);
}

console.log('Generated Emscripten bridge argument check passed.');
