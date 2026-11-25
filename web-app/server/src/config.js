// Configuration from environment variables (CB_DATA, CB_PORT, CB_BIND, CB_ENGINE, ...). The gateway refuses to run on
// the pristine scenario folders: the engine writes the CSV files in place.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
export const SERVER_DIR = path.resolve(here, '..');
export const WEB_ROOT = path.resolve(SERVER_DIR, '..');          // web-app
export const SCENARIOS_DIR = path.join(WEB_ROOT, 'data', 'scenarios');
export const WORK_DIR = path.join(WEB_ROOT, 'data', 'work');

export class ConfigError extends Error {}

function norm(p) {
  return path.resolve(p).replace(/[\\/]+$/, '').toLowerCase();
}

export function isInside(child, parent) {
  const c = norm(child), p = norm(parent);
  return c === p || c.startsWith(p + path.sep) || c.startsWith(p + '/');
}

/** Copies every file of a scenario folder (CSVs, geo_overlay.json, anything else) to a NEW work folder; never overwrites. */
export function copyScenario(from, to) {
  fs.mkdirSync(path.dirname(to), { recursive: true });
  fs.cpSync(from, to, { recursive: true, errorOnExist: true, force: false });
}

export function loadConfig(env = process.env) {
  const allowPristine = env.CB_ALLOW_PRISTINE === '1';
  let dataDir;
  if (env.CB_DATA) {
    dataDir = path.resolve(env.CB_DATA);
  } else {
    // default: the work copy of the lab scenario, made once (never overwritten) from the pristine one
    dataDir = path.join(WORK_DIR, 'lab');
    if (!fs.existsSync(dataDir)) {
      const pristine = path.join(SCENARIOS_DIR, 'lab');
      if (!fs.existsSync(pristine)) throw new ConfigError(`CB_DATA is not set and the sample scenario ${pristine} does not exist`);
      copyScenario(pristine, dataDir);
    }
  }
  if (isInside(dataDir, SCENARIOS_DIR) && !allowPristine) {
    throw new ConfigError(
      `CB_DATA (${dataDir}) is inside ${SCENARIOS_DIR}. Those folders are the pristine scenarios and the engine writes the CSV files in place. ` +
      'Use a work copy (scripts\\start-server.ps1 copies the scenario to data\\work\\<name> once), or set CB_ALLOW_PRISTINE=1 if you really mean it.');
  }
  if (!fs.existsSync(dataDir) || !fs.statSync(dataDir).isDirectory()) throw new ConfigError(`CB_DATA folder does not exist: ${dataDir}`);

  const enginePath = path.resolve(env.CB_ENGINE || path.join(WEB_ROOT, 'engine', 'build', 'cb_engine.exe'));
  if (!fs.existsSync(enginePath)) throw new ConfigError(`The engine was not found at ${enginePath}. Run scripts\\build-engine.ps1 or set CB_ENGINE.`);

  const port = env.CB_PORT === undefined ? 8787 : Number(env.CB_PORT);
  if (!Number.isInteger(port) || port < 0 || port > 65535) throw new ConfigError(`CB_PORT is not a valid port: ${env.CB_PORT}`);

  return {
    dataDir,
    enginePath,
    port,
    bind: env.CB_BIND || '127.0.0.1',
    webDist: path.resolve(env.CB_WEB_DIST || path.join(WEB_ROOT, 'web', 'dist')),
    secretPath: path.resolve(env.CB_SECRET_FILE || path.join(SERVER_DIR, '.secret')),
    heartbeatMs: Number(env.CB_HEARTBEAT_MS || 15000),
    ringSize: 200,
    scenario: path.basename(dataDir),
    allowPristine,
  };
}

/** The cookie-signing secret: made at the first run and kept in server/.secret (never logged). */
export function loadSecret(secretPath) {
  try {
    const text = fs.readFileSync(secretPath, 'utf8').trim();
    if (text.length >= 64) return Buffer.from(text, 'hex');
  } catch { /* first run */ }
  const secret = crypto.randomBytes(32);
  fs.mkdirSync(path.dirname(secretPath), { recursive: true });
  fs.writeFileSync(secretPath, secret.toString('hex') + '\n', { mode: 0o600 });
  return secret;
}
