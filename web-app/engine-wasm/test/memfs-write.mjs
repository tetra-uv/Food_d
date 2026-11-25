// memfs-write.mjs: write path inside MEMFS. On a temp copy of the lab scenario: offer.send 40 (DON001 -> R013),
// offer.accept; checks the CSVs rewritten in MEMFS (via temp file + rename), that cb_init on the written files
// reproduces the same state, and that the MEMFS CSVs equal what the native exe writes for the same flow.
//   node memfs-write.mjs        exit 1 on any failed check
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { loadEngine, copyDirToMemfs, listMemfs } from './engine-node.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const WEB = path.resolve(HERE, '..', '..');
const EXE = process.env.CB_ENGINE || path.join(WEB, 'engine', 'build', 'cb_engine.exe');
const LAB = path.join(WEB, 'data', 'scenarios', 'lab');

let pass = 0, fail = 0;
const check = (n, c, d) => { if (c) pass++; else { fail++; console.log('FAIL:', n, d === undefined ? '' : String(d).slice(0, 300)); } };
const copy = () => { const t = fs.mkdtempSync(path.join(os.tmpdir(), 'cb-memfs-')); for (const f of fs.readdirSync(LAB)) if (f.endsWith('.csv')) fs.copyFileSync(path.join(LAB, f), path.join(t, f)); return t; };

const tmp = copy();
const eng = await loadEngine();
const FS = eng.FS;
copyDirToMemfs(FS, tmp, '/data');
const before = listMemfs(FS, '/data');

// record the file operations the backend performs
const ops = [];
const origRename = FS.rename.bind(FS);
FS.rename = (a, b) => { ops.push(`rename ${a} -> ${b}`); return origRename(a, b); };
const origUnlink = FS.unlink.bind(FS);
FS.unlink = (a) => { ops.push(`unlink ${a}`); return origUnlink(a); };

const ready = JSON.parse(eng.init('/data'));
check('boot ready', ready.event === 'ready', JSON.stringify(ready));
let n = 0;
const call = (cmd, args = {}) => JSON.parse(eng.command(JSON.stringify({ v: 1, id: ++n, cmd, args })));

const s = call('offer.send', { donationId: 'DON001', recipientId: 'R013', quantity: 40 });
check('offer.send ok OFF001 rev 1', s.ok && s.rev === 1 && s.data.offer.id === 'OFF001', JSON.stringify(s));
const a = call('offer.accept', { offerId: 'OFF001' });
check('offer.accept ok T004 rev 2', a.ok && a.rev === 2 && a.data.transaction.id === 'T004', JSON.stringify(a));

const after = listMemfs(FS, '/data');
const txt = (f) => (after.get(f) || Buffer.alloc(0)).toString('utf8');
check('offers.csv created in MEMFS with OFF001 ACCEPTED', /^OFF001,DON001,R013,40,.*ACCEPTED/m.test(txt('offers.csv')), txt('offers.csv'));
check('transactions.csv has T004,DON001,R013,40,8', txt('transactions.csv').includes('T004,DON001,R013,40,8'), txt('transactions.csv'));
check('recipients.csv R013 capacity 250 -> 210', /^R013,.*,210,/m.test(txt('recipients.csv')) && /^R013,.*,250,/m.test(before.get('recipients.csv').toString()), txt('recipients.csv').split('\n').find((l) => l.startsWith('R013')));
check('donations.csv DON001 remaining 20 PARTIAL', /^DON001,DN001,COOKED,60,20,12,PARTIAL/m.test(txt('donations.csv')), txt('donations.csv').split('\n').find((l) => l.startsWith('DON001')));
check('graph files untouched', after.get('graph_nodes.csv').equals(before.get('graph_nodes.csv')) && after.get('graph_edges.csv').equals(before.get('graph_edges.csv')));
check('no temp files left in /data', [...after.keys()].every((f) => f.endsWith('.csv')), [...after.keys()].join(','));
const renames = ops.filter((o) => o.startsWith('rename'));
check('saves go through temp file + rename', renames.length >= 3 && renames.every((o) => /rename \S+ -> \/data\/\w+\.csv$/.test(o)), ops.join('; '));

// re-boot on the written files
const snapA = call('read.snapshot').data;
const re = JSON.parse(eng.init('/data'));
check('cb_init again: ready, rev 0, new bootId', re.event === 'ready' && re.bootId !== ready.bootId, JSON.stringify(re).slice(0, 200));
n = 0;
const snapB = call('read.snapshot').data;
const strip = (o) => JSON.stringify(o).replace(/"bootId":"[^"]*"/g, '');
check('state after re-boot equals the state before it', strip(snapA) === strip(snapB));
check('re-booted state: R013 210, 4 transactions, OFF001 ACCEPTED', snapB.recipients.find((r) => r.id === 'R013').availableCapacity === 210 && snapB.transactions.length === 4 && snapB.offers[0].status === 'ACCEPTED');
const nx = call('offer.send', { donationId: 'DON001', recipientId: 'R004', quantity: 20 });
check('ids continue after re-boot (OFF002)', nx.ok && nx.data.offer.id === 'OFF002', JSON.stringify(nx).slice(0, 200));

// native exe, same flow, compare the CSV files byte for byte
const tmpN = copy();
const lines = [
  { v: 1, id: 1, cmd: 'offer.send', args: { donationId: 'DON001', recipientId: 'R013', quantity: 40 } },
  { v: 1, id: 2, cmd: 'offer.accept', args: { offerId: 'OFF001' } },
  { v: 1, id: 3, cmd: 'sys.shutdown', args: {} },
].map((x) => JSON.stringify(x)).join('\n') + '\n';
const r = spawnSync(EXE, ['--data', tmpN], { input: lines });
check('native flow exit 0', r.status === 0, r.stderr.toString());
for (const f of ['offers.csv', 'transactions.csv', 'recipients.csv', 'donations.csv', 'donors.csv']) {
  const nat = fs.readFileSync(path.join(tmpN, f));
  const lf = (b) => b.toString('utf8').replace(/\r\n/g, '\n');   // the Windows exe writes CRLF (text-mode streams), WASM writes LF
  check(`MEMFS ${f} == native ${f} (bytes, line endings normalised)`, lf(nat) === lf(after.get(f)), `${nat.length} vs ${after.get(f)?.length}`);
  if (f === 'offers.csv') check('MEMFS files use LF only', !after.get(f).includes(13));
}

console.log('file operations:', ops.slice(0, 8).join('; '));
fs.rmSync(tmp, { recursive: true, force: true });
fs.rmSync(tmpN, { recursive: true, force: true });
console.log(`MEMFS WRITE TESTS: ${pass} PASS, ${fail} FAIL`);
process.exit(fail ? 1 : 0);

