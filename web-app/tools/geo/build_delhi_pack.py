#!/usr/bin/env python3
"""build_delhi_pack.py - build the `delhi-geo` scenario (real lat/lng + real driving minutes).

Python 3.12, standard library only. Every network response is cached on disk under tools/geo/cache/,
so a rerun is fully offline (only a cache miss triggers a request; <= 1 request/second, generic User-Agent).

Usage:
    python -I build_delhi_pack.py                       # build everything (offline when the cache is complete)
    python -I build_delhi_pack.py --offline             # refuse to touch the network (cache only)
    python -I build_delhi_pack.py --explore "Query"     # show raw Nominatim hits for a query and exit
    python -I build_delhi_pack.py --usable-factor 1.5   # force a usable_minutes factor instead of the automatic rule

Inputs : data/scenarios/lab/*.csv (read only), the cache, the NODE_SPECS table below.
Outputs: data/scenarios/delhi-geo/*  (six-CSV scenario minus offers.csv, geo_overlay.json, SOURCES.md,
         SCENARIO_NOTES.md, donations.lab-original.csv when donations were rescaled).
Nothing else is written. Lab files are never modified.
"""
from __future__ import annotations

import argparse
import ast
import hashlib
import json
import math
import re
import shutil
import statistics
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
PHASE4 = HERE.parent.parent
LAB = PHASE4 / "data" / "scenarios" / "lab"
OUT = PHASE4 / "data" / "scenarios" / "delhi-geo"
CACHE = HERE / "cache"
ORACLE = PHASE4 / "tools" / "oracle" / "oracle.py"
VALIDATOR = HERE / "validate_scenario.py"

USER_AGENT = "cb-geo-prep/1.0"
MIN_INTERVAL_S = 1.1  # <= 1 request/second to each public service (applied globally, stricter than needed)
NOMINATIM = "https://nominatim.openstreetmap.org/search"
OSRM = "https://router.project-osrm.org/route/v1/driving"
# Delhi NCT, generous box: (lng_min, lat_max, lng_max, lat_min) as Nominatim viewbox, also used as a sanity check
DELHI_BOX = (76.80, 28.90, 77.40, 28.40)

ROAD_FACTOR = 1.4      # fallback only: straight-line -> road distance
FALLBACK_KMH = 25.0    # fallback only: average driving speed
MAX_POINTS = 60        # simplified geometry cap per edge
DEGENERATE_SHARE = 0.40  # > 40% of open donations with zero candidates => rescale usable_minutes
NO_EXPIRY = 999999999  # "never expires" sentinel in donations.csv, kept as is

_last_request = 0.0
OFFLINE = False

# --------------------------------------------------------------------------------------------------
# Geocoding decisions. One row per graph node. (id, query, hit index, text the hit must contain,
# confidence, note). The hit index/`expect` pair was chosen after reading the raw Nominatim answers
# (see SCENARIO_NOTES.md); `expect` makes the build fail loudly if a later cache refresh reorders hits.
# "verified" = the OSM feature's own name matches the landmark named in the label. Everything else
# (locality centroid, nearest metro station / road as proxy, ambiguous name) is "approx".
# --------------------------------------------------------------------------------------------------
NODE_SPECS = [
    (0, "Karol Bagh", 1, "Karol Bagh, Pusa Road", "approx",
     "Karol Bagh metro station used as proxy: Nominatim did not return the market itself (a query for 'Karol Bagh Market' gave only a restaurant in 'new market, karol bagh', 1.5 km north of the station). Distance to the real market not verified."),
    (1, "Preet Vihar, Delhi", 0, "Preet Vihar Metro Station", "approx",
     "Preet Vihar metro station on Vikas Marg as proxy for the 'crossing'; no mapped junction named in the label."),
    (2, "Azadpur", 1, "Azadpur (Yellow Line)", "approx",
     "Azadpur metro station (Yellow Line) as proxy for Azadpur Chowk; 'Azadpur Chowk' itself returned no Nominatim hit."),
    (3, "ITO, New Delhi", 0, "ITO, Bahadur Shah Zafar Marg", "verified",
     "OSM station named 'ITO' on Bahadur Shah Zafar Marg; name matches the label's landmark. Exact junction centre not separately checked."),
    (4, "Nizamuddin Bridge, Delhi", 0, "Nizamuddin Road Bridge", "approx",
     "OSM bus stop named 'Nizamuddin Road Bridge' on Mahatma Gandhi Marg; the exact junction is not mapped as a named point."),
    (5, "Paharganj, Delhi", 0, "Paharganj, Karol Bagh", "approx",
     "Centroid of the Paharganj locality (OSM place=suburb); 'Paharganj Circle' not found as a named feature."),
    (6, "Connaught Place, Delhi", 0, "Connaught Place, New Delhi", "verified",
     "OSM place named 'Connaught Place' (suburb centroid); name matches the label. The Inner Circle itself was not separately located."),
    (7, "Shahdara, Delhi", 0, "Shahdara, Makki Sarai Railway Road", "approx",
     "Shahdara railway station as proxy for 'Shahdara Crossing'. Shahdara is a large area; the crossing itself is not pinned."),
    (8, "Rajouri Garden, Delhi", 0, "Rajouri Garden (Blue Line)", "approx",
     "Rajouri Garden metro station (Blue Line) as proxy for the junction; the Pink Line station is about 0.3 km away."),
    (9, "Dilshad Garden", 0, "Dilshad Garden, GT Road", "verified",
     "OSM station named 'Dilshad Garden' on GT Road, matching 'Dilshad Garden Metro Junction' by name."),
    (10, "Lodhi Colony", 1, "Lodhi Colony, Najaf Khan Road", "approx",
     "Lodhi Colony metro station as proxy for the crossing; the OSM locality centroid is 1.2 km away (north-east)."),
    (11, "Tilak Nagar, Delhi", 0, "Tilak Nagar, Tilak Nagar Round About", "approx",
     "Tilak Nagar metro station; its OSM address names a 'Tilak Nagar Round About' but the junction itself was not checked."),
    (12, "Ram Nagar, Delhi", 1, "Ram Nagar, Sadar Bazar", "approx",
     "UNSURE: at least three Ram Nagar localities exist in Delhi (Shahdara North, Sadar Bazar/Old Delhi, West Delhi). "
     "The Sadar Bazar one is chosen because it sits in the central cluster near Paharganj; the label gives no way to disambiguate."),
    (13, "Barakhamba Road, Delhi", 0, "Barakhamba Road, Connaught Place", "approx",
     "Centre point of one OSM segment of Barakhamba Road (three segments returned); the junction in the label is not pinned."),
    (14, "Vivek Vihar, Delhi", 1, "Vivek Vihar, Shahdara North", "approx",
     "Centroid of the Vivek Vihar locality (OSM place=suburb), about 1 km from the Dilshad Garden pin."),
    (15, "Jangpura Extension, Delhi", 0, "Jangpura Extension, Lajpat Nagar", "approx",
     "Centroid of the OSM residential landuse 'Jangpura Extension'; the junction is not pinned."),
    (16, "Mayur Vihar, Delhi", 0, "Mayur Vihar, Shahdara South", "approx",
     "Centroid of the Mayur Vihar locality (OSM place=suburb). Disconnected district in the engine graph."),
    (17, "Patparganj Industrial Area", 0, "Patparganj Industrial Area", "approx",
     "Judgement call: the plain 'Patparganj' hit lies only 0.28 km from the Mayur Vihar pin and would overlap on the map, "
     "so the centroid of 'Patparganj Industrial Area' (3.7 km away) is used. Disconnected district in the engine graph."),
    (18, "Najafgarh, Delhi", 0, "Najafgarh, Shivaji Marg", "approx",
     "Najafgarh station on Shivaji Marg as proxy for 'Najafgarh Remote Junction'. Disconnected district in the engine graph."),
    (19, "Dwarka, Delhi", 3, "Dwarka, Najafgarh, Delhi", "approx",
     "UNSURE: the label names no sector, so the centroid of the Dwarka sub-city (OSM place=suburb) is used, not a specific sector. "
     "Disconnected district in the engine graph."),
]


# ------------------------------------------------------------------------------------ cache + fetch
def cached_get(url: str) -> dict:
    """GET url (JSON), caching 200 responses on disk. Returns {"url","fetched_utc","status","body"}."""
    global _last_request
    CACHE.mkdir(parents=True, exist_ok=True)
    key = hashlib.sha1(url.encode("utf-8")).hexdigest()
    path = CACHE / f"{key}.json"
    if path.exists():
        return json.loads(path.read_text(encoding="utf-8"))
    if OFFLINE:
        raise RuntimeError(f"--offline and no cache entry for {url}")
    wait = MIN_INTERVAL_S - (time.monotonic() - _last_request)
    if wait > 0:
        time.sleep(wait)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=40) as resp:
            status, text = resp.status, resp.read().decode("utf-8")
    except urllib.error.HTTPError as exc:
        status, text = exc.code, exc.read().decode("utf-8", "replace")
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        _last_request = time.monotonic()
        raise ConnectionError(f"{type(exc).__name__}: {exc}") from exc
    _last_request = time.monotonic()
    try:
        body = json.loads(text)
    except json.JSONDecodeError:
        body = {"_raw": text[:2000]}
    entry = {"url": url, "fetched_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
             "status": status, "body": body}
    if status == 200:  # failures are not cached, so a rerun retries them
        path.write_text(json.dumps(entry, ensure_ascii=False, indent=1), encoding="utf-8")
    return entry


def nominatim(query: str, limit: int = 6) -> dict:
    params = {"q": query, "format": "jsonv2", "limit": str(limit), "countrycodes": "in",
              "viewbox": f"{DELHI_BOX[0]},{DELHI_BOX[1]},{DELHI_BOX[2]},{DELHI_BOX[3]}", "bounded": "1",
              "addressdetails": "0"}
    return cached_get(NOMINATIM + "?" + urllib.parse.urlencode(params))


def explore(query: str) -> None:
    entry = nominatim(query)
    print(f"# {query}  (fetched {entry['fetched_utc']}, HTTP {entry['status']})")
    hits = entry["body"] if isinstance(entry["body"], list) else []
    for i, hit in enumerate(hits):
        print(f"  [{i}] {hit['lat']},{hit['lon']}  {hit.get('category')}/{hit.get('type')}  "
              f"imp={hit.get('importance', 0):.3f}  {hit['display_name'][:110]}")


# ------------------------------------------------------------------------------------ geometry helpers
def haversine_km(lat1, lng1, lat2, lng2) -> float:
    r = 6371.0088
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = p2 - p1, math.radians(lng2 - lng1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def simplify(points: list[list[float]], max_points: int = MAX_POINTS) -> list[list[float]]:
    """Douglas-Peucker on a local metric projection; tolerance grows until <= max_points. Points are [lng, lat]."""
    if len(points) <= max_points:
        return [[round(p[0], 5), round(p[1], 5)] for p in points]
    lat0 = math.radians(sum(p[1] for p in points) / len(points))
    xy = [((p[0]) * 111320.0 * math.cos(lat0), p[1] * 110574.0) for p in points]

    def dp(tol: float) -> list[int]:
        keep = {0, len(points) - 1}
        stack = [(0, len(points) - 1)]
        while stack:
            a, b = stack.pop()
            ax, ay = xy[a]
            bx, by = xy[b]
            dx, dy = bx - ax, by - ay
            seg = math.hypot(dx, dy)
            best, idx = -1.0, -1
            for i in range(a + 1, b):
                px, py = xy[i]
                d = (math.hypot(px - ax, py - ay) if seg == 0
                     else abs(dy * (px - ax) - dx * (py - ay)) / seg)
                if d > best:
                    best, idx = d, i
            if idx != -1 and best > tol:
                keep.add(idx)
                stack += [(a, idx), (idx, b)]
        return sorted(keep)

    tol = 10.0
    kept = dp(tol)
    while len(kept) > max_points:
        tol *= 1.4
        kept = dp(tol)
    return [[round(points[i][0], 5), round(points[i][1], 5)] for i in kept]


# ------------------------------------------------------------------------------------ pipeline steps
def read_csv_rows(path: Path) -> tuple[list[str], list[list[str]]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    header = lines[0].split(",")
    return header, [ln.split(",") for ln in lines[1:] if ln.strip()]


def geocode_nodes(lab_nodes: list[list[str]]) -> list[dict]:
    labels = {int(r[0]): r[1] for r in lab_nodes}
    assert sorted(labels) == [s[0] for s in NODE_SPECS] == list(range(20)), "NODE_SPECS must cover node ids 0..19"
    out = []
    for nid, query, index, expect, conf, note in NODE_SPECS:
        entry = nominatim(query)
        hits = entry["body"] if isinstance(entry["body"], list) else []
        if index >= len(hits) or expect not in hits[index]["display_name"]:
            raise RuntimeError(f"node {nid} ({labels[nid]}): hit [{index}] for query {query!r} does not contain "
                               f"{expect!r}; re-explore with --explore and update NODE_SPECS")
        hit = hits[index]
        lat, lng = round(float(hit["lat"]), 6), round(float(hit["lon"]), 6)
        if not (DELHI_BOX[3] <= lat <= DELHI_BOX[1] and DELHI_BOX[0] <= lng <= DELHI_BOX[2]):
            raise RuntimeError(f"node {nid} outside the Delhi box: {lat},{lng}")
        out.append({"id": nid, "label": labels[nid], "lat": lat, "lng": lng, "confidence": conf,
                    "note": f"{note} [Nominatim {hit.get('category')}/{hit.get('type')} "
                            f"{hit['osm_type']}/{hit['osm_id']}, query \"{query}\"]",
                    "query": query, "osm": f"{hit['osm_type']}/{hit['osm_id']}",
                    "osm_class": f"{hit.get('category')}/{hit.get('type')}",
                    "display_name": hit["display_name"], "fetched_utc": entry["fetched_utc"]})
    return out


def route_edges(nodes: list[dict], lab_edges: list[list[str]]) -> list[dict]:
    by_id = {n["id"]: n for n in nodes}
    edges = []
    for u_s, v_s, lab_min in lab_edges:
        u, v = int(u_s), int(v_s)
        a, b = by_id[u], by_id[v]
        url = (f"{OSRM}/{a['lng']:.6f},{a['lat']:.6f};{b['lng']:.6f},{b['lat']:.6f}"
               f"?overview=full&geometries=geojson&steps=false&alternatives=false")
        entry, problem = None, None
        for attempt in range(2):
            try:
                entry = cached_get(url)
                if entry["status"] == 200 and entry["body"].get("code") == "Ok":
                    break
                problem = f"HTTP {entry['status']} code={entry['body'].get('code')}"
            except (ConnectionError, RuntimeError) as exc:
                problem = str(exc)
                entry = None
            if OFFLINE:
                break
            time.sleep(3)
        straight = haversine_km(a["lat"], a["lng"], b["lat"], b["lng"])
        rec = {"u": u, "v": v, "lab_minutes": int(lab_min), "straight_km": straight}
        if entry and entry["status"] == 200 and entry["body"].get("code") == "Ok":
            route = entry["body"]["routes"][0]
            rec.update(minutes=max(1, math.ceil(route["duration"] / 60.0)), source="osrm",
                       duration_s=route["duration"], distance_m=route["distance"],
                       geometry=simplify(route["geometry"]["coordinates"]),
                       raw_points=len(route["geometry"]["coordinates"]),
                       snap_m=[w["distance"] for w in entry["body"]["waypoints"]],
                       fetched_utc=entry["fetched_utc"])
        else:
            minutes = max(1, math.ceil(straight * ROAD_FACTOR / FALLBACK_KMH * 60.0))
            rec.update(minutes=minutes, source="estimate", problem=problem,
                       geometry=[[a["lng"], a["lat"]], [b["lng"], b["lat"]]], raw_points=2)
            print(f"WARNING edge {u}-{v}: OSRM unavailable ({problem}); using haversine estimate {minutes} min",
                  file=sys.stderr)
        edges.append(rec)
    return edges


def projection(nodes: list[dict]) -> tuple[float, float]:
    main = [n for n in nodes if n["id"] <= 15]  # main cluster = nodes 0..15 (16..19 are the isolated districts)
    return sum(n["lat"] for n in main) / len(main), sum(n["lng"] for n in main) / len(main)


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", newline="\n")


def fmt2(x: float) -> str:
    s = f"{round(x, 2):.2f}"
    return "0.00" if s == "-0.00" else s


def write_overlay(nodes: list[dict], edges: list[dict], minutes_source: str) -> None:
    last = max(n["fetched_utc"] for n in nodes)[:10]
    head = {"crs": "EPSG:4326", "scenario": "delhi-geo",
            "source": ("Node positions: OpenStreetMap data via Nominatim geocoding; edge minutes and route geometry: "
                       "OpenStreetMap data via the public OSRM demo server (driving profile). "
                       f"Fetched {last}. (c) OpenStreetMap contributors, ODbL. See SOURCES.md."),
            "disclaimer": "Approximate locations. Travel times are routing estimates, not live traffic.",
            "minutesSource": minutes_source}
    lines = ["{"]
    for k, v in head.items():
        lines.append(f"  {json.dumps(k)}: {json.dumps(v, ensure_ascii=False)},")
    lines.append('  "nodes": [')
    for i, n in enumerate(nodes):
        obj = {"id": n["id"], "lat": n["lat"], "lng": n["lng"], "confidence": n["confidence"], "note": n["note"]}
        lines.append("    " + json.dumps(obj, ensure_ascii=False, separators=(",", ":")) + ("," if i < len(nodes) - 1 else ""))
    lines.append("  ],")
    lines.append('  "edges": [')
    for i, e in enumerate(edges):
        obj = {"u": e["u"], "v": e["v"], "minutes": e["minutes"]}
        if e["source"] == "estimate":
            obj["minutesSource"] = "estimate"
        obj["geometry"] = e["geometry"]
        lines.append("    " + json.dumps(obj, separators=(",", ":")) + ("," if i < len(edges) - 1 else ""))
    lines.append("  ]")
    lines.append("}")
    write_text(OUT / "geo_overlay.json", "\n".join(lines) + "\n")
    json.loads((OUT / "geo_overlay.json").read_text(encoding="utf-8"))  # must be valid JSON


def write_nodes_edges(nodes: list[dict], edges: list[dict]) -> tuple[float, float]:
    lat0, lng0 = projection(nodes)
    rows = ["node_id,label,x_km,y_km"]
    for n in nodes:
        x = (n["lng"] - lng0) * 111.32 * math.cos(math.radians(lat0))
        y = -(n["lat"] - lat0) * 111.32  # y grows SOUTH so that north is up on screen
        n["x_km"], n["y_km"] = fmt2(x), fmt2(y)
        rows.append(f"{n['id']},{n['label']},{n['x_km']},{n['y_km']}")
    write_text(OUT / "graph_nodes.csv", "\n".join(rows) + "\n")
    erows = ["u,v,minutes"] + [f"{e['u']},{e['v']},{e['minutes']}" for e in edges]
    write_text(OUT / "graph_edges.csv", "\n".join(erows) + "\n")
    return lat0, lng0


def copy_lab_tables() -> None:
    for name in ("donors.csv", "recipients.csv", "donations.csv", "transactions.csv"):
        shutil.copyfile(LAB / name, OUT / name)  # byte-for-byte
    lo = OUT / "donations.lab-original.csv"
    if lo.exists():
        lo.unlink()  # re-created only when a rescale is applied


def run_oracle(folder: Path) -> dict:
    res = subprocess.run([sys.executable, "-I", str(ORACLE), str(folder)], capture_output=True, text=True,
                         encoding="utf-8", check=True)
    text = res.stdout
    def after(prefix: str):
        for ln in text.splitlines():
            if ln.startswith(prefix):
                return ast.literal_eval(ln[len(prefix):].strip())
        raise RuntimeError(f"oracle line not found: {prefix}")
    open_n = int(after("open donations:"))
    zero = after("open donations with ZERO candidates:")
    boundary = after("travel == usable (boundary-pass) cases:")
    dist = after("candidate-count distribution:")
    return {"open": open_n, "zero": zero, "boundary": boundary, "dist": dict(sorted(dist.items())), "raw": text}


def rescale_donations(factor: float) -> int:
    """Scale usable_minutes by factor (round to int, min 1, keep NO_EXPIRY). Returns number of changed rows."""
    src = (LAB / "donations.csv").read_text(encoding="utf-8").splitlines()
    out, changed = [src[0]], 0
    for ln in src[1:]:
        if not ln.strip():
            continue
        f = ln.split(",")
        old = int(f[5])
        if old != NO_EXPIRY:
            f[5] = str(max(1, round(old * factor)))
        changed += f[5] != str(old)
        out.append(",".join(f))
    shutil.copyfile(LAB / "donations.csv", OUT / "donations.lab-original.csv")
    write_text(OUT / "donations.csv", "\n".join(out) + "\n")
    return changed


def lab_main_minutes() -> list[int]:
    _, rows = read_csv_rows(LAB / "graph_edges.csv")
    return [int(r[2]) for r in rows if int(r[0]) <= 15]


# ------------------------------------------------------------------------------------ documents
def pct(n, d):
    return f"{100.0 * n / d:.0f}%" if d else "n/a"


def stats3(vals: list[int]) -> str:
    return f"{min(vals)} / {statistics.median(vals):g} / {max(vals)}"


def write_sources(nodes, edges) -> None:
    n_dates = sorted({n["fetched_utc"][:10] for n in nodes})
    e_dates = sorted({e["fetched_utc"][:10] for e in edges if "fetched_utc" in e})
    text = f"""# delhi-geo: sources and licences

Generated by `tools/geo/build_delhi_pack.py`. Dates are the UTC dates stored in the on-disk cache
(`tools/geo/cache/`), i.e. when each response was actually fetched. Nothing in this folder comes from a source that is not listed here.

| What | Source | Fetched | Licence / terms |
|------|--------|---------|-----------------|
| Latitude/longitude of the 20 nodes | Nominatim public geocoder, `https://nominatim.openstreetmap.org/search` (JSON, bounded to a Delhi viewbox), underlying data (c) OpenStreetMap contributors | {', '.join(n_dates)} | OSM data: ODbL 1.0 (https://www.openstreetmap.org/copyright). Service: Nominatim Usage Policy (https://operations.osmfoundation.org/policies/nominatim/): at most 1 request/second, identifying User-Agent, cache results. This build sends one query per place name, `cb-geo-prep/1.0` as User-Agent, and caches every answer. |
| Driving minutes and route geometry for the {len(edges)} edges | OSRM public demo server, `https://router.project-osrm.org/route/v1/driving/...` (driving profile, `overview=full&geometries=geojson`), routing on OpenStreetMap data | {', '.join(e_dates) or 'n/a'} | OSM data: ODbL 1.0. Best-effort demo server for light use, no SLA (https://github.com/Project-OSRM/osrm-backend/wiki/Api-usage-policy); routes need the OSRM source plus ODbL attribution. Car profile, no live traffic. One request per edge, throttled to under 1 request/second. |
| Row data (donors, recipients, donations, transactions) | Copied unchanged from the project's own synthetic `lab` scenario; fictional | n/a | Project data. `donations.csv` may be rescaled, see SCENARIO_NOTES.md |
| x_km / y_km | Derived by `build_delhi_pack.py` from the lat/lng above (local equirectangular projection) | n/a | Derived work of the OSM-based coordinates above |

## Attribution to show with any map or output built from this folder
"(c) OpenStreetMap contributors" (link to https://www.openstreetmap.org/copyright) and, for routes/minutes, "routing by OSRM".
Travel times are routing estimates for a typical car on the OSM road network, not live traffic, and the node positions are approximate.

## Policy pages read while building (2026-11-13, HTTP 200 each)
- Nominatim usage policy: no heavy use, absolute maximum 1 request per second, identifying User-Agent or Referer, cache results; small one-time
  bulk tasks are tolerated if single-threaded from one machine. This build is a one-time, single-thread, cached run (20 pinned queries plus a few exploratory ones).
- OSRM `Api-usage-policy` wiki page: the demo server is for light use, has no SLA and access can be withdrawn; it asks for a valid User-Agent and for clear
  display of the ODbL attribution and of the route source (OSRM), and points to the FOSSGIS terms of use for the current demo server (that page was not read).
  This build made one request per edge.
- OSMF tile usage policy: read to confirm the tile caveat below.
Policies can change without notice: re-check them before any reuse beyond this one-off cached build.

## Things deliberately not done
- No API keys, accounts or paid services were used. No project, user or donor data was sent to any service; only place-name strings
  and coordinate pairs were.
- Basemap tiles are not part of this folder. The public OSM tile servers have their own usage policy (https://operations.osmfoundation.org/policies/tiles/:
  visible attribution, valid User-Agent/Referer, no heavy use); the web app has to choose its own tile source.
"""
    write_text(OUT / "SOURCES.md", text)


def write_report(nodes, edges, lat0, lng0, before, after, factor, changed, cpp_out, minutes_source, auto_note) -> None:
    n_osrm = sum(e["source"] == "osrm" for e in edges)
    n_est = len(edges) - n_osrm
    main = [e for e in edges if e["u"] <= 15]
    isl = [e for e in edges if e["u"] > 15]
    lab_all = [e["lab_minutes"] for e in edges]
    new_all = [e["minutes"] for e in edges]
    lab_main = [e["lab_minutes"] for e in main]
    new_main = [e["minutes"] for e in main]
    rows_nodes = "\n".join(
        f"| {n['id']} | {n['label']} | {n['lat']:.6f} | {n['lng']:.6f} | {n['confidence']} | {n['osm']} ({n['osm_class']}) | "
        f"{n['note'].split(' [Nominatim')[0]} |" for n in nodes)
    rows_edges = "\n".join(
        f"| {e['u']}-{e['v']} | {e['lab_minutes']} | {e['minutes']} | {e['source']} | "
        f"{(e['distance_m'] / 1000):.1f} | {e['straight_km']:.1f} | {max(e['snap_m']):.0f} | {len(e['geometry'])}/{e['raw_points']} |"
        if e["source"] == "osrm" else
        f"| {e['u']}-{e['v']} | {e['lab_minutes']} | {e['minutes']} | estimate | n/a | {e['straight_km']:.1f} | n/a | 2/2 |"
        for e in edges)
    big_moves = ", ".join(f"{e['u']}-{e['v']} ({e['lab_minutes']} to {e['minutes']})" for e in edges
                          if abs(e["minutes"] - e["lab_minutes"]) >= 10) or "none"
    snap_bad = [f"{e['u']}-{e['v']}" for e in edges if e["source"] == "osrm" and max(e["snap_m"]) > 250]

    def dist_line(d):
        return ", ".join(f"{k}:{v}" for k, v in d.items())

    factor_txt = (f"Applied one factor **{factor}** to every `usable_minutes` except the `{NO_EXPIRY}` sentinel "
                  f"(round to int, min 1); {changed} rows changed. The untouched file is `donations.lab-original.csv`. {auto_note}"
                  if factor else "Not degenerate: `donations.csv` is byte-identical to the lab file; no rescale and no "
                                 "`donations.lab-original.csv`.")
    text = f"""# delhi-geo: scenario notes

## Method
1. Each of the 20 node labels was geocoded with Nominatim (OSM), bounded to a Delhi viewbox. Raw hits were read by hand; the chosen
   hit index and a required text fragment are pinned in `NODE_SPECS` of `tools/geo/build_delhi_pack.py`. Labels are generic
   ("... Crossing", "... Junction"), so most nodes sit on a nearby named OSM feature (metro station, locality centroid) rather than on
   the exact junction. Only names that match the label's landmark are marked `verified`.
2. For each of the 25 edge pairs of the lab scenario (same u,v rows, same order) one OSRM request (driving profile, u to v) gave duration
   and geometry. Minutes = ceil(duration / 60), minimum 1. Geometry is Douglas-Peucker simplified to at most {MAX_POINTS} points.
   The engine treats roads as undirected; only the u-to-v direction was routed, so one-way streets can make v-to-u slightly different.
3. `graph_nodes.csv`: same ids and labels; `x_km`,`y_km` derived from lat/lng with origin at the centroid of nodes 0-15
   (lat0={lat0:.6f}, lng0={lng0:.6f}): `x=(lng-lng0)*111.32*cos(lat0)`, `y=-(lat-lat0)*111.32`, 2 decimals, y grows south.
4. donors, recipients, transactions copied byte-for-byte from `lab`. Nodes 16-19 keep their lab edges only (16-17 and 18-19), so the four
   isolated districts stay disconnected from the main cluster.

## Edge sources
- minutesSource: **{minutes_source}**; edges from OSRM: **{n_osrm}**, edges from the haversine estimate: **{n_est}**
  (estimate = straight line x {ROAD_FACTOR} / {FALLBACK_KMH:g} km/h, ceil, min 1; such edges carry `"minutesSource":"estimate"` in the overlay).
- OSRM waypoint snapping: the largest distance between a pin and the road it was snapped to was {max(max(e['snap_m']) for e in edges if e['source']=='osrm'):.0f} m.
  Edges with a snap above 250 m: {', '.join(snap_bad) or 'none'}.

## Minutes: lab (simulated) vs delhi-geo (OSRM), min / median / max
| Set | lab | delhi-geo |
|-----|-----|-----------|
| all 25 edges | {stats3(lab_all)} | {stats3(new_all)} |
| main cluster (nodes 0-15), {len(main)} edges | {stats3(lab_main)} | {stats3(new_main)} |
| isolated districts, {len(isl)} edges | {stats3([e['lab_minutes'] for e in isl])} | {stats3([e['minutes'] for e in isl])} |

## Per-edge table
| edge | lab min | real min | source | route km | straight km | max snap m | geometry pts (kept/raw) |
|------|---------|----------|--------|----------|-------------|------------|-------------------------|
{rows_edges}

## Per-node confidence
`verified` = the OSM feature's own name matches the landmark named in the label. `approx` = proxy feature, locality centroid or
ambiguous name. Two nodes are genuinely unsure: **12 (Ram Nagar, ambiguous name)** and **19 (Dwarka, no sector given)**; node 17 is a judgement call.
| id | label | lat | lng | confidence | OSM object | note |
|----|-------|-----|-----|------------|------------|------|
{rows_nodes}

## Oracle statistics (web-app/tools/oracle/oracle.py), lab vs delhi-geo
| Metric | lab | delhi-geo (usable_minutes untouched) | delhi-geo (final donations.csv) |
|--------|-----|----------------------------------------|----------------------------------|
| open donations | {before['lab']['open']} | {before['geo']['open']} | {after['open']} |
| open donations with zero candidates | {len(before['lab']['zero'])} ({pct(len(before['lab']['zero']), before['lab']['open'])}) | {len(before['geo']['zero'])} ({pct(len(before['geo']['zero']), before['geo']['open'])}) | {len(after['zero'])} ({pct(len(after['zero']), after['open'])}) |
| zero-candidate donation ids | {', '.join(before['lab']['zero']) or '-'} | {', '.join(before['geo']['zero']) or '-'} | {', '.join(after['zero']) or '-'} |
| boundary cases (travel == usable) | {len(before['lab']['boundary'])} | {len(before['geo']['boundary'])} | {len(after['boundary'])} |
| candidate-count distribution (count:donations) | {dist_line(before['lab']['dist'])} | {dist_line(before['geo']['dist'])} | {dist_line(after['dist'])} |

Boundary cases in `lab` are deliberate; in delhi-geo they are whatever the real minutes happen to produce: {before['geo']['boundary']} before rescaling,
{after['boundary']} after. No hand-crafted boundary fixtures should be expected from this scenario.

## usable_minutes rescale (degenerate check, threshold {int(DEGENERATE_SHARE * 100)}% of open donations with zero candidates)
{factor_txt}

## C++ loader and validator
```
{cpp_out.strip()}
```

## Problems and limits
- Node positions are approximate; labels do not name exact points. See the per-node table; 12 and 19 are the least certain.
- Minutes are OSRM car-profile free-flow estimates (no traffic, no turn restrictions beyond what OSM carries), rounded up. Real peak-hour Delhi times will be longer.
- Edge endpoints are fixed to the lab graph's 25 pairs. Real roads would connect different pairs (e.g. 16 Mayur Vihar and 17 Patparganj are
  road-reachable from the main cluster, but are kept disconnected on purpose; the overlay geometry for 16-17 and 18-19 is still real).
  The lab's hand-made "slow direct edge vs fast multi-hop path" cases are not preserved: edges whose minutes moved by 10 or more versus lab
  (lab to real): {big_moves}. Do not expect the lab's Dijkstra-vs-direct demonstration to carry over.
- The engine's console banner still says "travel times are simulated minutes, not real road data" (console-app text, read-only here); the web app must not reuse that wording for this scenario.
- `transactions.csv` is copied from lab: its three `travel_minutes` (8, 15, 9) are the old simulated values and are not recomputed. The loader does not check them.
- The lab `usable_minutes` were tuned to simulated minutes, which is why the rescale check exists.
- Only the u-to-v direction of each road was routed.
"""
    write_text(OUT / "SCENARIO_NOTES.md", text)


# ------------------------------------------------------------------------------------ main
def main() -> int:
    global OFFLINE
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--explore", action="append", help="print raw Nominatim hits for a query and exit")
    ap.add_argument("--offline", action="store_true", help="never touch the network (cache only)")
    ap.add_argument("--usable-factor", type=float, help="force this usable_minutes factor (1.0 = no change)")
    args = ap.parse_args()
    OFFLINE = args.offline
    if args.explore:
        for q in args.explore:
            explore(q)
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    _, lab_nodes = read_csv_rows(LAB / "graph_nodes.csv")
    _, lab_edges = read_csv_rows(LAB / "graph_edges.csv")

    nodes = geocode_nodes(lab_nodes)
    edges = route_edges(nodes, lab_edges)
    sources = {e["source"] for e in edges}
    minutes_source = sources.pop() if len(sources) == 1 else "mixed"
    if minutes_source == "mixed":
        print("NOTE: some edges are OSRM and some are estimates; top-level minutesSource is 'mixed' "
              "(deviation from osrm|estimate)", file=sys.stderr)

    lat0, lng0 = write_nodes_edges(nodes, edges)
    write_overlay(nodes, edges, minutes_source)
    copy_lab_tables()

    before = {"lab": run_oracle(LAB), "geo": run_oracle(OUT)}
    geo = before["geo"]
    share = len(geo["zero"]) / geo["open"] if geo["open"] else 0.0
    factor, changed, auto_note = None, 0, ""
    if args.usable_factor is not None:
        if args.usable_factor != 1.0:
            factor = args.usable_factor
            auto_note = "Factor forced with --usable-factor."
    elif share > DEGENERATE_SHARE:
        # one documented factor: typical real/lab slowdown on the main cluster (median of edge minutes)
        factor = round(statistics.median(e["minutes"] for e in edges if e["u"] <= 15)
                       / statistics.median(lab_main_minutes()), 2)
        auto_note = (f"Automatic rule: factor = median(real main-cluster edge minutes) / median(lab main-cluster edge minutes) "
                     f"= {statistics.median(e['minutes'] for e in edges if e['u'] <= 15):g} / "
                     f"{statistics.median(lab_main_minutes()):g}, rounded to 2 decimals. "
                     f"Triggered because {len(geo['zero'])} of {geo['open']} open donations "
                     f"({pct(len(geo['zero']), geo['open'])}) had zero candidates.")
    if factor:
        changed = rescale_donations(factor)
    after = run_oracle(OUT)
    print(f"oracle lab:   open={before['lab']['open']} zero={len(before['lab']['zero'])}")
    print(f"oracle geo:   open={geo['open']} zero={len(geo['zero'])} ({share:.0%}) before rescale")
    print(f"usable factor: {factor or 'none'}  -> open={after['open']} zero={len(after['zero'])}")

    write_sources(nodes, edges)
    # validator last, so its output (incl. the C++ loader check on a temp copy) can go into the report
    write_report(nodes, edges, lat0, lng0, before, after, factor, changed, "(validator not run yet)", minutes_source, auto_note)
    if VALIDATOR.exists():
        res = subprocess.run([sys.executable, "-I", str(VALIDATOR), str(OUT), "--cpp"], capture_output=True,
                             text=True, encoding="utf-8")
        cpp_out = res.stdout + (res.stderr or "")
        write_report(nodes, edges, lat0, lng0, before, after, factor, changed, cpp_out, minutes_source, auto_note)
        print(cpp_out)
        return res.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
