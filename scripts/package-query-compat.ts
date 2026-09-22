import { mkdir, writeFile } from 'node:fs/promises';

function page(mode: 'threaded' | 'webgl'): string {
  return '<!doctype html>\n' +
    '<html lang="en-GB">\n' +
    '<head>\n' +
    '  <meta charset="utf-8">\n' +
    '  <meta name="viewport" content="width=device-width, initial-scale=1">\n' +
    '  <title>IsoWeb mode redirect</title>\n' +
    '</head>\n' +
    '<body>\n' +
    '  <script>\n' +
    '    (() => {\n' +
    "      const target = new URL('../', location.href);\n" +
    '      const existing = new URLSearchParams(location.search);\n' +
    "      existing.set('" + mode + "', '');\n" +
    '      target.search = existing.toString();\n' +
    '      location.replace(target);\n' +
    '    })();\n' +
    '  </script>\n' +
    '</body>\n' +
    '</html>\n';
}

for (const mode of ['threaded', 'webgl'] as const) {
  await mkdir(`site/${mode}`, { recursive: true });
  await writeFile(`site/${mode}/index.html`, page(mode));
}

console.log('Legacy renderer routes packaged as query-mode compatibility redirects.');
