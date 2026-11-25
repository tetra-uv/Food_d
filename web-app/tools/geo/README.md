# tools/geo - build and validate the `delhi-geo` scenario

Creates `data/scenarios/delhi-geo/`: the same 20 nodes / 25 edges / donors / recipients as the synthetic `lab` scenario, but
nodes sit at real Delhi latitude/longitude and edge minutes are real OSRM driving minutes. The engine's six-CSV schema is untouched;
the real geometry goes into `geo_overlay.json`. `lab` is never modified.

Python 3.12, standard library only. Run from any folder; use `-I` (isolated mode).

| Command | What it does |
|---------|--------------|
| `python -I build_delhi_pack.py` | Geocode (Nominatim) + route (OSRM demo server) + write the whole scenario, SOURCES.md and SCENARIO_NOTES.md, run the oracle and the validator. Offline when `cache/` is complete. |
| `python -I build_delhi_pack.py --offline` | Same, but fails instead of making any network request. |
| `python -I build_delhi_pack.py --explore "Place, Delhi"` | Print the raw Nominatim hits for a query (cached). Use this before editing `NODE_SPECS`. |
| `python -I build_delhi_pack.py --usable-factor 1.5` | Force one `usable_minutes` factor (`1.0` = keep). Without the flag, a factor is applied only if more than 40% of open donations have zero candidates. |
| `python -I validate_scenario.py <folder> [--cpp]` | Static checks (headers, ids, references, node range, minutes >= 1, components, overlay consistency). `--cpp` copies the folder to a temp directory and runs `console-app/CommunityBridge.exe` on the copy with stdin `0`. |

## Behaviour you should know
- **Politeness:** one request per place name / edge, at most 1 per 1.1 s, `User-Agent: cb-geo-prep/1.0`, no keys, nothing about the user or the project is sent.
- **Cache:** every successful response is stored in `cache/<sha1-of-url>.json` with its fetch time. Delete a file to refresh just that response. Failures are not cached.
- **Fallback:** if OSRM cannot be reached (or answers non-Ok) for an edge, that edge gets a haversine estimate (x1.4 road factor / 25 km/h, ceil, min 1), a straight two-point geometry and `"minutesSource":"estimate"` in the overlay; a warning is printed and the report counts it. If all edges fall back the top-level `minutesSource` is `estimate`; a mix is written as `mixed` (and must be logged in `docs/CONTRACT_DEVIATIONS.md`).
- **Geocoding is a judgement table**, not a blind "first hit": `NODE_SPECS` in `build_delhi_pack.py` pins the query, the hit index, a text fragment the hit must contain (the build stops if a refreshed cache no longer matches), the confidence and a note. Only name matches are `verified`.
- **Projection:** `x_km=(lng-lng0)*111.32*cos(lat0)`, `y_km=-(lat-lat0)*111.32` (north is up when y grows south), origin = centroid of nodes 0-15, 2 decimals.
- **Re-running is deterministic** from the cache (no timestamps are written into outputs other than the cache fetch dates).
- Outputs are written only to `data/scenarios/delhi-geo/`. The C++ check always runs on a temp copy.

Attribution and licences: see `data/scenarios/delhi-geo/SOURCES.md` (OSM data is ODbL; show "(c) OpenStreetMap contributors").
