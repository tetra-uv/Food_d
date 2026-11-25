#!/usr/bin/env python3
"""validate_delhi_real.py - checks for data/scenarios/delhi-real (Python 3.12 standard library only).

    python -I validate_delhi_real.py [--folder <scenario>] [--no-engine] [--no-oracle]

1. Static checks adapted from tools/geo/validate_scenario.py (headers, ids, references, node numbering, minutes >= 1,
   duplicate edges, components, overlay agreement) plus loader invariants from console-app/src/Datamanager.cpp
   (status vs quantities, transactions sum = original - remaining, ascending transaction ids), name rules
   (1-60 bytes, no comma/CR/LF) and "transaction travel_minutes = shortest travel time on the integer graph".
2. Native engine on a TEMP COPY: `cb_engine.exe --data <temp> --self-test` (timed), then over stdio 100 x match.build
   (timed), net.route for every transaction (travel check by the engine itself), and candidate-count parity of
   match.build against an independent re-implementation of the six rules for every open donation.
3. tools/oracle/oracle.py on the scenario (read-only), summarised.
Results are written to tools/realmap/output/validation.json (read by the build to fill the report section).
Exit code 0 = no errors.
"""
from __future__ import annotations

import argparse
import ast
import csv
import heapq
import json
import re
import shutil
import subprocess
import sys
import tempfile
import time
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
WEB = HERE.parent.parent
DEFAULT = WEB / "data" / "scenarios" / "delhi-real"
EXE = WEB / "engine" / "build" / "cb_engine.exe"
ORACLE = WEB / "tools" / "oracle" / "oracle.py"
HEADERS = {
    "donors.csv": "donor_id,name,type,node_id",
    "recipients.csv": "recipient_id,name,type,available_capacity,accepted_types,accepting,can_pickup,node_id",
    "donations.csv": "donation_id,donor_id,food_type,original_quantity,remaining_quantity,usable_minutes,status",
    "transactions.csv": "transaction_id,donation_id,recipient_id,quantity,travel_minutes",
    "graph_nodes.csv": "node_id,label,x_km,y_km",
    "graph_edges.csv": "u,v,minutes",
}
DELHI_BOX = (76.80, 28.90, 77.40, 28.40)
MAX_GEOMETRY_POINTS = 60
FOODS = {"COOKED", "PACKAGED", "PRODUCE", "BAKERY"}
errors: list[str] = []
warnings: list[str] = []


def err(m): errors.append(m)
def warn(m): warnings.append(m)


def load(folder: Path, name: str):
    path = folder / name
    if not path.exists():
        err(f"{name}: missing"); return []
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != HEADERS[name]:
        err(f"{name}: header mismatch"); return []
    want = HEADERS[name].count(",") + 1
    out = []
    for i, ln in enumerate(lines[1:], start=2):
        f = ln.split(",")
        if len(f) != want:
            err(f"{name}:{i}: {len(f)} fields, expected {want}"); continue
        out.append(f)
    return out


def isid(t, p): return re.fullmatch(p + r"\d{3,}", t) is not None


def dij(adj, s):
    d = {s: 0}; pq = [(0, s)]
    while pq:
        x, u = heapq.heappop(pq)
        if x > d.get(u, 1 << 60): continue
        for v, w in adj[u]:
            nx = x + w
            if nx < d.get(v, 1 << 60): d[v] = nx; heapq.heappush(pq, (nx, v))
    return d


def static(folder: Path) -> dict:
    nodes = load(folder, "graph_nodes.csv"); edges = load(folder, "graph_edges.csv")
    donors = load(folder, "donors.csv"); recips = load(folder, "recipients.csv")
    dons = load(folder, "donations.csv"); txs = load(folder, "transactions.csv")
    n = len(nodes)
    for i, r in enumerate(nodes):
        if r[0] != str(i): err(f"graph_nodes.csv: node {r[0]} out of order")
        float(r[2]); float(r[3])
    adj = [[] for _ in range(n)]; seen = set()
    for r in edges:
        u, v, m = int(r[0]), int(r[1]), int(r[2])
        if not (0 <= u < n and 0 <= v < n) or u == v or m < 1: err(f"graph_edges.csv: bad edge {r}")
        k = (min(u, v), max(u, v))
        if k in seen: err(f"graph_edges.csv: duplicate {k}")
        seen.add(k); adj[u].append((v, m)); adj[v].append((u, m))
    lab = [-1] * n; comps = 0
    for s in range(n):
        if lab[s] >= 0: continue
        st = [s]; lab[s] = comps
        while st:
            x = st.pop()
            for y, _ in adj[x]:
                if lab[y] < 0: lab[y] = comps; st.append(y)
        comps += 1
    if comps != 1: warn(f"graph has {comps} components")

    def name_ok(nm, where):
        b = nm.encode("utf-8")
        if not (1 <= len(b) <= 60) or any(c in nm for c in ",\r\n"):
            err(f"{where}: name {nm!r} breaks the 1-60 byte / no comma rule")

    dn = {}
    for r in donors:
        if not isid(r[0], "DN") or r[0] in dn: err(f"donors.csv: bad/duplicate id {r[0]}")
        if not (0 <= int(r[3]) < n): err(f"donors.csv: node {r[3]}")
        name_ok(r[1], r[0]); dn[r[0]] = int(r[3])
    rc = {}
    for r in recips:
        if not isid(r[0], "R") or r[0] in rc: err(f"recipients.csv: bad/duplicate id {r[0]}")
        if int(r[3]) < 0 or not (0 <= int(r[7]) < n) or r[5] not in "01" or r[6] not in "01":
            err(f"recipients.csv: bad row {r[0]}")
        if not r[4] or any(t not in FOODS for t in r[4].split(";")): err(f"recipients.csv: food types {r[0]}")
        name_ok(r[1], r[0]); rc[r[0]] = r
    dd = {}
    for r in dons:
        o, rem, us, stt = int(r[3]), int(r[4]), int(r[5]), r[6]
        if not isid(r[0], "DON") or r[0] in dd: err(f"donations.csv: bad/duplicate id {r[0]}")
        if r[1] not in dn: err(f"donations.csv: donor {r[1]} missing")
        if r[2] not in FOODS: err(f"donations.csv: food {r[2]}")
        if o < 1 or rem < 0 or rem > o or us < 1: err(f"donations.csv: quantities {r[0]}")
        ok = {"PENDING": rem == o, "PARTIAL": 0 < rem < o, "COMPLETED": rem == 0, "CLOSED": True}.get(stt)
        if not ok: err(f"donations.csv: status {stt} does not fit quantities ({r[0]})")
        dd[r[0]] = r
    placed = Counter(); prev = -1; cache = {}
    for r in txs:
        num = int(r[0][1:])
        if not isid(r[0], "T") or num <= prev: err(f"transactions.csv: id order {r[0]}")
        prev = num
        if r[1] not in dd or r[2] not in rc: err(f"transactions.csv: refs {r[0]}"); continue
        placed[r[1]] += int(r[3])
        s = dn[dd[r[1]][1]]
        if s not in cache: cache[s] = dij(adj, s)
        t = cache[s].get(int(rc[r[2]][7]))
        if t != int(r[4]): err(f"transactions.csv: {r[0]} travel {r[4]} but shortest is {t}")
        if dd[r[1]][2] not in rc[r[2]][4].split(";"): warn(f"{r[0]}: recipient does not accept the food type")
    for k, r in dd.items():
        if int(r[3]) - int(r[4]) != placed[k]: err(f"donations.csv: {k} given {int(r[3]) - int(r[4])} but transactions sum {placed[k]}")
    if (folder / "offers.csv").exists(): err("offers.csv must not exist in this scenario")
    # overlay
    ov = json.loads((folder / "geo_overlay.json").read_text(encoding="utf-8"))
    for k in ("crs", "scenario", "source", "disclaimer", "minutesSource", "nodes", "edges"):
        if k not in ov: err(f"geo_overlay.json: missing {k}")
    if [o["id"] for o in ov["nodes"]] != list(range(n)): err("geo_overlay.json: node ids")
    for o in ov["nodes"]:
        if not (DELHI_BOX[3] <= o["lat"] <= DELHI_BOX[1] and DELHI_BOX[0] <= o["lng"] <= DELHI_BOX[2]):
            err(f"geo_overlay.json: node {o['id']} outside Delhi box")
        if o.get("confidence") not in ("verified", "approx"): err(f"geo_overlay.json: confidence {o['id']}")
    if len(ov["edges"]) != len(edges): err("geo_overlay.json: edge count")
    pts = 0
    for o, r in zip(ov["edges"], edges):
        if (o["u"], o["v"], o["minutes"]) != (int(r[0]), int(r[1]), int(r[2])): err(f"geo_overlay.json: edge {o['u']}-{o['v']}")
        g = o["geometry"]; pts += len(g)
        if not (2 <= len(g) <= MAX_GEOMETRY_POINTS): err(f"geo_overlay.json: edge {o['u']}-{o['v']} has {len(g)} points")
        if not all(DELHI_BOX[0] <= p[0] <= DELHI_BOX[2] and DELHI_BOX[3] <= p[1] <= DELHI_BOX[1] for p in g):
            err(f"geo_overlay.json: edge {o['u']}-{o['v']} geometry outside Delhi box")
    return {"nodes": n, "edges": len(edges), "components": comps, "donors": len(donors), "recipients": len(recips),
            "donations": len(dons), "transactions": len(txs), "geometry_points": pts,
            "overlay_bytes": (folder / "geo_overlay.json").stat().st_size,
            "_adj": adj, "_dn": dn, "_rc": rc, "_dd": dd, "_txs": txs}


def rules_count(S, did):
    r = S["_dd"][did]; dist = dij(S["_adj"], S["_dn"][r[1]]); rem, us = int(r[4]), int(r[5]); c = 0
    for x in S["_rc"].values():
        t = dist.get(int(x[7]))
        if x[5] == "1" and r[2] in x[4].split(";") and int(x[3]) > 0 and x[6] == "1" and t is not None and t <= us:
            c += 1
    return c


def engine(folder: Path, S: dict) -> dict:
    res = {}
    with tempfile.TemporaryDirectory(prefix="cb-delhi-real-") as tmp:
        copy = Path(tmp) / "delhi-real"
        shutil.copytree(folder, copy)
        times = []
        for _ in range(3):
            t0 = time.perf_counter()
            p = subprocess.run([str(EXE), "--data", str(copy), "--self-test"], capture_output=True, timeout=600)
            times.append(time.perf_counter() - t0)
        out = p.stdout.decode("utf-8", "replace").strip().splitlines()
        rep = json.loads(out[-1]) if out else {}
        res["self_test_exit"] = p.returncode
        res["self_test_ok"] = rep.get("ok")
        res["self_test_seconds"] = [round(t, 3) for t in times]
        res["self_test_open"] = rep.get("openDonations"); res["self_test_matches"] = rep.get("matchesBuilt")
        res["self_test_problems"] = rep.get("invariantProblems")
        res["self_test_warnings"] = len(rep.get("warnings", []) or [])
        res["stderr_tail"] = p.stderr.decode("utf-8", "replace")[-400:]
        if p.returncode != 0 or not rep.get("ok"):
            err(f"engine self-test failed: exit {p.returncode} {out[-1][:300] if out else ''}")
            return res
        t0 = time.perf_counter()
        pr = subprocess.Popen([str(EXE), "--data", str(copy)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL)
        ready = pr.stdout.readline()
        res["boot_seconds"] = round(time.perf_counter() - t0, 3)
        res["ready_event"] = json.loads(ready).get("event")
        k = [0]

        def call(cmd, **a):
            k[0] += 1
            pr.stdin.write((json.dumps({"v": 1, "id": k[0], "cmd": cmd, "args": a}) + "\n").encode()); pr.stdin.flush()
            line = pr.stdout.readline()
            return json.loads(line), len(line)

        open_ids = [d for d, r in S["_dd"].items() if r[6] in ("PENDING", "PARTIAL")]
        lat, sizes = [], []
        for i in range(100):
            did = open_ids[i % len(open_ids)]
            t = time.perf_counter(); r, nbytes = call("match.build", donationId=did); lat.append((time.perf_counter() - t) * 1000)
            sizes.append(nbytes)
            if not r.get("ok"): err(f"match.build {did}: {r.get('error')}")
        lat.sort(); sizes.sort()
        res["match_build_100_ms"] = {"p50": round(lat[50], 2), "p95": round(lat[95], 2), "max": round(lat[-1], 2),
                                     "total": round(sum(lat), 1)}
        res["match_build_bytes"] = {"median": sizes[50], "max": sizes[-1]}
        mism = []
        for did in open_ids:
            r, _ = call("match.build", donationId=did)
            got = len(r["data"]["candidates"]); want = rules_count(S, did)
            if got != want: mism.append((did, got, want))
        res["candidate_parity"] = {"checked": len(open_ids), "mismatches": mism[:10], "n_mismatch": len(mism)}
        if mism: err(f"engine candidate counts differ from the rules re-implementation for {len(mism)} donations")
        bad = []
        for row in S["_txs"]:
            src = S["_dn"][S["_dd"][row[1]][1]]; dst = int(S["_rc"][row[2]][7])
            r, _ = call("net.route", **{"from": src, "to": dst})
            if not r.get("ok") or r["data"].get("minutes") != int(row[4]): bad.append((row[0], row[4], r.get("data", {}).get("minutes")))
        res["transaction_travel_engine_check"] = {"checked": len(S["_txs"]), "bad": bad}
        if bad: err(f"engine net.route disagrees with transactions: {bad[:5]}")
        r, nb = call("read.snapshot"); res["snapshot_bytes"] = nb
        r, nb = call("read.network"); res["network_bytes"] = nb
        pr.stdin.close(); pr.wait(timeout=60)
    return res


def oracle(folder: Path) -> dict:
    p = subprocess.run([sys.executable, "-I", str(ORACLE), str(folder)], capture_output=True, timeout=900)
    out = p.stdout.decode("utf-8", "replace")
    res = {"exit": p.returncode}
    for ln in out.splitlines():
        if ln.startswith("open donations:"): res["open"] = int(ln.split(":")[1])
        elif ln.startswith("open donations with ZERO candidates:"):
            z = ast.literal_eval(ln.split(":", 1)[1].strip()); res["zero"] = len(z); res["zero_ids"] = z[:40]
        elif ln.startswith("travel == usable"):
            b = ast.literal_eval(ln.split(":", 1)[1].strip()); res["boundary_cases"] = len(b); res["boundary_examples"] = b[:8]
        elif ln.startswith("candidate-count distribution:"):
            res["cand_dist"] = ast.literal_eval(ln.split(":", 1)[1].strip())
        elif ln.startswith("reason histogram:"):
            res["reasons"] = ast.literal_eval(ln.split(":", 1)[1].strip())
        elif ln.startswith("edges:"):
            res["header"] = ln.strip()
    if "open" in res and res["open"]:
        res["zero_share"] = round(res["zero"] / res["open"], 3)
    if p.returncode != 0: err(f"oracle exit {p.returncode}: {p.stderr.decode('utf-8', 'replace')[-300:]}")
    return res


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"): sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(); ap.add_argument("--folder", type=Path, default=DEFAULT)
    ap.add_argument("--no-engine", action="store_true"); ap.add_argument("--no-oracle", action="store_true")
    a = ap.parse_args(); folder = a.folder.resolve()
    S = static(folder)
    out = {"static": {k: v for k, v in S.items() if not k.startswith("_")}}
    print("static:", out["static"], flush=True)
    if not a.no_engine:
        out["engine"] = engine(folder, S); print("engine:", out["engine"], flush=True)
    if not a.no_oracle:
        out["oracle"] = oracle(folder); print("oracle:", {k: v for k, v in out["oracle"].items() if k != "zero_ids"}, flush=True)
    out["errors"] = errors; out["warnings"] = warnings
    for w in warnings: print("WARNING:", w)
    for e in errors: print("ERROR:", e)
    (HERE / "output").mkdir(exist_ok=True)
    (HERE / "output" / "validation.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
    print(f"RESULT: {'FAIL' if errors else 'OK'} ({len(errors)} errors, {len(warnings)} warnings)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
