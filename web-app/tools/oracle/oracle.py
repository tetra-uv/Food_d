"""Independent oracle: re-implements the six rules + ranking in Python from the CSVs only,
to (a) verify README claims and (b) pick demo scenarios. Not part of the product."""
import csv, heapq, sys, collections
D = sys.argv[1]
def rows(n): return list(csv.DictReader(open(f"{D}/{n}.csv", newline="")))
nodes = rows("graph_nodes"); edges = rows("graph_edges")
donors = {r["donor_id"]: r for r in rows("donors")}
recips = rows("recipients"); dons = rows("donations")
adj = collections.defaultdict(list)
for e in edges:
    u, v, w = int(e["u"]), int(e["v"]), int(e["minutes"]); adj[u].append((v, w)); adj[v].append((u, w))
def dij(s):
    d = {s: 0}; pq = [(0, s)]
    while pq:
        dist, u = heapq.heappop(pq)
        if dist > d.get(u, 1 << 60): continue
        for v, w in adj[u]:
            nd = dist + w
            if nd < d.get(v, 1 << 60): d[v] = nd; heapq.heappush(pq, (nd, v))
    return d
print("edges:", len(edges), "nodes:", len(nodes), "donors:", len(donors), "recipients:", len(recips), "donations:", len(dons))
def match(don):
    dn = donors[don["donor_id"]]; dist = dij(int(dn["node_id"]))
    rem = int(don["remaining_quantity"]); use = int(don["usable_minutes"]); cands = []; rej = []
    for r in recips:
        n = int(r["node_id"]); t = dist.get(n)
        if r["accepting"] != "1": rej.append((r["recipient_id"], "NOT_ACCEPTING", t)); continue
        if don["food_type"] not in r["accepted_types"].split(";"): rej.append((r["recipient_id"], "FOOD_INCOMPATIBLE", t)); continue
        cap = int(r["available_capacity"])
        if cap <= 0: rej.append((r["recipient_id"], "NO_CAPACITY", t)); continue
        if r["can_pickup"] != "1": rej.append((r["recipient_id"], "NO_PICKUP", t)); continue
        if t is None: rej.append((r["recipient_id"], "UNREACHABLE", None)); continue
        if t > use: rej.append((r["recipient_id"], "TOO_FAR", t)); continue
        acc = min(rem, cap); cands.append((-acc, t, cap - acc, int(r["recipient_id"][1:]), r["recipient_id"]))
    cands.sort(); return cands, rej
open_ = [d for d in dons if d["status"] in ("PENDING", "PARTIAL")]
print("open donations:", len(open_))
zero = []; equal = []; sizes = []
for d in open_:
    c, rj = match(d); sizes.append(len(c))
    if not c: zero.append(d["donation_id"])
    use = int(d["usable_minutes"])
    if any(r[1] != "TOO_FAR" and False for r in rj): pass
    for x in c:
        if x[1] == use: equal.append((d["donation_id"], x[4], use))
print("open donations with ZERO candidates:", zero)
print("travel == usable (boundary-pass) cases:", equal)
print("candidate-count distribution:", dict(collections.Counter(sizes)))
c, rj = match(next(d for d in dons if d["donation_id"] == "DON001"))
print("DON001 candidates:", [(x[4], -x[0], x[1], x[2]) for x in c])
print("DON001 rejections:", rj)
# reason histogram over all open donations
h = collections.Counter(r[1] for d in open_ for r in match(d)[1]); print("reason histogram:", dict(h))
# multi-org nodes
byn = collections.defaultdict(list)
for k, v in donors.items(): byn[int(v["node_id"])].append(k)
for r in recips: byn[int(r["node_id"])].append(r["recipient_id"])
print("orgs per node (>1):", {n: v for n, v in sorted(byn.items()) if len(v) > 1})
