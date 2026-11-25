# CommunityBridge web frontend (web-app\web)

Vite + React 18 + TypeScript (strict) + Leaflet + zustand. Plain CSS, calm light theme (2026-11-25 redesign: white/cream, warm ink, no blue; see "Look" below). The frontend renders what the
API returns; it never computes eligibility, ranking, allocation or capacity. Rendering math only (interpolating positions along a route,
the delivery marker along its route on the simulated clock).

## Run
```
cd web-app\web
npm install
npm run dev:mock        # mock gateway (8790) + Vite (5173), proxy -> mock.  Open http://localhost:5173
npm run dev:mock:geo    # same, with the geo overlay switched on (reload the page)
npm run dev             # Vite (5173), proxy /api -> real gateway http://127.0.0.1:8787
CB_API_TARGET=http://127.0.0.1:8791 npx vite --port 5174    # point at any other gateway
npm run typecheck       # tsc --noEmit
npm run build           # typecheck + production build into dist\ (the gateway serves ..\web\dist)
npm run fixtures        # regenerate mock\fixtures (prints the oracle checks, all must be true)
```
Start the real gateway with `powershell -File web-app\scripts\start-server.ps1` (see server\README.md). Switching between mock and
real changes only the proxy target (`vite.config.ts`, env `CB_API_TARGET`).

Mock extras (dev only): `POST /api/v1/__mock/geo {"on":true|false}` toggles the geo overlay for `/network`, `POST /api/v1/__mock/reset`
resets state, `POST /api/v1/__mock/engine {"state":"degraded"}` shows the degraded banner. Sign in as any role from the landing page;
the session is a cookie, so two browser profiles can play coordinator and recipient at once.

## Backends: http (gateway) or local (WebAssembly in the browser)
`VITE_BACKEND` picks who answers `/api/v1` (`src/backend.ts`); the UI does not care.
- `http` (default for `npm run dev` and `npm run build`): the Node gateway, as below.
- `local` (`npm run dev:local`, `npm run build:vercel`, file `.env.static`): `src/local/` runs the C++ engine compiled to
  WebAssembly in a Web Worker (`engine.worker.ts`), ports the gateway's routes, role checks, offer `context` and event payloads
  (`api.ts`), stores each scenario's CSVs in IndexedDB after every write and syncs tabs with Web Locks + BroadcastChannel
  (`engineHost.ts`). Session = role + identity in sessionStorage per tab (demo-grade). Top bar: scenario picker, Export (zip
  of the current CSVs), Reset data. Assets: `npm run assets` (`scripts/copy-assets.mjs`) copies `..\engine-wasm\dist` to
  `public/engine/` and `..\data\scenarios\{delhi-real,delhi-geo,lab}` (CSV + geo_overlay.json) to `public/scenarios/`,
  and writes `public/cb-manifest.json` (content hashes used as `?v=`). Deploy: see `DEPLOY.md`.

## Screens (workflow rework 2026-11-25: pipeline, one state per item, a detail page per item)
App shell: top bar (CommunityBridge, role navigation, role switcher, Settings: data set, simulated delivery clock, Reset
data, Back to start). Page template (`Page` in components/ui.tsx): title, one-line summary, content; map pages are a fluid
split (map fills the height, content column scrolls; map on top below 1024 px, collapsible below 768 px). Pages are lazy
chunks; Leaflet loads only on pages with a map (components/MapPanel.tsx).
- Coordinator: `#/c` Overview (live map: donors with food to assign, dashed lines for offers waiting, deliveries moving
  along their road route; "Needs you now" and "Offers waiting longest"). `#/c/donations[/offered|/done|/closed]` grouped by
  the engine's fields (Needs assignment = urgency queue with effectiveRemaining > 0). `#/c/donations/:id` = recommendations
  from match.build (plain reasons, Why not others, Assign dialog, Offers on this donation, Close donation).
  `#/c/offers[/history]`, `#/c/offers/:id` (timeline). `#/c/deliveries[/done]`, `#/c/deliveries/:txnId` (timeline, map).
- Recipient: `#/r` Incoming offers, `#/r/deliveries[/done]`, `#/r/profile` My details.
- Donor: `#/d` My donations (status timeline: waiting -> offered -> accepted -> on the way -> delivered, or closed with
  waste), `#/d/new` Add donation.
- Workflow guards (local API): one waiting offer per donation + recipient
  (DUPLICATE_PENDING_OFFER), Idempotency-Key per assign dialog, submit disabled while sending.
- Simulated delivery clock (lib/simclock.ts): accepted offer = engine transaction; the local activity log (src/local/activity.ts,
  saved with the CSVs in IndexedDB) stamps startAt; progress = elapsed / travelMinutes at 1 s = 1 min (or real time).
  Labelled as simulated, never fed back to the engine; older transactions show "Delivered earlier".

## Look
Tokens in `src/styles.css`: paper #FFFFFF, cream #FAF7F0, ink #1F1B16, secondary #6B645A, hairline #E7E1D5; meaning only:
green #2F6B3F (can take it / accepted), terracotta #A8432A (left out / destructive), ochre #8E6110 (urgent / limit).
Every status keeps a word. Leaflet CSS is vendored (`src/vendor/leaflet.css`) without its blue accents, and a small Vite
plugin (`vite.config.ts`) swaps Leaflet's blue JS defaults; tiles are greyscale at reduced opacity.

## Map
One component (`src/components/NetworkMap.tsx`), two modes.
- GEO when `/network` carries `geo`: OSM raster tiles (URL in `src/config.ts`, override with `VITE_TILE_URL`), light greyscale look by CSS filter,
  road geometry from `geo.edges[].geometry` else straight lines, fit on the main component, "Show all" button, badge
  "Approximate locations - routing estimates".
- SCHEMATIC otherwise (L.CRS.Simple, `xKm`/`yKm`, y flipped): islands are drawn as dashed insets next to the main cluster with
  "No road to the main network" and a break line labelled with the true distance. Badge "Simulation / sample data".
- Nodes with several organisations are one marker with a count badge and a popup list.
- The public OSM tile server is for light demo use only; use your own tile source for anything heavier.

Big real networks (more than 300 nodes or 500 edges, e.g. `delhi-real`): canvas renderer, all roads in one multi-polyline,
junctions not drawn, only nodes hosting organisations as canvas circle markers (radius by zoom, count badge from zoom 12.5),
roads drawn as one canvas multi-polyline; one canvas renderer for all vector layers; overlay items diffed by id.

## Structure
```
src/api/        types.ts (hand-written from docs 04/05), client.ts (fetch), events.ts (EventSource)
src/store.ts    zustand: session, role data, SSE handling, toasts, mutation helper
src/components/ NetworkMap (base layers + keyed overlay + moving deliveries), MapPanel (lazy), mapGeometry, studio (ranking, rules, rejected), ui (page template, tabs, timeline, dialogs, toasts, router)
src/screens/    Landing, coord/{Overview,Donations,Donation,Offers}, Deliveries, Recipient, Donor, common; src/lib/{derive,simclock,routes}
src/lib/        meta (food/reason/status semantics), format, motion
mock\           server.mjs (port 8790), gen-fixtures.mjs, fixtures\  (DEV ONLY)
scripts\dev-mock.mjs
```

## Mock vs real
The mock replays static fixtures generated from `data\scenarios\lab` (DON001: gates 26,23,17,15,13,11 -> 2, R013 then R004, net.field
from node 0 all asserted by `npm run fixtures`). It only keeps bookkeeping so flows work. Its previews ignore food, quantity and node and
vary only with usable minutes. It uses `data\scenarios\delhi-geo\geo_overlay.json` when that file exists, else the clearly labelled
fake `mock\fixtures\geo_overlay.TEMP-FAKE.json`.

## Known gaps
- Dark theme only (no Daybreak light theme, no Hindi), system fonts instead of self-hosted Inter / Space Grotesk.
- No landing attract-mode animation, no true-scale morph, no command palette, no zero-candidate flag in the queue, no Analytics /
  directories / ledger / activity / system pages, no coordinator "add donation" form.
- The reach (wavefront) animation, the time ruler and the what-if slider were removed in the workflow rework.
- Recipients see only quantity and travel minutes on offers until the gateway adds `context` (docs\CONTRACT_DEVIATIONS.md).
- Browser console shows one expected 401 for `GET /session` before sign-in.
- No automated tests; verified by typecheck, build and manual browser runs.
