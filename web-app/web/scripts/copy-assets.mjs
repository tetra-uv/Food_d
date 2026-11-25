// Copies the static assets local mode needs into web/public and writes public/cb-manifest.json.
//   ../engine-wasm/dist/*                      -> public/engine/
//   ../data/scenarios/<name>/{*.csv,geo_overlay.json} -> public/scenarios/<name>/   (delhi-real, delhi-geo, lab)
// Sources are read-only. A missing source is skipped with a warning; if public/ already holds a copy from an earlier
// run (for example on Vercel, where only web/ is uploaded) that copy is kept. The manifest is always rebuilt from what is in
// public/, with a short content hash per file (used as ?v= cache buster by the app).
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const web = path.resolve(here, '..');
const root = path.resolve(web, '..');               // web-app
const pub = path.join(web, 'public');
const SCENARIOS = [
  { name: 'delhi-real', label: 'Delhi - real roads', blurb: 'Real road network and place locations from OpenStreetMap; organisations simulated.' },
  { name: 'delhi-geo', label: 'Delhi - 20 places', blurb: 'Twenty Delhi places, travel minutes from OSRM routing.' },
  { name: 'lab', label: 'Lab (schematic)', blurb: 'The original desktop app sample data on a schematic map.' },
];
const warn = (m) => console.warn(`[copy-assets] WARNING: ${m}`);
const info = (m) => console.log(`[copy-assets] ${m}`);

function copyDir(src, dst, filter) {
  fs.rmSync(dst, { recursive: true, force: true });
  fs.mkdirSync(dst, { recursive: true });
  let n = 0, bytes = 0;
  for (const f of fs.readdirSync(src)) {
    const s = path.join(src, f);
    if (!fs.statSync(s).isFile() || !filter(f)) continue;
    fs.copyFileSync(s, path.join(dst, f));
    n++; bytes += fs.statSync(s).size;
  }
  return { n, bytes };
}
function hashes(dir) {
  const out = {}; let bytes = 0;
  if (!fs.existsSync(dir)) return { files: out, bytes };
  for (const f of fs.readdirSync(dir).sort()) {
    const p = path.join(dir, f);
    if (!fs.statSync(p).isFile()) continue;
    const buf = fs.readFileSync(p);
    out[f] = crypto.createHash('sha256').update(buf).digest('hex').slice(0, 12);
    bytes += buf.length;
  }
  return { files: out, bytes };
}
const kb = (b) => `${(b / 1024).toFixed(0)} KB`;

fs.mkdirSync(pub, { recursive: true });

// ---- engine
const engSrc = path.join(root, 'engine-wasm', 'dist');
const engDst = path.join(pub, 'engine');
if (fs.existsSync(path.join(engSrc, 'cb_engine.mjs'))) {
  const r = copyDir(engSrc, engDst, (f) => /\.(mjs|js|wasm|data)$/i.test(f));
  info(`engine: ${r.n} files, ${kb(r.bytes)}`);
} else if (fs.existsSync(path.join(engDst, 'cb_engine.mjs'))) warn(`${engSrc} not found; keeping the existing public/engine copy`);
else warn(`${engSrc}\\cb_engine.mjs not found; local mode will not work until the WASM engine is built`);

// ---- scenarios
const scenarios = [];
for (const s of SCENARIOS) {
  const src = path.join(root, 'data', 'scenarios', s.name);
  const dst = path.join(pub, 'scenarios', s.name);
  if (fs.existsSync(path.join(src, 'donations.csv'))) {
    const r = copyDir(src, dst, (f) => /\.csv$/i.test(f) || f === 'geo_overlay.json');
    info(`scenario ${s.name}: ${r.n} files, ${kb(r.bytes)}`);
  } else if (fs.existsSync(path.join(dst, 'donations.csv'))) warn(`scenario ${s.name}: source missing; keeping the existing public copy`);
  else { warn(`scenario ${s.name}: not found at ${src}; skipped`); continue; }
  const h = hashes(dst);
  scenarios.push({ ...s, files: h.files, geo: 'geo_overlay.json' in h.files, bytes: h.bytes });
}

const eng = hashes(engDst);
const manifest = {
  generatedAt: new Date().toISOString(),
  engine: eng.files['cb_engine.mjs'] ? { files: eng.files, bytes: eng.bytes } : null,
  scenarios,
  default: scenarios[0]?.name ?? null,
};
fs.writeFileSync(path.join(pub, 'cb-manifest.json'), JSON.stringify(manifest, null, 2));
info(`manifest: engine ${manifest.engine ? kb(eng.bytes) : 'MISSING'}, scenarios ${scenarios.map((s) => `${s.name} (${kb(s.bytes)})`).join(', ') || 'NONE'}`);
