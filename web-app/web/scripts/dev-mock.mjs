// Starts the dev-only mock gateway (8790) and Vite (5173) in one process, with the Vite proxy pointed at the mock.
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startMock } from '../mock/server.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
process.env.CB_API_TARGET = 'http://127.0.0.1:' + (process.env.MOCK_PORT || 8790);
await startMock();
const { createServer } = await import('vite');
const vite = await createServer({ root, configFile: path.join(root, 'vite.config.ts') });
await vite.listen();
vite.printUrls();
