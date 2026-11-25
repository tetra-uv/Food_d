// Scenario catalogue for local mode. scripts/copy-assets.mjs writes public/cb-manifest.json listing the engine files and
// the scenario folders it copied (each file with a short content hash used as ?v= cache buster).

export interface ScenarioInfo { name: string; label: string; blurb?: string; files: Record<string, string>; geo: boolean; bytes: number }
export interface AssetManifest {
  generatedAt: string;
  engine: { files: Record<string, string> } | null;
  scenarios: ScenarioInfo[];
  default: string | null;
}

const BASE = import.meta.env.BASE_URL || '/';
export const assetUrl = (p: string, v?: string) => `${BASE}${p}${v ? `?v=${v}` : ''}`;

let manifestP: Promise<AssetManifest> | null = null;
export function loadManifest(): Promise<AssetManifest> {
  if (!manifestP) {
    manifestP = fetch(assetUrl('cb-manifest.json'), { cache: 'no-cache' }).then(async (r) => {
      if (!r.ok) throw new Error(`cb-manifest.json missing (${r.status}); run npm run assets`);
      return (await r.json()) as AssetManifest;
    });
    manifestP.catch(() => { manifestP = null; });
  }
  return manifestP;
}

const KEY = 'cb-scenario';
/** The scenario this tab runs: ?scenario= in the URL, else the last one picked (localStorage), else the manifest default. */
export function chosenScenario(m: AssetManifest): ScenarioInfo {
  const names = new Set(m.scenarios.map((s) => s.name));
  let want: string | null = null;
  try { want = new URLSearchParams(location.search).get('scenario'); } catch { /* ignore */ }
  if (!want || !names.has(want)) { try { want = localStorage.getItem(KEY); } catch { want = null; } }
  if (!want || !names.has(want)) want = m.default && names.has(m.default) ? m.default : m.scenarios[0]?.name ?? null;
  const s = m.scenarios.find((x) => x.name === want);
  if (!s) throw new Error('No scenario was packaged with this site (cb-manifest.json lists none)');
  return s;
}

/** Switch scenario: remember it and reload the page (a fresh engine boots on the new data). */
export function switchScenario(name: string) {
  try { localStorage.setItem(KEY, name); } catch { /* private mode: URL param still works */ }
  const u = new URL(location.href);
  u.searchParams.set('scenario', name);
  u.hash = '#/';
  location.href = u.toString();
}
