# cb_engine WebAssembly build (web-app/engine-wasm)

The same frozen console app + desktop app backend and the same `engine/src` bridge code (Json, Engine, Writes, Session) as the
native `cb_engine.exe`, compiled with Emscripten so the engine runs inside the browser (static Vercel site).
Only the transport differs: `src/wasm_main.cpp` replaces `engine/src/main.cpp` (stdin/stdout loop).

## Build
```
powershell -File web-app\engine-wasm\build.ps1            # [-Opt -O2|-Os] [-Clean] [-Emsdk <folder>]
```
Needs the official Emscripten SDK (default folder `%USERPROFILE%\tools\emsdk`, or set `EMSDK`) (installed from the emsdk GitHub zip, then
`emsdk install latest` + `emsdk activate latest`; Emscripten 7.0.1). The script activates the env, runs
`emcmake cmake -G Ninja` + `cmake --build`, and prints raw/gzip sizes. Output: `dist/cb_engine.mjs` + `dist/cb_engine.wasm`.

Flags: `-O2`, `-fwasm-exceptions` (C++ exceptions ON: the backend throws `AppException`, the bridge throws `CmdError`),
`MODULARIZE EXPORT_ES6 EXPORT_NAME=createCbEngine`, `ENVIRONMENT=web,worker,node`, `ALLOW_MEMORY_GROWTH`,
`STACK_SIZE=2 MiB`, `FORCE_FILESYSTEM`. Runtime methods exported: `FS ccall cwrap UTF8ToString stringToUTF8
lengthBytesUTF8 HEAPU8`; functions `_cb_init _cb_command _cb_command_n _cb_rev _malloc _free`.

Sizes (-O2): `cb_engine.wasm` 492,048 B raw / 151,760 B gzip; `cb_engine.mjs` 62,529 B raw / 17,474 B gzip.

## API contract (the web codes against this)
| Function | Meaning |
|---|---|
| `const char* cb_init(const char* dataDir)` | Boots on a folder in the Emscripten FS (MEMFS). Returns the same first line as the native exe: `{"event":"ready",...}` or `{"event":"boot_failed","error":{code,message,details}}`. Call again to re-boot after the files change (previous engine is released; new `bootId`, `rev` 0). |
| `const char* cb_command(const char* line)` | One doc-04 request line in, the response line out (no newline). A trailing `\n`/`\r\n` is allowed. Empty line -> `""` (native ignores it). Over 1 MiB -> `LINE_TOO_LONG`. Not booted / after `sys.shutdown` -> `NOT_BOOTED`. |
| `const char* cb_command_n(const char* p, int n)` | Same with an explicit byte length (for raw/invalid UTF-8 tests). |
| `int cb_rev()` | Current `rev`, or -1 when no engine is running. |

**Ownership:** every returned pointer belongs to the module and is valid only until the next `cb_*` call; copy it at
once (`UTF8ToString`, or `ccall(..., 'string', ...)` which copies). Single threaded: call from one worker only.
Writes rewrite the CSVs inside MEMFS (`<file>.tmp`, unlink old, rename). Persist (e.g. to IndexedDB) after
`cb_command` returns, never during a call.

### Web Worker example
```js
// engine.worker.js  (type: module)
import createCbEngine from './cb_engine.mjs';
const m = await createCbEngine({ locateFile: (f) => new URL(f, import.meta.url).href });
const files = await loadCsvsSomehow();                 // { 'donors.csv': Uint8Array, ... } e.g. from IndexedDB / fetch
m.FS.mkdir('/data');
for (const [name, bytes] of Object.entries(files)) m.FS.writeFile('/data/' + name, bytes);
const init = m.cwrap('cb_init', 'string', ['string']);
const command = m.cwrap('cb_command', 'string', ['string']);
postMessage({ type: 'boot', event: JSON.parse(init('/data')) });
onmessage = (e) => {                                   // e.data = { v:1, id, cmd, args }
  const resp = JSON.parse(command(JSON.stringify(e.data)));
  if (resp.effects || resp.error?.details?.stateChanged) persist(m.FS.readdir('/data')
      .filter((f) => f.endsWith('.csv')).map((f) => [f, m.FS.readFile('/data/' + f)]));
  postMessage({ type: 'resp', resp });
};
```

## Tests and parity (clang 7.0.1/libc++ wasm32 vs the tested MinGW GCC 6.3 exe)
```
python -I web-app\engine\tests\test_engine.py                     # native: 139 PASS, 0 FAIL
$env:CB_ENGINE_CMD='["node","<repo>/web-app/engine-wasm/test/shim.mjs"]'
python -I web-app\engine\tests\test_engine.py                     # WASM:   139 PASS, 0 FAIL
node web-app\engine-wasm\test\compare.mjs lab                     # 117 responses, 0 mismatches
node web-app\engine-wasm\test\compare.mjs delhi-real              # 455 responses (2062 nodes), 0 mismatches
node web-app\engine-wasm\test\memfs-write.mjs                     # 21 PASS, 0 FAIL
```
- `test/shim.mjs`: same command line, exit codes and stdin/stdout protocol as `cb_engine.exe`; copies the folder into
  MEMFS and writes changed files back after every state change (so restart tests work).
- `test/compare.mjs`: temp copy of a scenario; ready, sys.hello, read.snapshot/network/queue, match.build and
  match.compare for every donation, net.field for up to 50 nodes; byte-for-byte except `bootId` and `dataDir`.
- `test/memfs-write.mjs`: offer.send 40 DON001->R013, offer.accept -> T004; checks the CSVs in MEMFS, temp+rename
  saves, re-`cb_init` reproduces the state (and ids continue: OFF002), files equal the native exe's output.

Performance (Node 26, this laptop): module load 10-12 ms. lab: copy+boot 19 ms, match.build p50 0.37 ms.
delhi-real (2062 nodes, 200 donations): copy+boot 77 ms, match.build p50 33 ms / p95 45 ms / max 51 ms
(native exe on the same data about 51 ms per match.build).

## Known gaps
- Line endings: the Windows exe rewrites CSVs with CRLF (text-mode streams), WASM writes LF; both load either.
  Content is otherwise identical.
- console app save is unlink-then-rename (not atomic). Harmless in single-threaded MEMFS; persist only between calls.
- `-fwasm-exceptions` needs WebAssembly exception handling (Chrome 95+, Firefox 100+, Safari 15.2+, Node 17+).
  For older browsers rebuild with `-DCB_EH=-fexceptions` (bigger, slower).
- Not tested in a real browser yet (only Node). `--self-test` of the exe is not exposed. `match.build` on delhi-real
  (~33 ms) should run in a worker, not on the UI thread.
