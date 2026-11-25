# Gateway (web-app/server)

Node (>= 22) + Fastify 5 (the only dependency), plain ESM JavaScript. It supervises ONE `cb_engine.exe`, serialises commands,
enforces roles server-side, exposes REST + SSE, merges `geo_overlay.json` into `/network`, and serves `..\web\dist` (with SPA
fallback) when it exists. It contains no business rule.

## Start
```
powershell -File web-app\scripts\start-server.ps1 [-Scenario lab] [-Port 8787] [-Bind 127.0.0.1]
```
Builds the engine if missing, runs `npm install` if needed, copies `data\scenarios\<name>` (ALL files) to `data\work\<name>` only if
that folder does not exist, sets `CB_DATA` to the work copy and starts `node src\index.js`. Reset a work copy with
`scripts\reset-data.ps1 -Scenario lab -Yes` (backs up first; stop the gateway before).

**Pristine data guard.** The engine writes CSVs in place, so the gateway refuses to start (clear error, exit 1) when `CB_DATA`
resolves inside `web-app\data\scenarios\` unless `CB_ALLOW_PRISTINE=1`. If `CB_DATA` is not set it defaults to
`data\work\lab` (copied once from the scenario, never overwritten).

Environment: `CB_DATA`, `CB_PORT` (8787), `CB_BIND` (127.0.0.1), `CB_ENGINE` (`..\engine\build\cb_engine.exe`), `CB_WEB_DIST`,
`CB_SECRET_FILE` (default `server\.secret`, 32 random bytes made at first run; never log or share it), `CB_HEARTBEAT_MS` (15000).

## Tests
```
cd web-app\server && npm test          # node:test, gateway on port 8788 with a temp copy of lab
powershell -File web-app\scripts\test-backend.ps1   # engine tests + gateway tests
```

## API implemented (base `/api/v1`, see server/src/app.js)
Public: `GET /health` `GET /meta` `GET /identities`. Session: `POST|GET|DELETE /session` (open mode: role + identityId, signed
httpOnly cookie `cb_session`). Coordinator: `GET /snapshot /queue /summary /stats/coordinator /donors /recipients /donations
/donations/:id /donations/:id/match /donations/:id/compare /network/field?source= /offers /transactions`, `POST /match/preview
/donations /donations/with-new-donor /donations/:id/close {confirm:true} /offers /offers/:id/withdraw {confirm:true} /donors
/recipients /admin/reload`, `PATCH /recipients/:id`. Donor: own donations/transactions/offers, `POST /donations` (own),
`POST /match/preview` (aggregate only). Recipient: own offers/transactions, `POST /offers/:id/accept`, `/decline {confirm:true}`,
`PATCH /recipients/:id` (own). Any session: `GET /network` (donor/recipient see only own org and offer counterparts),
`/network/route?from=&to=`, `GET /events` (SSE: heartbeat comment, `Last-Event-ID` replay from a 200-event ring, `resync`).
Events: offer.sent/accepted/declined/withdrawn, transaction.created, donation.created/updated/closed, recipient.updated/created,
donor.created, engine.status, resync.
Errors: `application/problem+json` `{type, code, title, detail, status, requestId, details?}`.

## Supervisor
One child process, FIFO queue, deadlines (reads 2 s, writes 10 s), restart with backoff 250 ms / 1 s / 4 s, DEGRADED after 5
crashes in 60 s (retries every 15 s), `FAILED_BOOT` on bad data (gateway stays up and `/health` shows the error). A read in flight at
a crash is repeated once; a write in flight answers `OUTCOME_UNKNOWN` (504).

## Known gaps
No Idempotency-Key, If-Match, rate limits, PIN mode, activity log, file watch, admin export/backup, OpenAPI file, compression,
TLS. Demo-grade access control only (anyone can pick any identity in open mode).
