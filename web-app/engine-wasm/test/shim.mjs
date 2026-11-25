// shim.mjs: runs the WASM engine in Node with the same command line and stdin/stdout JSON-lines protocol as
// cb_engine.exe, so engine/tests/test_engine.py can drive either engine.
//   node shim.mjs --data <folder> [--protocol 1] [--log-level x] [--version]
// The folder is copied into MEMFS (/data); after every command that changed rev (or answered with effects) the
// changed MEMFS files are written back to the host folder, so a restart sees the saved CSVs like the native exe.
// Exit codes: 0 clean, 2 boot failed, 64 usage, 70 fatal. Set CB_SHIM_TIMING=1 for load/boot timings on stderr.
import fs from 'node:fs';
import { loadEngine, copyDirToMemfs, syncMemfsToDir } from './engine-node.mjs';

function logLine(level, msg) { process.stderr.write(JSON.stringify({ level, msg }) + '\n'); }
const USAGE = 'usage: cb_engine --data <folder> [--protocol 1] [--log-level info|debug] [--self-test] [--version]';

const argv = process.argv.slice(2);
let data = '';
for (let k = 0; k < argv.length; k++) {
  const a = argv[k];
  if (a === '--data' && k + 1 < argv.length) data = argv[++k];
  else if (a === '--protocol' && k + 1 < argv.length) { if (argv[++k] !== '1') { logLine('error', 'only protocol 1 is supported'); process.exit(64); } }
  else if (a === '--log-level' && k + 1 < argv.length) ++k;
  else if (a === '--version') { process.stdout.write('cb_engine 0.1.0 protocol 1 (wasm)\n'); process.exit(0); }
  else { logLine('error', USAGE); process.exit(64); }
}
if (!data) { logLine('error', USAGE); process.exit(64); }

const t0 = performance.now();
const eng = await loadEngine();
const t1 = performance.now();
const MEM = '/data';
const cache = copyDirToMemfs(eng.FS, data, MEM);
const ready = eng.initBytes(MEM);
const t2 = performance.now();
if (process.env.CB_SHIM_TIMING) logLine('info', `wasm load ${(t1 - t0).toFixed(1)} ms, copy+boot ${(t2 - t1).toFixed(1)} ms`);

const out = [];
function emit(buf) { out.push(buf, Buffer.from('\n')); }
function flushAndExit(code) {
  const all = Buffer.concat(out); out.length = 0;
  if (all.length === 0) process.exit(code);
  process.stdout.write(all, () => process.exit(code));
}

emit(ready);
if (eng.rev() < 0) flushAndExit(2);
else {
  let lastRev = eng.rev();
  let pending = Buffer.alloc(0);
  let done = false;
  const flush = () => { if (out.length) { const all = Buffer.concat(out); out.length = 0; process.stdout.write(all); } };
  flush();                                          // the ready line goes out before any request is read

  function handle(lineBuf) {
    const reply = eng.commandBytes(lineBuf);
    if (reply.length > 0) emit(reply);
    const rev = eng.rev();
    if (rev < 0) {                                  // sys.shutdown or fatal: the engine was released
      syncMemfsToDir(eng.FS, MEM, data, cache);
      done = true;
      const fatal = reply.includes('"Out of memory"');
      flushAndExit(fatal ? 70 : 0);
      return;
    }
    if (rev !== lastRev || reply.includes('"effects"')) syncMemfsToDir(eng.FS, MEM, data, cache);
    lastRev = rev;
  }

  process.stdin.on('data', (chunk) => {
    if (done) return;
    pending = pending.length ? Buffer.concat([pending, chunk]) : chunk;
    let start = 0, nl;
    while (!done && (nl = pending.indexOf(10, start)) !== -1) {
      handle(pending.subarray(start, nl));
      start = nl + 1;
    }
    pending = pending.subarray(start);
    if (!done) flush();
  });
  process.stdin.on('end', () => { if (!done) { done = true; flushAndExit(0); } });
}
