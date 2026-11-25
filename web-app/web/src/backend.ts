// Which backend answers /api/v1: 'http' = the Node gateway (dev default, `npm run build`), 'local' = the C++ engine compiled
// to WebAssembly running in a Web Worker in this browser (`npm run build:vercel`, `npm run dev:local`). Set by VITE_BACKEND.
// UI code never branches on this except to show local-only controls (scenario picker, reset, export).
export type Backend = 'http' | 'local';
export const BACKEND: Backend = (import.meta.env.VITE_BACKEND as string | undefined) === 'local' ? 'local' : 'http';
export const IS_LOCAL = BACKEND === 'local';
