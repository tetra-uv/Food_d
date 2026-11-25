// Main-thread side of local mode: owns the engine worker, the scenario files, persistence and cross-tab coordination.
//
// State model (per scenario):
//   pristine files  = /scenarios/<name>/*.csv shipped with the site (read-only)
//   saved copy      = IndexedDB record {stamp, revision, files} written after every successful write (any tab)
//   engine copy     = MEMFS /data inside this tab's worker; `this.stamp` says which saved copy it was loaded from
//                     (null = pristine)
// Writes run inside navigator.locks 'cb-write-<scenario>': if the saved stamp differs from ours (another tab wrote or reset),
// reload the saved copy first, run the command, read the CSVs back from MEMFS, save them with a new stamp, then broadcast on
// BroadcastChannel 'cb-<scenario>'. Other tabs reload from IndexedDB when they hear it. Without Web Locks or
// BroadcastChannel we run single-tab (still correct in one tab; other tabs only catch up when they regain focus).
import { assetUrl, chosenScenario, loadManifest, type AssetManifest, type ScenarioInfo } from './scenarios';
import { clearState, idbAvailable, loadState, saveState } from './idb';
import type { GeoOverlay } from '../api/types';
import { applyEffects, emptyActivity, type Activity } from './activity';

export interface EngineResponse {
  id?: number; ok: boolean; rev?: number; data?: any; message?: string; effects?: any;
  error?: { code: string; message: string; details?: any };
}
export interface LocalEventMsg { type: string; data: Record<string, unknown>; audience: { donors?: string[]; recipients?: string[] }; broadcast: boolean }
type ChannelMsg =
  | { kind: 'changed'; from: string; stamp: string; events: LocalEventMsg[] }
  | { kind: 'reset'; from: string };

export interface LocalStatus {
  scenario: ScenarioInfo; manifest: AssetManifest;
  multiTab: boolean; persistent: boolean; revision: number; savedAt: string | null; notes: string[];
}

const WRITE_RE = /^(offer\.(send|accept|decline|withdraw)|donation\.(create|close|createWithNewDonor)|donor\.create|recipient\.(create|update)|sys\.reload)$/;
const tabId = Math.random().toString(36).slice(2, 10);

class LocalEngine {
  manifest!: AssetManifest;
  scenario!: ScenarioInfo;
  geo: GeoOverlay | null = null;
  ready: Record<string, unknown> | null = null;
  /** bumps on every load and write; used as the snapshot cache key */
  stateKey = 0;
  stamp: string | null = null;
  revision = 0;
  savedAt: string | null = null;
  persistent = false;
  multiTab = false;
  notes: string[] = [];
  /** timestamps for offers/deliveries/donations (presentation only, never sent to the engine) */
  activity: Activity = emptyActivity();

  private worker: Worker | null = null;
  private seq = 0;
  private waiting = new Map<number, { resolve: (v: any) => void; reject: (e: Error) => void }>();
  private chain: Promise<unknown> = Promise.resolve();
  private channel: BroadcastChannel | null = null;
  private listeners = new Set<(ev: LocalEventMsg) => void>();
  private cmdId = 1;
  private pristine: Record<string, string> | null = null;

  async start() {
    this.manifest = await loadManifest();
    if (!this.manifest.engine) throw new Error('The WebAssembly engine was not packaged with this site (public/engine is empty)');
    this.scenario = chosenScenario(this.manifest);
    this.persistent = await idbAvailable();
    const locks = typeof navigator !== 'undefined' && 'locks' in navigator && !!navigator.locks;
    const bc = typeof BroadcastChannel !== 'undefined';
    this.multiTab = locks && bc;
    if (!this.persistent) this.notes.push('This browser does not allow storage here (private window?), so changes last only until you reload.');
    if (!this.multiTab) this.notes.push('Single-tab mode: this browser lacks Web Locks or BroadcastChannel, so other tabs will not update live.');
    if (bc) {
      this.channel = new BroadcastChannel(`cb-${this.scenario.name}`);
      this.channel.onmessage = (e) => void this.onChannel(e.data as ChannelMsg);
    }
    if (typeof document !== 'undefined') {
      document.addEventListener('visibilitychange', () => { if (document.visibilityState === 'visible') void this.catchUp(); });
    }
    const geoHash = this.scenario.files['geo_overlay.json'];
    const geoP = geoHash ? fetch(assetUrl(`scenarios/${this.scenario.name}/geo_overlay.json`, geoHash)).then((r) => (r.ok ? r.json() : null)).catch(() => null) : Promise.resolve(null);
    await this.exclusive(() => this.loadSaved());
    this.geo = (await geoP) as GeoOverlay | null;
  }

  // ------------------------------------------------------------------ worker plumbing
  private ensureWorker(): Worker {
    if (!this.worker) {
      this.worker = new Worker(new URL('./engine.worker.ts', import.meta.url), { type: 'module' });
      this.worker.onmessage = (e) => {
        const m = e.data as { id: number; ok: boolean; error?: string };
        const w = this.waiting.get(m.id);
        if (!w) return;
        this.waiting.delete(m.id);
        if (m.ok) w.resolve(m); else w.reject(new Error(m.error || 'engine worker error'));
      };
      this.worker.onerror = (e) => {
        for (const w of this.waiting.values()) w.reject(new Error(e.message || 'engine worker crashed'));
        this.waiting.clear();
      };
    }
    return this.worker;
  }
  private post<T>(msg: Record<string, unknown>): Promise<T> {
    const id = ++this.seq;
    const w = this.ensureWorker();
    return new Promise<T>((resolve, reject) => { this.waiting.set(id, { resolve, reject }); w.postMessage({ ...msg, id }); });
  }
  /** Runs fn after everything queued before it in this tab (engine access is strictly serial, like the gateway). */
  exclusive<T>(fn: () => Promise<T>): Promise<T> {
    const p = this.chain.then(fn, fn);
    this.chain = p.catch(() => undefined);
    return p;
  }

  // ------------------------------------------------------------------ loading
  private async fetchPristine(): Promise<Record<string, string>> {
    if (this.pristine) return this.pristine;
    const out: Record<string, string> = {};
    await Promise.all(Object.entries(this.scenario.files).filter(([n]) => /\.csv$/i.test(n)).map(async ([name, hash]) => {
      const r = await fetch(assetUrl(`scenarios/${this.scenario.name}/${name}`, hash));
      if (!r.ok) throw new Error(`Could not download ${name} (${r.status})`);
      out[name] = await r.text();
    }));
    this.pristine = out;
    return out;
  }
  private async boot(files: Record<string, string>) {
    const eng = this.manifest.engine!.files;
    const wasmName = Object.keys(eng).find((n) => n.endsWith('.wasm'));
    const r = await this.post<{ ready: Record<string, unknown>; logs: string[] }>({
      type: 'load', files,
      engineUrl: new URL(assetUrl('engine/cb_engine.mjs', eng['cb_engine.mjs']), location.href).toString(),
      wasmUrl: wasmName ? new URL(assetUrl(`engine/${wasmName}`, eng[wasmName]), location.href).toString() : '',
    });
    if (r.ready.event !== 'ready') {
      const err = (r.ready.error as { message?: string } | undefined)?.message ?? JSON.stringify(r.ready).slice(0, 300);
      throw new Error(`The engine could not start on this data: ${err}`);
    }
    this.ready = r.ready;
    this.stateKey++;
  }
  /** Load the saved copy (or the pristine files when there is none). Call inside exclusive(). */
  private async loadSaved() {
    let saved = null;
    if (this.persistent) { try { saved = await loadState(this.scenario.name); } catch { saved = null; } }
    if (saved) {
      try {
        await this.boot(saved.files);
        this.stamp = saved.stamp; this.revision = saved.revision; this.savedAt = saved.savedAt;
        this.activity = saved.activity ?? emptyActivity();
        return;
      } catch (e) {
        this.notes.push(`The saved data could not be loaded (${(e as Error).message}); started from the original data instead.`);
      }
    }
    await this.boot(await this.fetchPristine());
    this.stamp = null; this.revision = 0; this.savedAt = null; this.activity = emptyActivity();
  }
  private async savedStamp(): Promise<string | null> {
    if (!this.persistent) return this.stamp;
    try { return (await loadState(this.scenario.name))?.stamp ?? null; } catch { return this.stamp; }
  }

  // ------------------------------------------------------------------ commands
  private async raw(cmd: string, args: Record<string, unknown>): Promise<EngineResponse> {
    const line = JSON.stringify({ v: 1, id: this.cmdId++, cmd, args });
    const r = await this.post<{ line: string }>({ type: 'cmd', line });
    try { return JSON.parse(r.line) as EngineResponse; } catch { throw new Error(`engine answered something that is not JSON: ${String(r.line).slice(0, 200)}`); }
  }
  read(cmd: string, args: Record<string, unknown> = {}, signal?: AbortSignal): Promise<EngineResponse> {
    // a superseded what-if preview is dropped when its turn comes instead of keeping the worker busy
    return this.exclusive(() => { if (signal?.aborted) return Promise.reject(new DOMException('Aborted', 'AbortError')); return this.raw(cmd, args); });
  }
  isWrite(cmd: string) { return WRITE_RE.test(cmd); }

  /**
   * One write, serialised across tabs. Returns the engine answer and, when state changed, a fresh snapshot (read under
   * the same lock so the events describe exactly this change).
   * `precheck` (optional) runs inside the lock on the current snapshot before the command; returning an error refuses
   * the write without touching the engine (workflow guards such as DUPLICATE_PENDING_OFFER, see api.ts).
   */
  write(cmd: string, args: Record<string, unknown>, precheck?: (snap: any) => EngineResponse['error'] | null): Promise<{ resp: EngineResponse; snap: any | null }> {
    const body = () => this.exclusive(async () => {
      if ((await this.savedStamp()) !== this.stamp) { await this.loadSaved(); }
      if (precheck) {
        const s0 = await this.raw('read.snapshot', {});
        const refusal = s0.ok ? precheck(s0.data) : null;
        if (refusal) return { resp: { ok: false, error: refusal } as EngineResponse, snap: null };
      }
      const resp = await this.raw(cmd, args);
      const changed = resp.ok || resp.error?.details?.stateChanged === true;
      if (!changed) return { resp, snap: null };
      this.stateKey++;
      const files = (await this.post<{ files: Record<string, string> }>({ type: 'files' })).files;
      const stamp = `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 10)}`;
      const revision = this.revision + 1, savedAt = new Date().toISOString();
      const activity = applyEffects(this.activity, resp.ok ? resp.effects : resp.error?.details?.effects, Date.now());
      if (this.persistent) {
        try { await saveState({ scenario: this.scenario.name, stamp, revision, savedAt, files, activity }); }
        catch (e) { this.notes.push(`Saving to this browser failed (${(e as Error).message}); the change is kept only in this tab.`); this.persistent = false; }
      }
      this.stamp = stamp; this.revision = revision; this.savedAt = savedAt; this.activity = activity;
      const s = await this.raw('read.snapshot', {});
      return { resp, snap: s.ok ? s.data : null };
    });
    if (this.multiTab) return navigator.locks.request(`cb-write-${this.scenario.name}`, body);
    return body();
  }

  // ------------------------------------------------------------------ events
  on(fn: (ev: LocalEventMsg) => void) { this.listeners.add(fn); return () => { this.listeners.delete(fn); }; }
  private emit(ev: LocalEventMsg) { for (const l of [...this.listeners]) { try { l(ev); } catch { /* listener bug */ } } }
  /** Deliver events in this tab and tell the other tabs (they reload from IndexedDB, then deliver to their own session). */
  publish(events: LocalEventMsg[]) {
    for (const ev of events) this.emit(ev);
    if (this.stamp) this.channel?.postMessage({ kind: 'changed', from: tabId, stamp: this.stamp, events } satisfies ChannelMsg);
  }
  private async onChannel(m: ChannelMsg) {
    if (!m || m.from === tabId) return;
    if (m.kind === 'reset') {
      await this.exclusive(() => this.loadSaved());
      this.emit({ type: 'resync', data: { reason: 'reset' }, audience: {}, broadcast: true });
      return;
    }
    if (m.kind === 'changed') {
      let reloaded = false;
      await this.exclusive(async () => { if (this.stamp !== m.stamp) { await this.loadSaved(); reloaded = true; } });
      if (reloaded && this.stamp !== m.stamp) {
        // the saved copy moved on again already; a later message will follow, but make sure the UI refetches
        this.emit({ type: 'resync', data: { reason: 'other_tab' }, audience: {}, broadcast: true });
      }
      for (const ev of m.events) this.emit(ev);
    }
  }
  /** On focus: if the saved copy changed while we were not listening, reload and ask the UI to refetch. */
  private async catchUp() {
    if (!this.persistent || !this.ready) return;
    let changed = false;
    await this.exclusive(async () => { if ((await this.savedStamp()) !== this.stamp) { await this.loadSaved(); changed = true; } });
    if (changed) this.emit({ type: 'resync', data: { reason: 'other_tab' }, audience: {}, broadcast: true });
  }

  // ------------------------------------------------------------------ reset / export
  async reset() {
    const body = () => this.exclusive(async () => {
      if (this.persistent) await clearState(this.scenario.name);
      this.notes = this.notes.filter((n) => !n.startsWith('The saved data'));
      await this.loadSaved();
    });
    if (this.multiTab) await navigator.locks.request(`cb-write-${this.scenario.name}`, body); else await body();
    this.channel?.postMessage({ kind: 'reset', from: tabId } satisfies ChannelMsg);
    this.emit({ type: 'resync', data: { reason: 'reset' }, audience: {}, broadcast: true });
  }
  currentFiles(): Promise<Record<string, string>> {
    return this.exclusive(async () => (await this.post<{ files: Record<string, string> }>({ type: 'files' })).files);
  }
  status(): LocalStatus {
    return { scenario: this.scenario, manifest: this.manifest, multiTab: this.multiTab, persistent: this.persistent, revision: this.revision, savedAt: this.savedAt, notes: [...this.notes] };
  }
}

let engineP: Promise<LocalEngine> | null = null;
export function getEngine(): Promise<LocalEngine> {
  if (!engineP) {
    const e = new LocalEngine();
    engineP = e.start().then(() => e);
    engineP.catch(() => { engineP = null; });
  }
  return engineP;
}
export type { LocalEngine };
