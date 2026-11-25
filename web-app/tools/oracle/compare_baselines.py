"""Prototype (plan-time) independent oracle #2: Dijkstra vs the project's straight-line baseline
(ceil(km / 0.5 km-per-min), see console-app/include/traveltime.h) and what-if checks on DON001.
Usage: python -I compare_baselines.py <data-folder>      e.g.  ../../../desktop-app/data
Reads CSVs only. NOT product code: it is the cross-check used by the golden-parity suite.
Result on the lab data: 195 feasible pairs, 99 false positives, 8 false negatives."""
import csv, heapq, math, collections, sys
D = sys.argv[1]
def rows(n): return list(csv.DictReader(open(f"{D}/{n}.csv", newline="")))
nodes = {int(r["node_id"]): (float(r["x_km"]), float(r["y_km"])) for r in rows("graph_nodes")}
adj = collections.defaultdict(list)
for e in rows("graph_edges"):
    u, v, w = int(e["u"]), int(e["v"]), int(e["minutes"]); adj[u].append((v, w)); adj[v].append((u, w))
donors = {r["donor_id"]: r for r in rows("donors")}; recips = rows("recipients"); dons = rows("donations")
def dij(s):
    d = {s: 0}; pq = [(0, s)]
    while pq:
        dist, u = heapq.heappop(pq)
        if dist > d[u]: continue
        for v, w in adj[u]:
            nd = dist + w
            if v not in d or nd < d[v]: d[v] = nd; heapq.heappush(pq, (nd, v))
    return d
def straight(s):
    x0, y0 = nodes[s]; return {n: math.ceil(math.hypot(x - x0, y - y0) / 0.5) for n, (x, y) in nodes.items()}
def feasible(don, dist, usable=None):
    use = int(don["usable_minutes"]) if usable is None else usable; out = set()
    for r in recips:
        if r["accepting"] != "1" or don["food_type"] not in r["accepted_types"].split(";"): continue
        if int(r["available_capacity"]) <= 0 or r["can_pickup"] != "1": continue
        t = dist.get(int(r["node_id"]))
        if t is not None and t <= use: out.add(r["recipient_id"])
    return out
d1 = next(d for d in dons if d["donation_id"] == "DON001")
for u in (12, 11, 7): print("DON001 usable", u, "->", sorted(feasible(d1, dij(0), u)))
fp = fn = tot = affected_fp = affected_fn = n = 0
for d in dons:
    if d["status"] not in ("PENDING", "PARTIAL"): continue
    n += 1; s = int(donors[d["donor_id"]]["node_id"]); a = feasible(d, dij(s)); b = feasible(d, straight(s))
    tot += len(a); fp += len(b - a); fn += len(a - b); affected_fp += bool(b - a); affected_fn += bool(a - b)
print(f"open donations {n}: true-feasible pairs {tot}, false positives {fp}, false negatives {fn}, "
      f"donations with a false positive {affected_fp}, with a false negative {affected_fn}")
