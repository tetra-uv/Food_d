// Web Worker that hosts the C++ engine compiled to WebAssembly (web-app/engine-wasm, copied to /engine/ at build time).
// It is a dumb host: write CSVs into MEMFS /data, cb_init('/data'), answer cb_command(line). All coordination
// (IndexedDB, Web Locks, BroadcastChannel, roles) lives on the main thread in engineHost.ts / api.ts.
//
// Messages in:  {id, type:'load', engineUrl, wasmUrl, files:{name:text}}  -> {id, ok, ready}   (fresh engine instance)
//               {id, type:'cmd', line}                                     -> {id, ok, line}
//               {id, type:'files'}                                         -> {id, ok, files}   (all *.csv in /data)
// Every answer carries ok:false + error text when something threw.

// typed loosely so this file compiles with the DOM lib of the app tsconfig
const ctx = self as unknown as { postMessage(m: unknown): void; onmessage: ((e: MessageEvent) => void) | null };

interface EmFS {
  mkdir(p: string): void; readdir(p: string): string[]; unlink(p: string): void;
  writeFile(p: string, data: string): void; readFile(p: string, o: { encoding: 'utf8' }): string;
}
interface EmModule { FS: EmFS; cwrap: (name: string, ret: string | null, args: string[]) => (...a: unknown[]) => unknown }
type Factory = (opts?: Record<string, unknown>) => Promise<EmModule>;

let factory: Factory | null = null;
let compiled: WebAssembly.Module | null = null;
let compiledFrom = '';
let mod: EmModule | null = null;
let command: ((line: string) => string) | null = null;
const DATA = '/data';

async function getFactory(engineUrl: string): Promise<Factory> {
  if (factory) return factory;
  const m = (await import(/* @vite-ignore */ engineUrl)) as { default?: Factory; createCbEngine?: Factory };
  const f = m.createCbEngine ?? m.default;
  if (typeof f !== 'function') throw new Error('cb_engine.mjs does not export createCbEngine');
  factory = f;
  return f;
}

async function compile(wasmUrl: string): Promise<WebAssembly.Module | null> {
  if (!wasmUrl) return null;
  if (compiled && compiledFrom === wasmUrl) return compiled;
  const res = await fetch(wasmUrl);
  if (!res.ok) throw new Error(`Could not download the engine (${res.status})`);
  try {
    compiled = await WebAssembly.compileStreaming(res.clone());
  } catch {
    compiled = await WebAssembly.compile(await res.arrayBuffer()); // wrong MIME type on the host: compile from bytes
  }
  compiledFrom = wasmUrl;
  return compiled;
}

async function load(engineUrl: string, wasmUrl: string, files: Record<string, string>) {
  const f = await getFactory(engineUrl);
  const wasm = await compile(wasmUrl);
  const logs: string[] = [];
  const opts: Record<string, unknown> = {
    print: (t: string) => { if (logs.length < 50) logs.push(t); },
    printErr: (t: string) => { if (logs.length < 50) logs.push(t); },
    noInitialRun: true,
  };
  if (wasm) {
    // Re-use the compiled module: a reload (other tab changed data, reset) only instantiates, it does not recompile.
    opts.instantiateWasm = (imports: WebAssembly.Imports, done: (i: WebAssembly.Instance, m: WebAssembly.Module) => void) => {
      void WebAssembly.instantiate(wasm, imports).then((inst) => done(inst, wasm));
      return {};
    };
  }
  mod = null; command = null;
  const m = await f(opts);
  try { m.FS.mkdir(DATA); } catch { /* exists */ }
  for (const name of m.FS.readdir(DATA)) if (name !== '.' && name !== '..') { try { m.FS.unlink(`${DATA}/${name}`); } catch { /* dir */ } }
  for (const [name, text] of Object.entries(files)) m.FS.writeFile(`${DATA}/${name}`, text);
  const init = m.cwrap('cb_init', 'string', ['string']) as (d: string) => string;
  const ready = JSON.parse(init(DATA)) as Record<string, unknown>;
  mod = m;
  command = m.cwrap('cb_command', 'string', ['string']) as (l: string) => string;
  return { ready, logs };
}

function readFiles(): Record<string, string> {
  if (!mod) throw new Error('engine not loaded');
  const out: Record<string, string> = {};
  for (const name of mod.FS.readdir(DATA)) {
    if (!/\.csv$/i.test(name)) continue;
    out[name] = mod.FS.readFile(`${DATA}/${name}`, { encoding: 'utf8' });
  }
  return out;
}

ctx.onmessage = async (e: MessageEvent) => {
  const msg = e.data as { id: number; type: string; [k: string]: unknown };
  try {
    if (msg.type === 'load') {
      const r = await load(String(msg.engineUrl), String(msg.wasmUrl ?? ''), msg.files as Record<string, string>);
      ctx.postMessage({ id: msg.id, ok: true, ready: r.ready, logs: r.logs });
    } else if (msg.type === 'cmd') {
      if (!command) throw new Error('engine not loaded');
      ctx.postMessage({ id: msg.id, ok: true, line: command(String(msg.line)) });
    } else if (msg.type === 'files') {
      ctx.postMessage({ id: msg.id, ok: true, files: readFiles() });
    } else throw new Error(`unknown message ${msg.type}`);
  } catch (err) {
    ctx.postMessage({ id: msg.id, ok: false, error: err instanceof Error ? err.message : String(err) });
  }
};
