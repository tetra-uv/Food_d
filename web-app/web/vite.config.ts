import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';

// The only thing that changes between the mock and the real gateway is this proxy target.
// Default: real gateway on 8787. `npm run dev:mock` sets CB_API_TARGET to the mock (8790).
const target = process.env.CB_API_TARGET || 'http://127.0.0.1:8787';

// Leaflet ships a blue default path colour and a flag icon in its attribution prefix. The app always sets its own colours
// and its own prefix, so swap both defaults for the site ink at build time (keeps "no blue anywhere" true in the bundle).
const leafletNoBlue: Plugin = {
  name: 'leaflet-no-blue',
  transform(code, id) {
    if (!/[\\/]leaflet[\\/]dist[\\/]leaflet-src/.test(id)) return null;
    return { code: code.replace(/#3388ff/gi, '#1F1B16').replace(/#4C7BE1/gi, '#1F1B16').replace(/#0057B7/gi, '#1F1B16'), map: null };
  },
};

export default defineConfig({
  plugins: [react(), leafletNoBlue],
  server: {
    port: 5173,
    strictPort: false,
    proxy: {
      '/api': { target, changeOrigin: false },
    },
  },
  build: { sourcemap: false, chunkSizeWarningLimit: 700 },
});
