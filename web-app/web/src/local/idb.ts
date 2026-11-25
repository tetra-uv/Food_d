// Tiny IndexedDB wrapper: one record per scenario holding the engine's CSV files after the latest successful write.
// The whole record is written in one put, so files and stamp always match. A missing record means "pristine".

export interface StoredState {
  scenario: string;
  /** Changes on every saved write (random); tabs compare stamps to know whether their engine copy is current. */
  stamp: string;
  /** Counts saved writes since the last reset (for display only). */
  revision: number;
  savedAt: string;
  files: Record<string, string>;
  /** Wall-clock activity log (src/local/activity.ts); absent in records saved before it existed. */
  activity?: import('./activity').Activity;
}

const DB = 'communitybridge-local';
const STORE = 'scenarios';
let dbp: Promise<IDBDatabase> | null = null;

function open(): Promise<IDBDatabase> {
  if (dbp) return dbp;
  dbp = new Promise<IDBDatabase>((resolve, reject) => {
    if (typeof indexedDB === 'undefined') { reject(new Error('IndexedDB is not available')); return; }
    const req = indexedDB.open(DB, 1);
    req.onupgradeneeded = () => { req.result.createObjectStore(STORE, { keyPath: 'scenario' }); };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error ?? new Error('IndexedDB open failed'));
    req.onblocked = () => reject(new Error('IndexedDB is blocked by another tab'));
  });
  dbp.catch(() => { dbp = null; });
  return dbp;
}

function tx<T>(mode: IDBTransactionMode, fn: (s: IDBObjectStore) => IDBRequest<T> | null): Promise<T | undefined> {
  return open().then((db) => new Promise<T | undefined>((resolve, reject) => {
    const t = db.transaction(STORE, mode);
    const req = fn(t.objectStore(STORE));
    let out: T | undefined;
    if (req) req.onsuccess = () => { out = req.result; };
    t.oncomplete = () => resolve(out);
    t.onerror = () => reject(t.error ?? new Error('IndexedDB transaction failed'));
    t.onabort = () => reject(t.error ?? new Error('IndexedDB transaction aborted'));
  }));
}

export async function idbAvailable(): Promise<boolean> {
  try { await open(); return true; } catch { return false; }
}
export const loadState = (scenario: string) => tx<StoredState>('readonly', (s) => s.get(scenario) as IDBRequest<StoredState>);
export const saveState = (st: StoredState) => tx('readwrite', (s) => s.put(st)).then(() => undefined);
export const clearState = (scenario: string) => tx('readwrite', (s) => s.delete(scenario)).then(() => undefined);
