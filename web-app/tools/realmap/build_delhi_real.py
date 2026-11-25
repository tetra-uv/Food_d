#!/usr/bin/env python3
"""build_delhi_real.py - build scenario `delhi-real` (all of NCT Delhi, main roads, real OSM places, imitation orgs).

Python 3.12, standard library only, deterministic (fixed seed, no clock values in outputs), cache-first.

    python -I build_delhi_real.py              # fetch missing Overpass responses (cache), then build everything
    python -I build_delhi_real.py --offline    # cache only; reproduces byte-identical output
    python -I build_delhi_real.py --tune       # print the coarsening error for several cluster radii and exit

Pipeline: Overpass (roads, places, localities) -> fine junction graph with per-class speeds -> degree-2 chain
contraction -> largest connected component -> greedy cluster centres (minimum spacing RADIUS_M, major roads first)
+ network-Voronoi assignment -> coarse edges with integer minutes = round(fine shortest time between the two
representatives) (min 1) -> error measurement -> places sampled + snapped -> imitation names + seeded synthetic
attributes, donations and history -> scenario CSVs + geo_overlay.json + SOURCES.md + SCENARIO_NOTES.md.
Outputs: data/scenarios/delhi-real/ and tools/realmap/output/ (metadata incl. OSM ids of source places).
"""
from __future__ import annotations

import argparse
import heapq
import json
import math
import random
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))  # -I drops the script folder from sys.path
import osm_fetch  # noqa: E402

WEB = HERE.parent.parent
OUT = WEB / "data" / "scenarios" / "delhi-real"
META_OUT = HERE / "output"
SEED = 20261125
RADIUS_M = 300           # minimum spacing of cluster centres (tuned with --tune, see SCENARIO_NOTES.md)
SNAP_MAX_M = 1000
N_DONORS, N_RECIPIENTS, N_DONATIONS = 60, 140, 200

# Assumed average urban speeds incl. congestion (km/h). Not measured; see SCENARIO_NOTES.md.
SPEED_KMH = {"motorway": 45, "trunk": 35, "primary": 28, "secondary": 22, "tertiary": 18,
             "motorway_link": 30, "trunk_link": 25, "primary_link": 20, "secondary_link": 18, "tertiary_link": 15}
CLASS_RANK = {"motorway": 0, "trunk": 1, "primary": 2, "secondary": 3, "tertiary": 4,
              "motorway_link": 5, "trunk_link": 5, "primary_link": 6, "secondary_link": 7, "tertiary_link": 8}

R_EARTH = 6371008.8


def hav(lat1, lon1, lat2, lon2):
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = p2 - p1, math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * R_EARTH * math.asin(math.sqrt(a))


def clean_text(s: str) -> str:
    s = s.replace(",", " ").replace('"', "").replace("\r", " ").replace("\n", " ").replace(";", " ")
    return re.sub(r"\s+", " ", s).strip()


def ascii_name(tags: dict) -> str:
    for k in ("name:en", "name"):
        v = tags.get(k, "")
        if v and all(32 <= ord(c) < 127 for c in v):
            return clean_text(v)
    return ""


def pct(sorted_vals, q):
    if not sorted_vals:
        return float("nan")
    i = min(len(sorted_vals) - 1, max(0, int(round(q * (len(sorted_vals) - 1)))))
    return sorted_vals[i]


# ------------------------------------------------------------------------------------------- boundary
class Boundary:
    """Even-odd point-in-polygon over all member way segments of the NCT boundary relation (lat-banded)."""

    def __init__(self, rel_json: dict):
        segs = []
        for el in rel_json["elements"]:
            for m in el.get("members", []):
                if m.get("type") == "way" and m.get("role") in ("outer", "inner") and "geometry" in m:
                    g = m["geometry"]
                    for i in range(len(g) - 1):
                        segs.append((g[i]["lat"], g[i]["lon"], g[i + 1]["lat"], g[i + 1]["lon"]))
        self.n_segments = len(segs)
        self.lat_min = min(min(s[0], s[2]) for s in segs); self.lat_max = max(max(s[0], s[2]) for s in segs)
        self.nb = 400
        self.h = (self.lat_max - self.lat_min) / self.nb
        self.bands = [[] for _ in range(self.nb)]
        for s in segs:
            lo, hi = min(s[0], s[2]), max(s[0], s[2])
            for b in range(max(0, int((lo - self.lat_min) / self.h)), min(self.nb, int((hi - self.lat_min) / self.h) + 1)):
                self.bands[b].append(s)

    def inside(self, lat, lng):
        if not (self.lat_min <= lat <= self.lat_max):
            return False
        b = min(self.nb - 1, int((lat - self.lat_min) / self.h))
        c = False
        for a_lat, a_lng, b_lat, b_lng in self.bands[b]:
            if (a_lat > lat) != (b_lat > lat):
                x = a_lng + (lat - a_lat) * (b_lng - a_lng) / (b_lat - a_lat)
                if x > lng:
                    c = not c
        return c


# ------------------------------------------------------------------------------------------- fine graph
class Fine:
    """Contracted fine graph: nodes = OSM junctions of degree != 2, edges = chains of OSM way segments."""

    def __init__(self, roads: dict, border: Boundary):
        coords = {}
        ways = []
        outside = set()
        for el in roads["elements"]:
            if el["type"] == "node":
                coords[el["id"]] = (el["lat"], el["lon"])
                if not border.inside(el["lat"], el["lon"]):
                    outside.add(el["id"])
        self.outside_nodes = len(outside)
        self.clipped_ways = 0
        for el in roads["elements"]:
            if el["type"] == "way":
                hw = el.get("tags", {}).get("highway", "")
                if hw in SPEED_KMH and len(el.get("nodes", [])) >= 2:
                    nds = [n for n in el["nodes"] if n in coords]
                    if any(n in outside for n in nds):
                        self.clipped_ways += 1
                    run = []
                    for n in nds + [None]:
                        if n is not None and n not in outside:
                            run.append(n); continue
                        if len(run) >= 2:
                            ways.append((el["id"], hw, ascii_name(el.get("tags", {})), run))
                        run = []
        ways.sort(key=lambda w: (w[0], w[3][0]))
        self.coords = coords
        self.n_ways = len(ways)
        use = Counter()
        for _, _, _, nds in ways:
            for n in nds:
                use[n] += 1
        junction = set(n for n, c in use.items() if c >= 2)
        for _, _, _, nds in ways:
            junction.add(nds[0]); junction.add(nds[-1])
        # segments between consecutive junctions of a way
        segs = []  # (a, b, seconds, metres, cls, name, node list)
        km_by_class = Counter()
        for wid, hw, name, nds in ways:
            start = 0
            for i in range(1, len(nds)):
                if nds[i] in junction:
                    part = nds[start:i + 1]
                    m = sum(hav(*coords[part[j]], *coords[part[j + 1]]) for j in range(len(part) - 1))
                    km_by_class[hw] += m / 1000
                    if part[0] != part[-1] and m > 0:
                        segs.append((part[0], part[-1], m / (SPEED_KMH[hw] / 3.6), m, hw, name, part))
                    start = i
        self.raw_junctions = len(junction)
        self.raw_segments = len(segs)
        self.km_by_class = {k: round(v, 1) for k, v in sorted(km_by_class.items(), key=lambda kv: CLASS_RANK[kv[0]])}
        # largest connected component on the segment graph
        adj = defaultdict(list)
        for i, s in enumerate(segs):
            adj[s[0]].append(i); adj[s[1]].append(i)
        comp = {}
        sizes = []
        for s0 in sorted(adj):
            if s0 in comp:
                continue
            cid = len(sizes); comp[s0] = cid; stack = [s0]; cnt = 0
            while stack:
                x = stack.pop(); cnt += 1
                for si in adj[x]:
                    y = segs[si][1] if segs[si][0] == x else segs[si][0]
                    if y not in comp:
                        comp[y] = cid; stack.append(y)
            sizes.append(cnt)
        big = max(range(len(sizes)), key=lambda c: sizes[c])
        self.components = len(sizes)
        self.dropped_junctions = sum(sizes) - sizes[big]
        kept = [s for s in segs if comp[s[0]] == big]
        self.dropped_km = round(sum(s[3] for s in segs if comp[s[0]] != big) / 1000, 1)
        self.dropped_examples = sorted(Counter(s[5] or "(unnamed " + s[4] + ")" for s in segs
                                               if comp[s[0]] != big).items(), key=lambda kv: -kv[1])[:8]
        self.lcc_km = round(sum(s[3] for s in kept) / 1000, 1)
        # degree-2 contraction
        adj = defaultdict(list)
        for i, s in enumerate(kept):
            adj[s[0]].append(i); adj[s[1]].append(i)
        keep = {n: len(v) != 2 for n, v in adj.items()}
        if not any(keep.values()):
            keep[min(adj)] = True
        used = [False] * len(kept)
        chains = []  # (a, b, seconds, metres, best class rank, names, osm node path)
        for s0 in sorted(n for n in adj if keep[n]):
            for si in adj[s0]:
                if used[si]:
                    continue
                cur, secs, metres, rank, names, path = s0, 0.0, 0.0, 99, [], [s0]
                nxt_seg = si
                while True:
                    used[nxt_seg] = True
                    a, b, t, m, hw, name, part = kept[nxt_seg]
                    seq = part if a == cur else part[::-1]
                    cur = b if a == cur else a
                    secs += t; metres += m; rank = min(rank, CLASS_RANK[hw])
                    if name and name not in names:
                        names.append(name)
                    path.extend(seq[1:])
                    if keep[cur]:
                        break
                    others = [x for x in adj[cur] if not used[x]]
                    if not others:
                        break
                    nxt_seg = others[0]
                if cur != s0 and keep[cur]:
                    chains.append((s0, cur, secs, metres, rank, names, path))
        ids = sorted(set([c[0] for c in chains] + [c[1] for c in chains]))
        self.osm_ids = ids
        self.index = {o: i for i, o in enumerate(ids)}
        self.lat = [coords[o][0] for o in ids]
        self.lng = [coords[o][1] for o in ids]
        self.adj = [[] for _ in ids]
        self.chains = []
        self.node_rank = [99] * len(ids)
        self.node_names = [[] for _ in ids]
        for ci, (a, b, t, m, rank, names, path) in enumerate(chains):
            ia, ib = self.index[a], self.index[b]
            self.chains.append((ia, ib, t, m, rank, names, path))
            self.adj[ia].append((ib, t, ci)); self.adj[ib].append((ia, t, ci))
            for x, nm in ((ia, names[:1]), (ib, names[-1:])):
                self.node_rank[x] = min(self.node_rank[x], rank)
                for q in nm:
                    if q not in self.node_names[x]:
                        self.node_names[x].append(q)
        self.n = len(ids)

    def dijkstra(self, src, targets=None, sources=None):
        """Plain Dijkstra on seconds. With `targets`, stops when all are settled. With `sources` (list), multi-source
        and also returns the owning source of every node."""
        INF = float("inf")
        dist = {}
        prev = {}
        owner = {}
        pq = []
        if sources is not None:
            for s in sources:
                pq.append((0.0, s, s))
        else:
            pq.append((0.0, src, src))
        heapq.heapify(pq)
        left = set(targets) if targets is not None else None
        best = {}
        while pq:
            d, u, o = heapq.heappop(pq)
            if u in dist:
                continue
            dist[u] = d; owner[u] = o
            if left is not None:
                left.discard(u)
                if not left:
                    break
            for v, w, ci in self.adj[u]:
                nd = d + w
                if v not in dist and nd < best.get(v, INF):
                    best[v] = nd; prev[v] = (u, ci); heapq.heappush(pq, (nd, v, o))
        return dist, prev, owner


# ------------------------------------------------------------------------------------------- coarsening
def local_xy(lat, lng, lat0, lng0):
    """metres, x east, y north (for clustering)."""
    return ((lng - lng0) * math.cos(math.radians(lat0)) * 111320.0, (lat - lat0) * 111320.0)


def coarsen(F: Fine, radius_m: float):
    lat0 = round(sum(F.lat) / F.n, 4); lng0 = round(sum(F.lng) / F.n, 4)
    xy = [local_xy(F.lat[i], F.lng[i], lat0, lng0) for i in range(F.n)]
    order = sorted(range(F.n), key=lambda i: (F.node_rank[i], -len(F.adj[i]), F.osm_ids[i]))
    cell = radius_m
    grid = defaultdict(list)
    centres = []
    for i in order:
        x, y = xy[i]
        cx, cy = int(math.floor(x / cell)), int(math.floor(y / cell))
        ok = True
        for gx in (cx - 1, cx, cx + 1):
            for gy in (cy - 1, cy, cy + 1):
                for j in grid.get((gx, gy), ()):
                    if (xy[j][0] - x) ** 2 + (xy[j][1] - y) ** 2 < radius_m ** 2:
                        ok = False; break
                if not ok: break
            if not ok: break
        if ok:
            centres.append(i); grid[(cx, cy)].append(i)
    centres.sort()
    _, _, owner = F.dijkstra(None, sources=centres)
    cidx = {c: k for k, c in enumerate(centres)}
    nbrs = defaultdict(set)
    for a, b, t, m, rank, names, path in F.chains:
        ca, cb = cidx[owner[a]], cidx[owner[b]]
        if ca != cb:
            nbrs[ca].add(cb); nbrs[cb].add(ca)
    edges = {}
    geoms = {}
    for ca in range(len(centres)):
        targets = sorted(centres[cb] for cb in nbrs[ca] if cb > ca)
        if not targets:
            continue
        dist, prev, _ = F.dijkstra(centres[ca], targets=targets)
        for t in targets:
            cb = cidx[t]
            edges[(ca, cb)] = dist[t]
            # recover OSM node path of the fine shortest path
            seq = []
            v = t
            while v != centres[ca]:
                u, ci = prev[v]
                a, b, _, _, _, _, path = F.chains[ci]
                seq.append(path[::-1] if a == v else path)   # path from u to v
                v = u
            pts = [F.osm_ids[centres[ca]]]
            for part in reversed(seq):
                pts.extend(part[1:])
            geoms[(ca, cb)] = pts
    members = Counter(owner[i] for i in range(F.n))
    return {"centres": centres, "edges": edges, "geoms": geoms, "lat0": lat0, "lng0": lng0,
            "members": [members[c] for c in centres], "owner": owner, "cidx": cidx}


def coarse_minutes(seconds: float) -> int:
    return max(1, int(math.floor(seconds / 60.0 + 0.5)))


def int_dijkstra(adj, src):
    dist = {src: 0}; pq = [(0, src)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist.get(u, 1 << 60):
            continue
        for v, w in adj[u]:
            nd = d + w
            if nd < dist.get(v, 1 << 60):
                dist[v] = nd; heapq.heappush(pq, (nd, v))
    return dist


def measure_error(F: Fine, C: dict, rng: random.Random, n_src=50, n_tgt=10):
    k = len(C["centres"])
    adj = [[] for _ in range(k)]
    for (a, b), s in C["edges"].items():
        m = coarse_minutes(s); adj[a].append((b, m)); adj[b].append((a, m))
    srcs = rng.sample(range(k), n_src)
    rows = []
    for s in srcs:
        fd, _, _ = F.dijkstra(C["centres"][s])
        cd = int_dijkstra(adj, s)
        tg = rng.sample([x for x in range(k) if x != s], n_tgt)
        for t in tg:
            fine = fd[C["centres"][t]] / 60.0
            rows.append((fine, cd[t]))
    ab = sorted(abs(c - f) for f, c in rows)
    rel = sorted(abs(c - f) / f for f, c in rows if f > 0)
    signed = sorted((c - f) / f for f, c in rows if f > 0)
    short = sorted(abs(c - f) / f for f, c in rows if 0 < f < 15)
    return {"pairs": len(rows), "median_abs_min": round(pct(ab, 0.5), 2), "p95_abs_min": round(pct(ab, 0.95), 2),
            "median_rel": round(pct(rel, 0.5), 4), "p95_rel": round(pct(rel, 0.95), 4),
            "median_signed_rel": round(pct(signed, 0.5), 4),
            "short_pairs_lt15min": len(short), "short_median_rel": round(pct(short, 0.5), 4) if short else None,
            "fine_minutes_median": round(pct(sorted(f for f, _ in rows), 0.5), 1),
            "fine_minutes_max": round(max(f for f, _ in rows), 1)}


# ------------------------------------------------------------------------------------------- places
DONOR_LABEL = {"HOTEL": "Hotel", "CATERER": "Caterer", "MARKET": "Market", "BAKERY": "Bakery", "CANTEEN": "Canteen"}
RECIP_LABEL = {"SHELTER": "Shelter", "FOOD_BANK": "Food bank", "COMMUNITY_KITCHEN": "Community kitchen",
               "CARE_HOME": "Care home", "CHILDRENS_HOME": "Children's home", "COMMUNITY_CENTRE": "Community centre",
               "RELIEF_POINT": "Relief point"}
DONOR_QUOTA = {"HOTEL": 16, "MARKET": 12, "BAKERY": 12, "CATERER": 10, "CANTEEN": 10}
RECIP_QUOTA = {"SHELTER": 22, "FOOD_BANK": 8, "COMMUNITY_KITCHEN": 30, "CARE_HOME": 18, "CHILDRENS_HOME": 14,
               "COMMUNITY_CENTRE": 30, "RELIEF_POINT": 18}


def classify(t: dict):
    am, sf = t.get("amenity", ""), t.get("social_facility", "")
    sfor = t.get("social_facility:for", "")
    if am == "food_bank" or sf == "food_bank":
        return "R", "FOOD_BANK"
    if am == "place_of_worship" and t.get("religion") == "sikh":
        return "R", "COMMUNITY_KITCHEN"
    if am == "social_facility":
        if sf == "soup_kitchen":
            return "R", "COMMUNITY_KITCHEN"
        if re.search(r"child|orphan|juvenile|youth", sfor) or re.search(r"orphan|bal ashram|children", t.get("name", "").lower()):
            return "R", "CHILDRENS_HOME"
        if sf in ("shelter",) or re.search(r"homeless|rain basera|night shelter", (sfor + " " + t.get("name", "")).lower()):
            return "R", "SHELTER"
        if sf in ("nursing_home", "assisted_living", "group_home") or re.search(r"senior|elderly|old_age", sfor) \
                or re.search(r"old age|vridh|senior", t.get("name", "").lower()):
            return "R", "CARE_HOME"
        return "R", "RELIEF_POINT"
    if am == "community_centre":
        return "R", "COMMUNITY_CENTRE"
    if t.get("tourism") == "hotel":
        return "D", "HOTEL"
    if t.get("craft") == "caterer" or t.get("shop") == "catering":
        return "D", "CATERER"
    if am == "marketplace":
        return "D", "MARKET"
    if t.get("shop") == "bakery":
        return "D", "BAKERY"
    if am in ("canteen", "food_court"):
        return "D", "CANTEEN"
    if am == "restaurant":
        return "D", "RESTAURANT"
    return None, None


def spread_sample(pool, quota, rng, cell_deg=0.036):
    """Round-robin over ~4 km cells (seeded order) so the sample spreads across the city."""
    cells = defaultdict(list)
    for p in pool:
        cells[(int(math.floor(p["lat"] / cell_deg)), int(math.floor(p["lng"] / cell_deg)))].append(p)
    keys = sorted(cells)
    rng.shuffle(keys)
    for k in keys:
        rng.shuffle(cells[k])
    out = []
    while len(out) < quota and any(cells[k] for k in keys):
        for k in keys:
            if cells[k] and len(out) < quota:
                out.append(cells[k].pop())
    return out


# ------------------------------------------------------------------------------------------- rules (engine copy)
def candidates(don, donor_node, recips, dist):
    res = []
    for r in recips:
        if not r["accepting"] or don["food"] not in r["types"] or r["cap"] <= 0 or not r["pickup"]:
            continue
        t = dist.get(r["node"])
        if t is None or t > don["usable"]:
            continue
        acc = min(don["rem"], r["cap"])
        res.append((-acc, t, r["cap"] - acc, r["num"], r))
    res.sort(key=lambda x: x[:4])
    return res


def douglas_peucker(pts, eps):
    if len(pts) < 3:
        return pts
    lat0 = pts[0][1]
    k = math.cos(math.radians(lat0)) * 111320.0
    xy = [(p[0] * k, p[1] * 111320.0) for p in pts]
    keep = [False] * len(pts); keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        ax, ay = xy[a]; bx, by = xy[b]
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy
        best, bi = -1.0, -1
        for i in range(a + 1, b):
            px, py = xy[i]
            if L2 == 0:
                d = math.hypot(px - ax, py - ay)
            else:
                tt = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L2))
                d = math.hypot(px - ax - tt * dx, py - ay - tt * dy)
            if d > best:
                best, bi = d, i
        if best > eps:
            keep[bi] = True; stack.append((a, bi)); stack.append((bi, b))
    return [p for p, k2 in zip(pts, keep) if k2]


# ------------------------------------------------------------------------------------------- main
def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser()
    ap.add_argument("--offline", action="store_true")
    ap.add_argument("--tune", action="store_true")
    ap.add_argument("--radius", type=float, default=RADIUS_M)
    args = ap.parse_args()

    roads, roads_meta = osm_fetch.fetch("roads", args.offline)
    places, places_meta = osm_fetch.fetch("places", args.offline)
    locs, locs_meta = osm_fetch.fetch("localities", args.offline)
    bnd, bnd_meta = osm_fetch.fetch("boundary", args.offline)
    border = Boundary(bnd)
    F = Fine(roads, border)
    del roads
    print(f"[fine] ways={F.n_ways} junctions={F.raw_junctions} segments={F.raw_segments} components={F.components} "
          f"dropped_junctions={F.dropped_junctions} ({F.dropped_km} km); contracted nodes={F.n} chains={len(F.chains)} "
          f"outside_nodes={F.outside_nodes} clipped_ways={F.clipped_ways} boundary_segments={border.n_segments}")

    if args.tune:
        rows = []
        for r in (200, 250, 300, 350, 400, 500, 700, 1000):
            C = coarsen(F, r)
            e = measure_error(F, C, random.Random(SEED))
            rows.append({"radius": r, "nodes": len(C["centres"]), "edges": len(C["edges"]), **e})
            print(f"[tune] radius={r} nodes={len(C['centres'])} edges={len(C['edges'])} {e}", flush=True)
        META_OUT.mkdir(parents=True, exist_ok=True)
        (META_OUT / "tune.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")
        return 0

    C = coarsen(F, args.radius)
    err = measure_error(F, C, random.Random(SEED))
    K = len(C["centres"])
    print(f"[coarse] radius={args.radius} nodes={K} edges={len(C['edges'])} error={err}")
    lat0, lng0 = C["lat0"], C["lng0"]
    clat = [F.lat[c] for c in C["centres"]]
    clng = [F.lng[c] for c in C["centres"]]

    # localities
    loc = []
    for el in locs["elements"]:
        nm = ascii_name(el.get("tags", {}))
        if nm:
            loc.append((el["lat"], el["lon"], nm, el["tags"].get("place", ""), el["id"]))
    loc.sort(key=lambda x: x[4])

    def locality(lat, lng):
        best, bn = 1e18, "Delhi"
        for la, ln, nm, _, _ in loc:
            d = (la - lat) ** 2 + ((ln - lng) * math.cos(math.radians(lat))) ** 2
            if d < best:
                best, bn = d, nm
        return bn if math.sqrt(best) * 111320 < 3000 else "Delhi"

    # node labels
    labels = []
    for k, c in enumerate(C["centres"]):
        names = [n for n in F.node_names[c]][:2]
        if len(names) >= 2:
            lab = f"{names[0]} / {names[1]}"
        elif names:
            lab = f"{names[0]} - {locality(clat[k], clng[k])}"
        else:
            lab = locality(clat[k], clng[k])
        labels.append(lab[:60].strip())

    # coarse adjacency (integer minutes)
    edge_list = sorted(C["edges"].items())
    cadj = [[] for _ in range(K)]
    for (a, b), s in edge_list:
        m = coarse_minutes(s); cadj[a].append((b, m)); cadj[b].append((a, m))

    # node snapping grid
    ngrid = defaultdict(list)
    for k in range(K):
        ngrid[(int(clat[k] / 0.01), int(clng[k] / 0.01))].append(k)

    def snap(lat, lng):
        best, bk = 1e18, -1
        gi, gj = int(lat / 0.01), int(lng / 0.01)
        for a in range(gi - 1, gi + 2):
            for b in range(gj - 1, gj + 2):
                for k in ngrid.get((a, b), ()):
                    d = hav(lat, lng, clat[k], clng[k])
                    if d < best:
                        best, bk = d, k
        return bk, best

    # places
    pools = defaultdict(list)
    raw_counts = Counter()
    seen_osm = set()
    for el in sorted(places["elements"], key=lambda e: (e["type"], e["id"])):
        t = el.get("tags", {})
        role, typ = classify(t)
        if not role:
            continue
        lat = el.get("lat", el.get("center", {}).get("lat"))
        lng = el.get("lon", el.get("center", {}).get("lon"))
        if lat is None:
            continue
        key = f"{el['type']}/{el['id']}"
        if key in seen_osm:
            continue
        seen_osm.add(key)
        raw_counts[(role, typ)] += 1
        k, d = snap(lat, lng)
        if k < 0 or d > SNAP_MAX_M:
            raw_counts[(role, typ, "far")] += 1
            continue
        pools[(role, typ)].append({"osm": key, "lat": lat, "lng": lng, "node": k, "snap_m": round(d), "type": typ,
                                   "osm_tags": {x: t[x] for x in ("amenity", "tourism", "shop", "craft",
                                                                    "social_facility", "social_facility:for",
                                                                    "religion") if x in t}})
    rng = random.Random(SEED)
    notes = []

    def pick(role, quota_map, total):
        quota = dict(quota_map)
        avail = {t: len(pools[(role, t)]) for t in quota}
        spare = 0
        for t in quota:
            if avail[t] < quota[t]:
                spare += quota[t] - avail[t]
                notes.append(f"{role} {t}: only {avail[t]} usable OSM places (quota {quota[t]})")
                quota[t] = avail[t]
        while spare > 0:
            moved = False
            for t in sorted(quota, key=lambda t: -(avail[t] - quota[t])):
                if spare > 0 and avail[t] > quota[t]:
                    quota[t] += 1; spare -= 1; moved = True
            if not moved:
                break
        out = []
        for t in quota:
            out.extend(spread_sample(pools[(role, t)], quota[t], rng))
        return out, quota

    donors_sel, dq = pick("D", DONOR_QUOTA, N_DONORS)
    if len(donors_sel) < N_DONORS:
        extra = spread_sample(pools[("D", "RESTAURANT")], N_DONORS - len(donors_sel), rng)
        for p in extra:
            p["type"] = "CANTEEN"; p["note"] = "OSM amenity=restaurant used as a canteen-like source (shortfall)"
        donors_sel.extend(extra)
        notes.append(f"D: {len(extra)} restaurants used as CANTEEN-type donors to reach {N_DONORS}")
    recips_sel, rq = pick("R", RECIP_QUOTA, N_RECIPIENTS)

    # names
    name_count = Counter()

    def imitation(label, lat, lng):
        loc_name = locality(lat, lng)
        name_count[(label, loc_name)] += 1
        n = name_count[(label, loc_name)]
        base = f"{label} · "
        tail = f" #{n}"
        room = 60 - len(base.encode("utf-8")) - len(tail)
        return base + loc_name[:room].strip() + tail, loc_name

    donors = []
    for i, p in enumerate(donors_sel, start=1):
        nm, ln = imitation(DONOR_LABEL[p["type"]], p["lat"], p["lng"])
        donors.append({"id": f"DN{i:03d}", "name": nm, "type": p["type"], "node": p["node"], "src": p, "locality": ln})
    # synthetic recipient attributes
    CAP = {"SHELTER": (40, 200), "FOOD_BANK": (200, 800), "COMMUNITY_KITCHEN": (150, 600), "CARE_HOME": (20, 80),
           "CHILDRENS_HOME": (20, 100), "COMMUNITY_CENTRE": (50, 250), "RELIEF_POINT": (50, 300)}
    # accepted food types by recipient type (assumption, see SCENARIO_NOTES.md): gurdwara langars and food banks
    # take ingredients / dry goods rather than cooked meals; care and children's homes take meals and fresh food.
    TYPES = {"SHELTER": ["COOKED", "BAKERY"], "FOOD_BANK": ["PACKAGED", "PRODUCE"],
             "COMMUNITY_KITCHEN": ["PRODUCE", "PACKAGED"], "CARE_HOME": ["COOKED", "PRODUCE"],
             "CHILDRENS_HOME": ["COOKED", "BAKERY", "PACKAGED"], "COMMUNITY_CENTRE": ["COOKED", "PACKAGED"],
             "RELIEF_POINT": ["COOKED", "PACKAGED"]}
    FOODS = ["COOKED", "PACKAGED", "PRODUCE", "BAKERY"]
    arng = random.Random(SEED + 1)
    recips = []
    for i, p in enumerate(recips_sel, start=1):
        nm, ln = imitation(RECIP_LABEL[p["type"]], p["lat"], p["lng"])
        lo, hi = CAP[p["type"]]
        cap = int(round(arng.randint(lo, hi) / 5.0) * 5)
        if arng.random() < 0.03:
            cap = 0
        types = list(TYPES[p["type"]])
        x = arng.random()
        if x < 0.15 and len(types) > 1:
            types.pop(arng.randrange(len(types)))
        elif x < 0.30:
            extra = [f for f in FOODS if f not in types]
            if extra:
                types.append(arng.choice(extra))
        types = [f for f in FOODS if f in types]
        recips.append({"id": f"R{i:03d}", "num": i, "name": nm, "type": p["type"], "cap": cap, "types": types,
                       "accepting": arng.random() < 0.90, "pickup": arng.random() < 0.85, "node": p["node"],
                       "src": p, "locality": ln})

    # donations
    FOOD_BY_DONOR = {"HOTEL": [("COOKED", .6), ("BAKERY", .2), ("PACKAGED", .2)],
                     "CATERER": [("COOKED", .85), ("PACKAGED", .15)],
                     "MARKET": [("PRODUCE", .8), ("PACKAGED", .2)],
                     "BAKERY": [("BAKERY", .9), ("PACKAGED", .1)],
                     "CANTEEN": [("COOKED", .7), ("PACKAGED", .2), ("BAKERY", .1)]}
    QTY = {"COOKED": (20, 150), "BAKERY": (20, 200), "PRODUCE": (20, 300), "PACKAGED": (20, 400)}
    # usable_minutes variants, tried in order; the first whose zero-candidate share of open donations is within
    # ZERO_BAND is used (all variants and their shares are reported). V0 = the ranges suggested in the brief.
    USABLE_VARIANTS = [
        ("V0 uniform", "uniform", {"COOKED": (30, 180), "BAKERY": (120, 600), "PRODUCE": (240, 1440), "PACKAGED": (1440, 10080)}),
        ("V1 log-uniform", "log", {"COOKED": (15, 180), "BAKERY": (60, 600), "PRODUCE": (240, 1440), "PACKAGED": (1440, 10080)}),
        ("V2 log-uniform", "log", {"COOKED": (10, 120), "BAKERY": (30, 480), "PRODUCE": (120, 1440), "PACKAGED": (1440, 10080)}),
        ("V3 log-uniform", "log", {"COOKED": (6, 90), "BAKERY": (15, 360), "PRODUCE": (60, 1440), "PACKAGED": (720, 10080)}),
        ("V4 log-uniform", "log", {"COOKED": (4, 60), "BAKERY": (10, 240), "PRODUCE": (30, 1440), "PACKAGED": (480, 10080)}),
    ]
    ZERO_BAND = (0.10, 0.30)
    drng = random.Random(SEED + 2)
    donor_seq = list(range(len(donors))) + [drng.randrange(len(donors)) for _ in range(N_DONATIONS - len(donors))]
    drng.shuffle(donor_seq)
    base = []
    for i, di in enumerate(donor_seq, start=1):
        d = donors[di]
        x, acc = drng.random(), 0.0
        food = FOOD_BY_DONOR[d["type"]][-1][0]
        for f, w in FOOD_BY_DONOR[d["type"]]:
            acc += w
            if x < acc:
                food = f; break
        q = int(round(drng.randint(*QTY[food]) / 5.0) * 5)
        if drng.random() < 0.03:
            q *= 5
        base.append({"id": f"DON{i:03d}", "donor": d, "food": food, "orig": q, "u01": drng.random()})
    initial_caps = [r["cap"] for r in recips]

    dist_cache = {}

    def dist_from(node):
        if node not in dist_cache:
            dist_cache[node] = int_dijkstra(cadj, node)
        return dist_cache[node]

    def realise(kind, ranges):
        for r, c in zip(recips, initial_caps):
            r["cap"] = c
        dons = []
        for b in base:
            lo, hi = ranges[b["food"]]
            u = lo + b["u01"] * (hi - lo) if kind == "uniform" else lo * (hi / lo) ** b["u01"]
            u = int(round(u))
            u = int(round(u / 5.0) * 5) if u >= 60 else u
            dons.append({"id": b["id"], "donor": b["donor"], "food": b["food"], "orig": b["orig"], "rem": b["orig"],
                         "usable": max(1, u), "status": "PENDING"})
        # history: a few PARTIAL / COMPLETED / CLOSED with consistent transactions (engine ranking + engine travel)
        hrng = random.Random(SEED + 3)
        plan = ["PARTIAL"] * 12 + ["COMPLETED"] * 10 + ["CLOSED_TX"] * 5 + ["CLOSED_NOTX"] * 3
        order = list(range(len(dons)))
        hrng.shuffle(order)
        txs = []
        used = set()
        for want in plan:
            for idx in order:
                if idx in used:
                    continue
                dn = dons[idx]
                dist = dist_from(dn["donor"]["node"])
                cs = candidates(dn, dn["donor"]["node"], recips, dist)
                if want == "CLOSED_NOTX":
                    used.add(idx); dn["status"] = "CLOSED"; break
                if not cs:
                    continue
                if want == "COMPLETED":
                    if sum(c[4]["cap"] for c in cs) < dn["rem"]:
                        continue
                    while dn["rem"] > 0:
                        cs = candidates(dn, dn["donor"]["node"], recips, dist)
                        top = cs[0]; r = top[4]; q = -top[0]
                        txs.append((dn, r, q, top[1])); r["cap"] -= q; dn["rem"] -= q
                    dn["status"] = "COMPLETED"
                else:
                    top = cs[0]; r = top[4]
                    q = max(1, min(r["cap"], int(dn["orig"] * hrng.uniform(0.3, 0.7))))
                    if q >= dn["rem"]:
                        continue
                    txs.append((dn, r, q, top[1])); r["cap"] -= q; dn["rem"] -= q
                    dn["status"] = "PARTIAL" if want == "PARTIAL" else "CLOSED"
                used.add(idx)
                break
        txs.sort(key=lambda t: t[0]["id"])
        # stats (same six rules + ranking as the engine / tools/oracle)
        open_d = [d for d in dons if d["status"] in ("PENDING", "PARTIAL")]
        sizes, zero, boundary = [], [], []
        reasons = Counter()
        for d in open_d:
            dist = dist_from(d["donor"]["node"])
            cs = candidates(d, d["donor"]["node"], recips, dist)
            sizes.append(len(cs))
            if not cs:
                zero.append(d["id"])
            for c in cs:
                if c[1] == d["usable"]:
                    boundary.append((d["id"], c[4]["id"], d["usable"]))
            for r in recips:
                t = dist.get(r["node"])
                if not r["accepting"]: reasons["NOT_ACCEPTING"] += 1
                elif d["food"] not in r["types"]: reasons["FOOD_INCOMPATIBLE"] += 1
                elif r["cap"] <= 0: reasons["NO_CAPACITY"] += 1
                elif not r["pickup"]: reasons["NO_PICKUP"] += 1
                elif t is None: reasons["UNREACHABLE"] += 1
                elif t > d["usable"]: reasons["TOO_FAR"] += 1
                else: reasons["OK"] += 1
        zs = set(zero)
        st = {"open": len(open_d), "zero": len(zero), "zero_share": round(len(zero) / max(1, len(open_d)), 3),
              "zero_by_food": dict(sorted(Counter(d["food"] for d in open_d if d["id"] in zs).items())),
              "open_by_food": dict(sorted(Counter(d["food"] for d in open_d).items())),
              "cand_dist": dict(sorted(Counter(sizes).items())), "boundary": boundary, "reasons": dict(reasons),
              "cand_median": pct(sorted(sizes), 0.5), "cand_max": max(sizes) if sizes else 0,
              "cand_buckets": dict(Counter("0" if s == 0 else "1-3" if s <= 3 else "4-10" if s <= 10 else
                                           "11-30" if s <= 30 else "31+" for s in sizes))}
        return dons, txs, st

    variant_log = []
    chosen = None
    for vname, kind, ranges in USABLE_VARIANTS:
        dons, txs, stats = realise(kind, ranges)
        variant_log.append({"variant": vname, "kind": kind, "ranges": ranges, "zero_share": stats["zero_share"],
                            "cand_median": stats["cand_median"]})
        print(f"[usable] {vname} zero_share={stats['zero_share']} median_cands={stats['cand_median']}")
        if ZERO_BAND[0] <= stats["zero_share"] <= ZERO_BAND[1]:
            chosen = (vname, kind, ranges); break
    if chosen is None:
        chosen = USABLE_VARIANTS[-1]
        notes.append("no usable_minutes variant reached the zero-candidate band; the last one is used")
    USABLE = {"variant": chosen[0], "kind": chosen[1], "ranges": chosen[2], "tried": variant_log}
    print(f"[orgs] donors={len(donors)} recipients={len(recips)} donations={len(dons)} transactions={len(txs)}")
    print(f"[oracle-rules] open={stats['open']} zero={stats['zero']} share={stats['zero_share']} "
          f"by_food={stats['zero_by_food']} median_cands={stats['cand_median']} boundary={len(stats['boundary'])}")

    # ---------------------------------------------------------------- write scenario
    OUT.mkdir(parents=True, exist_ok=True)
    META_OUT.mkdir(parents=True, exist_ok=True)

    def wcsv(name, header, rows):
        with open(OUT / name, "w", encoding="utf-8", newline="") as fh:
            fh.write(header + "\n")
            for r in rows:
                fh.write(",".join(str(x) for x in r) + "\n")

    coslat = math.cos(math.radians(lat0))
    wcsv("graph_nodes.csv", "node_id,label,x_km,y_km",
         [(k, labels[k], f"{(clng[k] - lng0) * 111.32 * coslat:.3f}", f"{-(clat[k] - lat0) * 111.32:.3f}")
          for k in range(K)])
    wcsv("graph_edges.csv", "u,v,minutes", [(a, b, coarse_minutes(s)) for (a, b), s in edge_list])
    wcsv("donors.csv", "donor_id,name,type,node_id", [(d["id"], d["name"], d["type"], d["node"]) for d in donors])
    wcsv("recipients.csv", "recipient_id,name,type,available_capacity,accepted_types,accepting,can_pickup,node_id",
         [(r["id"], r["name"], r["type"], r["cap"], ";".join(r["types"]), int(r["accepting"]), int(r["pickup"]),
           r["node"]) for r in recips])
    wcsv("donations.csv", "donation_id,donor_id,food_type,original_quantity,remaining_quantity,usable_minutes,status",
         [(d["id"], d["donor"]["id"], d["food"], d["orig"], d["rem"], d["usable"], d["status"]) for d in dons])
    wcsv("transactions.csv", "transaction_id,donation_id,recipient_id,quantity,travel_minutes",
         [(f"T{i:03d}", t[0]["id"], t[1]["id"], t[2], t[3]) for i, t in enumerate(txs, start=1)])

    # geo overlay
    coords = F.coords
    eps_used = Counter()
    ov_nodes = []
    for k, c in enumerate(C["centres"]):
        mem = C["members"][k]
        ov_nodes.append({"id": k, "lat": round(clat[k], 6), "lng": round(clng[k], 6),
                         "confidence": "verified" if mem == 1 else "approx",
                         "note": f"OSM junction node/{F.osm_ids[c]}" + ("" if mem == 1 else
                                 f"; represents a cluster of {mem} junctions (spacing >= {int(args.radius)} m)")})
    ov_edges = []
    for (a, b), s in edge_list:
        pts = [(coords[o][1], coords[o][0]) for o in C["geoms"][(a, b)]]
        eps = 10.0
        g = douglas_peucker(pts, eps)
        while len(g) > 60:
            eps *= 1.5; g = douglas_peucker(pts, eps)
        eps_used[eps] += 1
        ov_edges.append({"u": a, "v": b, "minutes": coarse_minutes(s),
                         "geometry": [[round(p[0], 5), round(p[1], 5)] for p in g]})
    fetched = sorted({m["fetched_utc"][:10] for m in (roads_meta, places_meta, locs_meta, bnd_meta)})
    overlay = {"crs": "EPSG:4326", "scenario": "delhi-real",
               "source": "Roads, junction positions and place locations: OpenStreetMap data via the Overpass API "
                         f"(overpass-api.de), fetched {', '.join(fetched)}. (c) OpenStreetMap contributors, ODbL. "
                         "Edge minutes: road length / assumed class speed, coarsened. See SOURCES.md.",
               "disclaimer": "Imitation of Delhi: real roads and place locations from OpenStreetMap; organisation "
                             "names, capacities and donations are simulated.",
               "minutesSource": "osm-speed-model",
               "nodes": ov_nodes, "edges": ov_edges}
    parts = ['{\n  "crs": "EPSG:4326",\n  "scenario": "delhi-real",\n',
             f'  "source": {json.dumps(overlay["source"], ensure_ascii=False)},\n',
             f'  "disclaimer": {json.dumps(overlay["disclaimer"], ensure_ascii=False)},\n',
             '  "minutesSource": "osm-speed-model",\n  "nodes": [\n']
    parts.append(",\n".join("    " + json.dumps(n, ensure_ascii=False, separators=(",", ":")) for n in ov_nodes))
    parts.append('\n  ],\n  "edges": [\n')
    parts.append(",\n".join("    " + json.dumps(e, separators=(",", ":")) for e in ov_edges))
    parts.append("\n  ]\n}\n")
    (OUT / "geo_overlay.json").write_text("".join(parts), encoding="utf-8")
    json.loads((OUT / "geo_overlay.json").read_text(encoding="utf-8"))
    for stale in ("offers.csv",):
        if (OUT / stale).exists():
            (OUT / stale).unlink()

    # metadata (OSM ids of source places live only here)
    meta = {"seed": SEED, "radius_m": args.radius, "lat0": lat0, "lng0": lng0,
            "donors": [{"id": d["id"], "osm": d["src"]["osm"], "osm_tags": d["src"]["osm_tags"], "lat": d["src"]["lat"],
                        "lng": d["src"]["lng"], "node": d["node"], "snap_m": d["src"]["snap_m"],
                        "locality": d["locality"], **({"note": d["src"]["note"]} if "note" in d["src"] else {})}
                       for d in donors],
            "recipients": [{"id": r["id"], "osm": r["src"]["osm"], "osm_tags": r["src"]["osm_tags"],
                            "lat": r["src"]["lat"], "lng": r["src"]["lng"], "node": r["node"],
                            "snap_m": r["src"]["snap_m"], "locality": r["locality"]} for r in recips],
            "nodes": [{"id": k, "osm_node": F.osm_ids[c], "cluster_junctions": C["members"][k]}
                      for k, c in enumerate(C["centres"])]}
    (META_OUT / "delhi_real_meta.json").write_text(json.dumps(meta, indent=1, ensure_ascii=False), encoding="utf-8")

    sizes_b = {p.name: p.stat().st_size for p in sorted(OUT.glob("*")) if p.suffix in (".csv", ".json")}
    snap_d = sorted([d["src"]["snap_m"] for d in donors] + [r["src"]["snap_m"] for r in recips])
    build = {"fine": {"ways": F.n_ways, "raw_junctions": F.raw_junctions, "raw_segments": F.raw_segments,
                      "components": F.components, "dropped_junctions": F.dropped_junctions,
                      "dropped_km": F.dropped_km, "dropped_examples": F.dropped_examples, "lcc_km": F.lcc_km,
                      "km_by_class": F.km_by_class, "contracted_nodes": F.n, "chains": len(F.chains)},
             "coarse": {"nodes": K, "edges": len(edge_list), "radius_m": args.radius,
                        "minutes_hist": dict(sorted(Counter(coarse_minutes(s) for _, s in edge_list).items())),
                        "single_junction_nodes": sum(1 for m in C["members"] if m == 1)},
             "error": err, "stats": stats, "notes": notes, "raw_place_counts": {"|".join(k): v for k, v in
                                                                                sorted(raw_counts.items())},
             "donor_types": dict(Counter(d["type"] for d in donors)),
             "recipient_types": dict(Counter(r["type"] for r in recips)),
             "status": dict(sorted(Counter(d["status"] for d in dons).items())), "transactions": len(txs),
             "snap_m": {"median": pct(snap_d, 0.5), "p95": pct(snap_d, 0.95), "max": snap_d[-1]},
             "eps_hist": {str(k): v for k, v in sorted(eps_used.items())}, "sizes": sizes_b,
             "fetch": [roads_meta, places_meta, locs_meta, bnd_meta]}
    for m in build["fetch"]:
        m.pop("seconds", None)
    (META_OUT / "build_stats.json").write_text(json.dumps(build, indent=1, ensure_ascii=False), encoding="utf-8")
    import write_docs
    write_docs.write(OUT, build, SPEED_KMH, SEED, CAP, TYPES, FOOD_BY_DONOR, QTY, USABLE, META_OUT)
    print(f"[write] {OUT} sizes={sizes_b}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
