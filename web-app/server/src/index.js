// Entry point: node src/index.js   (env: CB_DATA, CB_PORT=8787, CB_BIND=127.0.0.1, CB_ENGINE, CB_ALLOW_PRISTINE)
import { loadConfig, loadSecret, ConfigError } from './config.js';
import { buildApp } from './app.js';

const log = (level, msg, extra) => {
  const line = JSON.stringify({ t: new Date().toISOString(), level, msg, ...extra });
  (level === 'error' || level === 'warn' ? process.stderr : process.stdout).write(line + '\n');
};

let config;
try {
  config = loadConfig();
} catch (e) {
  if (e instanceof ConfigError) {
    process.stderr.write(`\nCannot start: ${e.message}\n`);
    process.exit(1);
  }
  throw e;
}

const secret = loadSecret(config.secretPath);
const gateway = await buildApp({ config, secret, log });
const addr = await gateway.start();
log('info', 'gateway ready', { url: `http://${config.bind}:${addr.port}`, data: config.dataDir, engine: config.enginePath, engineState: gateway.sup.state });
if (gateway.sup.state === 'FAILED_BOOT') log('error', `the engine did not start: ${gateway.sup.bootError?.message}`);

let stopping = false;
const stop = async () => {
  if (stopping) return;
  stopping = true;
  log('info', 'shutting down');
  await gateway.close();
  process.exit(0);
};
process.on('SIGINT', stop);
process.on('SIGTERM', stop);
