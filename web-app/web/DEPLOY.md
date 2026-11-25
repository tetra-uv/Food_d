# Deploying the static site to Vercel

The Vercel build runs the app in **local mode** (`VITE_BACKEND=local`): no server. The C++ engine runs in the visitor's
browser as WebAssembly, data comes from the packaged scenario files, and changes are saved in the visitor's IndexedDB.

## How the upload works
The deploy runs from `web-app\web\` (Vercel "Root Directory" = this folder). Vercel only sees this folder, so the files
from `..\engine-wasm\dist\` and `..\data\scenarios\` must already be copied into `public\` **before** uploading:

- `public\engine\` (cb_engine.mjs + cb_engine.wasm), `public\scenarios\<name>\` (CSVs + geo_overlay.json), `public\cb-manifest.json`.

`npm run assets` (scripts\copy-assets.mjs) makes these copies. On Vercel the same script runs again inside
`npm run build:vercel`; the source folders are missing there, so it keeps the uploaded `public\` copies (warning only) and
rebuilds the manifest. `.vercelignore` keeps `node_modules`, `dist`, `mock` out of the upload. `vercel.json` sets the
build (`npm run build:vercel`, output `dist`), the SPA rewrite, `application/wasm` and cache headers (hashed `/assets/*`
and `/engine/*`, `/scenarios/*` are long-cached; the app always requests engine and scenario files with `?v=<content hash>`
from `cb-manifest.json`, which is `no-cache`).

## Steps (after `npx vercel login`)
```
cd web-app\web
npm ci
npm run assets                 # copies engine + scenarios into public\ ; check it lists the engine and delhi-real
npm run build:vercel           # local check: must finish without errors
npx vite preview --port 4173   # optional: open http://localhost:4173 and click through
npx vercel                     # first time: link/create the project. Answers: scope = your account,
                               #   link to existing = No, name = communitybridge, directory = ./ ,
                               #   override settings = No (vercel.json supplies framework Vite, build, output dist)
npx vercel --prod              # promote to production when the preview URL looks right
```
If the dashboard asks: Framework Preset **Vite**, Root Directory **web** (when importing the whole repo) or `./` (when
running the CLI from `web\`), Build Command `npm run build:vercel`, Output Directory `dist`, Install Command `npm ci`.
No environment variables or secrets are needed (`VITE_BACKEND=local` comes from `.env.static`).

## After any data or engine change
Re-run `npm run assets` before the next `npx vercel` (otherwise the old copies in `public\` are deployed). File hashes in
the manifest change automatically, so returning visitors download the new files; their saved IndexedDB copy of a scenario
is kept until they press **Reset data** (top bar).

## Notes
- Map tiles come from the public OpenStreetMap tile server (light/demo use only, attribution shown). Set `VITE_TILE_URL`
  at build time to use another tile source.
- Everything is demo-grade: anyone can pick any role; data lives only in each visitor's browser.
