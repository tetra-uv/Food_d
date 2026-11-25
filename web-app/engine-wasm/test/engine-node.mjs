// engine-node.mjs: Node helpers around dist/cb_engine.mjs (used by shim.mjs, compare.mjs, memfs-write.mjs).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
export const DIST = path.resolve(HERE, '..', 'dist');

export async function loadEngine() {
  const { default: createCbEngine } = await import(pathToFileURL(path.join(DIST, 'cb_engine.mjs')).href);
  const m = await createCbEngine({ print: () => {}, printErr: (t) => process.stderr.write(t + '\n') });
  const dec = new TextDecoder('utf-8');

  // raw bytes of the NUL-terminated result at ptr (no decode/encode round trip)
  function bytesAt(ptr) {
    const heap = m.HEAPU8;
    let end = ptr;
    while (heap[end] !== 0) end++;
    return Buffer.from(heap.subarray(ptr, end));
  }
  return {
    module: m,
    FS: m.FS,
    initBytes(dir) {
      return bytesAt(m.ccall('cb_init', 'number', ['string'], [dir]));
    },
    init(dir) { return dec.decode(this.initBytes(dir)); },
    commandBytes(buf) {          // buf: Buffer/Uint8Array with one request line (any bytes)
      const ptr = m._malloc(buf.length + 1);
      m.HEAPU8.set(buf, ptr);
      m.HEAPU8[ptr + buf.length] = 0;
      try { return bytesAt(m._cb_command_n(ptr, buf.length)); } finally { m._free(ptr); }
    },
    command(line) { return dec.decode(this.commandBytes(Buffer.from(line, 'utf-8'))); },
    rev() { return m._cb_rev(); },
  };
}

// Copies the regular files of a host folder (flat) into a MEMFS folder.
export function copyDirToMemfs(FS, hostDir, memDir) {
  mkdirp(FS, memDir);
  const cache = new Map();
  if (fs.existsSync(hostDir)) {
    for (const name of fs.readdirSync(hostDir)) {
      const p = path.join(hostDir, name);
      if (!fs.statSync(p).isFile()) continue;
      const data = fs.readFileSync(p);
      FS.writeFile(memDir + '/' + name, data);
      cache.set(name, data);
    }
  }
  return cache;
}

export function mkdirp(FS, dir) {
  let cur = '';
  for (const part of dir.split('/').filter(Boolean)) {
    cur += '/' + part;
    try { FS.mkdir(cur); } catch (e) { /* exists */ }
  }
}

export function listMemfs(FS, memDir) {
  const out = new Map();
  for (const name of FS.readdir(memDir)) {
    if (name === '.' || name === '..') continue;
    const st = FS.stat(memDir + '/' + name);
    if (FS.isFile(st.mode)) out.set(name, Buffer.from(FS.readFile(memDir + '/' + name)));
  }
  return out;
}

// Writes MEMFS files that changed since `cache` back to hostDir (and removes deleted ones). Returns changed names.
export function syncMemfsToDir(FS, memDir, hostDir, cache) {
  const now = listMemfs(FS, memDir);
  const changed = [];
  for (const [name, data] of now) {
    const old = cache.get(name);
    if (!old || !old.equals(data)) {
      const tmp = path.join(hostDir, '.' + name + '.shim-tmp');
      fs.writeFileSync(tmp, data);
      fs.renameSync(tmp, path.join(hostDir, name));
      cache.set(name, data);
      changed.push(name);
    }
  }
  for (const name of [...cache.keys()]) {
    if (!now.has(name)) {
      try { fs.unlinkSync(path.join(hostDir, name)); } catch (e) { /* gone */ }
      cache.delete(name);
      changed.push('-' + name);
    }
  }
  return changed;
}
