// compare.mjs: byte-for-byte parity of the native cb_engine.exe and the WASM build on one scenario.
//   node compare.mjs [scenarioName=lab] [--max-field N]
// Works on a temp copy of web-app/data/scenarios/<name> (the pristine folder is never opened by an engine).
// Compares: ready event, read.snapshot/network/queue, sys.hello, match.build + match.compare for every donation,
// net.field for up to N nodes (default 50), all with "bootId" values blanked. Prints timings. Exit 1 on any mismatch.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { loadEngine, copyDirToMemfs } from './engine-node.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const WEB = path.resolve(HERE, '..', '..');
const EXE = process.env.CB_ENGINE || path.join(WEB, 'engine', 'build', 'cb_engine.exe');
const args = process.argv.slice(2);
const name = args.find((a) => !a.startsWith('--')) || 'lab';
const mi = args.indexOf('--max-field');
const MAX_FIELD = mi >= 0 ? parseInt(args[mi + 1], 10) : 50;

const src = path.join(WEB, 'data', 'scenarios', name);
if (!fs.existsSync(src)) { console.error('no scenario', src); process.exit(2); }
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'cb-compare-'));
for (const f of fs.readdirSync(src)) if (f.endsWith('.csv')) fs.copyFileSync(path.join(src, f), path.join(tmp, f));

const blank = (s) => s.replace(/"bootId":"[^"]*"/g, '"bootId":"*"').replace(/"dataDir":"[^"]*"/g, '"dataDir":"*"');
const pct = (xs, p) => { const s = [...xs].sort((a, b) => a - b); return s[Math.min(s.length - 1, Math.floor(p * s.length))]; };

// ---- WASM
const tw0 = performance.now();
const eng = await loadEngine();
const tw1 = performance.now();
copyDirToMemfs(eng.FS, tmp, '/data');
const wReady = eng.init('/data');
const tw2 = performance.now();
const ready = JSON.parse(wReady);
if (ready.event !== 'ready') { console.error('wasm boot failed', wReady); process.exit(1); }

let id = 0;
const reqs = [];
const add = (cmd, a = {}) => reqs.push(JSON.stringify({ v: 1, id: ++id, cmd, args: a }));
add('sys.hello'); add('read.snapshot'); add('read.network'); add('read.queue');
const snap = JSON.parse(eng.command(JSON.stringify({ v: 1, id: 0, cmd: 'read.snapshot', args: {} }))).data;
const donations = snap.donations.map((d) => d.id);
const nodeCount = JSON.parse(eng.command(JSON.stringify({ v: 1, id: 0, cmd: 'read.network', args: {} }))).data.nodes.length;
const firstBuild = id + 1;
for (const d of donations) add('match.build', { donationId: d });
const lastBuild = id;
for (const d of donations) add('match.compare', { donationId: d });
const step = Math.max(1, Math.floor(nodeCount / MAX_FIELD));
for (let n = 0, k = 0; n < nodeCount && k < MAX_FIELD; n += step, k++) add('net.field', { source: n });

const wOut = []; const wLat = [];
for (const r of reqs) {
  const t = performance.now();
  wOut.push(eng.command(r));
  const dt = performance.now() - t;
  const rid = JSON.parse(r).id;
  if (rid >= firstBuild && rid <= lastBuild) wLat.push(dt);
}
// warm second pass for match.build latency
const wLat2 = [];
for (const d of donations) { const t = performance.now(); eng.command(JSON.stringify({ v: 1, id: 1, cmd: 'match.build', args: { donationId: d } })); wLat2.push(performance.now() - t); }

// ---- native
const tn0 = performance.now();
const p = spawn(EXE, ['--data', tmp], { stdio: ['pipe', 'pipe', 'pipe'] });
const lines = [];
let buf = '';
const want = reqs.length + 1;
const nDone = new Promise((resolve, reject) => {
  p.stdout.setEncoding('utf8');
  p.stdout.on('data', (c) => {
    buf += c; let i;
    while ((i = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, i)); buf = buf.slice(i + 1); }
    if (lines.length >= want) resolve();
  });
  p.on('exit', () => (lines.length >= want ? resolve() : reject(new Error('native exited early, ' + lines.length + ' lines'))));
});
p.stdin.write(reqs.join('\n') + '\n');
await nDone;
const tn1 = performance.now();
p.stdin.end();

// ---- compare
let mism = 0; const bad = [];
if (blank(lines[0]) !== blank(wReady)) { mism++; bad.push('ready'); }
for (let k = 0; k < reqs.length; k++) {
  if (blank(lines[k + 1]) !== blank(wOut[k])) { mism++; if (bad.length < 10) bad.push(JSON.parse(reqs[k]).cmd + ' ' + JSON.stringify(JSON.parse(reqs[k]).args)); }
}
const nBuild = lastBuild - firstBuild + 1;
const bytes = wOut.reduce((s, x) => s + x.length, 0);
console.log(`scenario ${name}: ${ready.counts ? JSON.stringify(ready.counts) : ''} nodes ${nodeCount}`);
console.log(`compared ${reqs.length + 1} responses (${nBuild} match.build, ${nBuild} match.compare, ${reqs.length - 4 - 2 * nBuild} net.field, 4 reads, ready), ${(bytes / 1024).toFixed(0)} KiB`);
console.log(`MISMATCHES: ${mism}` + (bad.length ? '  first: ' + bad.join(' | ') : ''));
console.log(`wasm: module load ${(tw1 - tw0).toFixed(1)} ms, copy+boot ${(tw2 - tw1).toFixed(1)} ms`);
console.log(`wasm match.build: first pass p50 ${pct(wLat, 0.5).toFixed(3)} ms p95 ${pct(wLat, 0.95).toFixed(3)} ms; warm p50 ${pct(wLat2, 0.5).toFixed(3)} ms p95 ${pct(wLat2, 0.95).toFixed(3)} ms max ${Math.max(...wLat2).toFixed(3)} ms`);
console.log(`native: boot + all ${reqs.length} requests ${(tn1 - tn0).toFixed(1)} ms (incl. process start)`);
fs.rmSync(tmp, { recursive: true, force: true });
process.exit(mism ? 1 : 0);

