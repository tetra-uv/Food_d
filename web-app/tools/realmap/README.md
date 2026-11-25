# tools/realmap - build and validate the `delhi-real` scenario

Builds `data/scenarios/delhi-real/`: an imitation of all of NCT Delhi. Real main roads (OSM highway motorway..tertiary and
`_link`s, clipped to the NCT boundary), real place locations of the right kind (hotels, markets, bakeries, gurdwaras,
community centres, social facilities, ...); imitation organisation names and simulated capacities, donations and history.
The engine's six-CSV schema is unchanged; road geometry goes into `geo_overlay.json` (same shape as `delhi-geo`, with
`"minutesSource":"osm-speed-model"`). Python 3.12, standard library only; always run with `-I`.

| Command | What it does |
|---------|--------------|
| `python -I build_delhi_real.py` | Fetch missing Overpass responses (cache-first), build, write the scenario + SOURCES.md + SCENARIO_NOTES.md |
| `python -I build_delhi_real.py --offline` | Same from `cache/` only; output is byte-identical on every rerun (checked with SHA-256) |
| `python -I build_delhi_real.py --offline --tune` | Coarsening error for cluster spacings 200..1000 m -> `output/tune.json` (read by the report) |
| `python -I build_delhi_real.py --radius 400` | Build with another centre spacing (default 300 m) |
| `python -I validate_delhi_real.py` | Static + loader-invariant checks, native engine on a TEMP copy (`--self-test`, 100 x match.build, candidate parity, `net.route` per transaction), `tools/oracle/oracle.py`; writes `output/validation.json` |

After `validate_delhi_real.py`, rerun the build (`--offline`) so SCENARIO_NOTES.md section 6 shows the latest validation.

## Files
- `osm_fetch.py` - the four Overpass queries (`roads`, `places`, `localities`, `boundary`), User-Agent `cb-realmap/1.0`,
  one request at a time, exponential backoff on 502/429/timeouts, cache `cache/<name>-<sha1(query)[:12]>.json` + `.meta.json`
  (query, UTC fetch time, OSM data timestamp). Delete a cache file to refetch that query. Failures are never cached.
- `build_delhi_real.py` - pipeline (see its docstring); seed 20261125.
- `write_docs.py` - writes SOURCES.md and SCENARIO_NOTES.md from computed numbers only.
- `validate_delhi_real.py` - checks (never runs the engine on the pristine folder).
- `output/delhi_real_meta.json` - OSM element id, tags, lat/lng and snap distance of every source place, OSM node id and
  cluster size of every coarse node (the OSM ids are deliberately not in the displayed names).
- `output/build_stats.json`, `output/tune.json`, `output/validation.json` - numbers behind the report.

Data (c) OpenStreetMap contributors, ODbL 1.0. See `data/scenarios/delhi-real/SOURCES.md`.
