#!/usr/bin/env python3
"""validate_scenario.py - static checks for a CommunityBridge scenario folder (python 3.12, stdlib only).

Usage:
    python -I validate_scenario.py <scenario-folder> [--cpp] [--exe <path to CommunityBridge.exe>]

Checks the six-CSV contract used by console-app/src/Datamanager.cpp (exact headers, id formats with >= 3 digits,
references, node numbering 0..n-1, edge minutes integers >= 1, no self loops / duplicate pairs), counts graph
components, and - when geo_overlay.json exists - that the overlay agrees with the CSVs.
With --cpp the folder is COPIED to a temp directory and console-app/CommunityBridge.exe is started on the copy with
stdin "0\\n" (exit immediately) so the real C++ loader validates the data. The real folder is never passed to the exe.
Exit code 0 = no errors (warnings allowed), 1 = errors.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_EXE = HERE.parent.parent.parent / "console-app" / "CommunityBridge.exe"

HEADERS = {
    "donors.csv": "donor_id,name,type,node_id",
    "recipients.csv": "recipient_id,name,type,available_capacity,accepted_types,accepting,can_pickup,node_id",
    "donations.csv": "donation_id,donor_id,food_type,original_quantity,remaining_quantity,usable_minutes,status",
    "transactions.csv": "transaction_id,donation_id,recipient_id,quantity,travel_minutes",
    "graph_nodes.csv": "node_id,label,x_km,y_km",
    "graph_edges.csv": "u,v,minutes",
}
DELHI_BOX = (76.80, 28.90, 77.40, 28.40)  # lng_min, lat_max, lng_max, lat_min
MAX_GEOMETRY_POINTS = 60

errors: list[str] = []
warnings: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def warn(msg: str) -> None:
    warnings.append(msg)


def load(folder: Path, name: str) -> list[list[str]]:
    path = folder / name
    if not path.exists():
        err(f"{name}: file missing")
        return []
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines:
        err(f"{name}: empty file")
        return []
    if lines[0].strip() != HEADERS[name]:
        err(f"{name}: header is {lines[0]!r}, expected {HEADERS[name]!r}")
    want = HEADERS[name].count(",") + 1
    rows = []
    for i, ln in enumerate(lines[1:], start=2):
        if not ln.strip():
            continue
        f = next(csv.reader([ln]))
        if len(f) != want:
            err(f"{name}:{i}: {len(f)} fields, expected {want}")
            continue
        rows.append([x.strip() for x in f] + [str(i)])  # last element = line number
    return rows


def is_id(text: str, prefix: str) -> bool:
    return re.fullmatch(prefix + r"\d{3,}", text) is not None


def to_int(text: str):
    return int(text) if re.fullmatch(r"-?\d+", text) else None


def check(folder: Path) -> dict:
    nodes = load(folder, "graph_nodes.csv")
    edges = load(folder, "graph_edges.csv")
    donors = load(folder, "donors.csv")
    recips = load(folder, "recipients.csv")
    dons = load(folder, "donations.csv")
    txs = load(folder, "transactions.csv")

    # nodes: numbered 0..n-1 in order, numeric coordinates
    for pos, r in enumerate(nodes):
        if to_int(r[0]) != pos:
            err(f"graph_nodes.csv:{r[-1]}: node ids must be 0,1,2,... in order (found {r[0]!r})")
        for col in (2, 3):
            try:
                float(r[col])
            except ValueError:
                err(f"graph_nodes.csv:{r[-1]}: coordinate {r[col]!r} is not a number")
    n = len(nodes)

    # edges
    adj = {i: set() for i in range(n)}
    seen = set()
    minutes = []
    for r in edges:
        u, v, m = to_int(r[0]), to_int(r[1]), to_int(r[2])
        if None in (u, v, m):
            err(f"graph_edges.csv:{r[-1]}: u, v and minutes must be integers")
            continue
        if not (0 <= u < n and 0 <= v < n):
            err(f"graph_edges.csv:{r[-1]}: endpoint outside node range 0..{n - 1}")
            continue
        if u == v:
            err(f"graph_edges.csv:{r[-1]}: self loop")
        if m < 1:
            err(f"graph_edges.csv:{r[-1]}: minutes must be >= 1 (got {m})")
        key = (min(u, v), max(u, v))
        if key in seen:
            warn(f"graph_edges.csv:{r[-1]}: duplicate edge {key} (loader keeps the smaller minutes)")
        seen.add(key)
        adj[u].add(v)
        adj[v].add(u)
        minutes.append(m)

    # components
    comp, label = 0, {}
    for s in range(n):
        if s in label:
            continue
        stack = [s]
        label[s] = comp
        while stack:
            x = stack.pop()
            for y in adj[x]:
                if y not in label:
                    label[y] = comp
                    stack.append(y)
        comp += 1
    comp_sizes = Counter(label.values())

    # donors
    donor_ids = set()
    for r in donors:
        if not is_id(r[0], "DN"):
            err(f"donors.csv:{r[-1]}: bad donor id {r[0]!r} (DN + >= 3 digits)")
        if r[0] in donor_ids:
            err(f"donors.csv:{r[-1]}: duplicate id {r[0]}")
        donor_ids.add(r[0])
        nd = to_int(r[3])
        if nd is None or not (0 <= nd < n):
            err(f"donors.csv:{r[-1]}: node_id {r[3]!r} not in 0..{n - 1}")

    # recipients
    rec_ids, rec_node = set(), {}
    foods = {"COOKED", "PACKAGED", "PRODUCE", "BAKERY"}
    for r in recips:
        if not is_id(r[0], "R"):
            err(f"recipients.csv:{r[-1]}: bad recipient id {r[0]!r} (R + >= 3 digits)")
        if r[0] in rec_ids:
            err(f"recipients.csv:{r[-1]}: duplicate id {r[0]}")
        rec_ids.add(r[0])
        cap, nd = to_int(r[3]), to_int(r[7])
        if cap is None or cap < 0:
            err(f"recipients.csv:{r[-1]}: capacity must be an integer >= 0")
        if nd is None or not (0 <= nd < n):
            err(f"recipients.csv:{r[-1]}: node_id {r[7]!r} not in 0..{n - 1}")
        else:
            rec_node[r[0]] = nd
        for t in filter(None, r[4].split(";")):
            if t not in foods:
                warn(f"recipients.csv:{r[-1]}: food type {t!r} is not one of {sorted(foods)} (check Models.cpp)")

    # donations
    don_ids, usable = set(), []
    for r in dons:
        if not is_id(r[0], "DON"):
            err(f"donations.csv:{r[-1]}: bad donation id {r[0]!r} (DON + >= 3 digits)")
        if r[0] in don_ids:
            err(f"donations.csv:{r[-1]}: duplicate id {r[0]}")
        don_ids.add(r[0])
        if r[1] not in donor_ids:
            err(f"donations.csv:{r[-1]}: donor {r[1]} not in donors.csv")
        o, rem, us = to_int(r[3]), to_int(r[4]), to_int(r[5])
        if None in (o, rem, us) or o < 1 or rem < 0 or rem > o:
            err(f"donations.csv:{r[-1]}: quantities invalid (original >= 1, 0 <= remaining <= original)")
        if us is None or us < 1:
            err(f"donations.csv:{r[-1]}: usable_minutes must be an integer >= 1")
        else:
            usable.append(us)

    # transactions
    tx_ids = set()
    for r in txs:
        if not is_id(r[0], "T"):
            err(f"transactions.csv:{r[-1]}: bad transaction id {r[0]!r}")
        if r[0] in tx_ids:
            err(f"transactions.csv:{r[-1]}: duplicate id {r[0]}")
        tx_ids.add(r[0])
        if r[1] not in don_ids or r[2] not in rec_ids:
            err(f"transactions.csv:{r[-1]}: donation or recipient not found")
        if to_int(r[3]) is None or to_int(r[4]) is None:
            err(f"transactions.csv:{r[-1]}: quantity and travel_minutes must be integers")

    # recipients that cannot reach any donor (the loader only warns)
    donor_comps = {label[to_int(r[3])] for r in donors if to_int(r[3]) in label}
    cut_off = [rid for rid, nd in rec_node.items() if nd in label and label[nd] not in donor_comps]
    if cut_off:
        warn(f"{len(cut_off)} recipient(s) not connected to any donor (loader warns, by design in lab): {', '.join(cut_off)}")

    if (folder / "offers.csv").exists():
        warn("offers.csv exists in the scenario folder (this scenario is not supposed to have one)")

    overlay_info = check_overlay(folder, nodes, edges) if (folder / "geo_overlay.json").exists() else None
    return {"nodes": n, "edges": len(edges), "components": dict(sorted(comp_sizes.items())),
            "donors": len(donors), "recipients": len(recips), "donations": len(dons), "transactions": len(txs),
            "minutes": minutes, "usable": usable, "overlay": overlay_info}


def check_overlay(folder: Path, nodes, edges) -> dict:
    try:
        ov = json.loads((folder / "geo_overlay.json").read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        err(f"geo_overlay.json: invalid JSON: {exc}")
        return {}
    for k in ("crs", "scenario", "source", "disclaimer", "minutesSource", "nodes", "edges"):
        if k not in ov:
            err(f"geo_overlay.json: missing key {k!r}")
    if ov.get("crs") != "EPSG:4326":
        err("geo_overlay.json: crs must be EPSG:4326")
    if ov.get("minutesSource") not in ("osrm", "estimate"):
        warn(f"geo_overlay.json: minutesSource is {ov.get('minutesSource')!r}, expected 'osrm' or 'estimate'")
    onodes, oedges = ov.get("nodes", []), ov.get("edges", [])
    if [o.get("id") for o in onodes] != list(range(len(nodes))):
        err("geo_overlay.json: node ids must be 0..n-1 matching graph_nodes.csv")
    for o in onodes:
        lat, lng = o.get("lat"), o.get("lng")
        if not (isinstance(lat, (int, float)) and isinstance(lng, (int, float))
                and DELHI_BOX[3] <= lat <= DELHI_BOX[1] and DELHI_BOX[0] <= lng <= DELHI_BOX[2]):
            err(f"geo_overlay.json: node {o.get('id')} lat/lng missing or outside Delhi box")
        if o.get("confidence") not in ("verified", "approx"):
            err(f"geo_overlay.json: node {o.get('id')} confidence must be verified|approx")
    if len(oedges) != len(edges):
        err(f"geo_overlay.json: {len(oedges)} edges, graph_edges.csv has {len(edges)}")
    est = 0
    for o, r in zip(oedges, edges):
        if (o.get("u"), o.get("v"), o.get("minutes")) != (to_int(r[0]), to_int(r[1]), to_int(r[2])):
            err(f"geo_overlay.json: edge {o.get('u')}-{o.get('v')} does not match graph_edges.csv line {r[-1]} (u,v,minutes)")
        g = o.get("geometry")
        if g is not None:
            if not (2 <= len(g) <= MAX_GEOMETRY_POINTS):
                err(f"geo_overlay.json: edge {o.get('u')}-{o.get('v')} geometry has {len(g)} points (2..{MAX_GEOMETRY_POINTS})")
            elif not all(len(p) == 2 and DELHI_BOX[0] <= p[0] <= DELHI_BOX[2] and DELHI_BOX[3] <= p[1] <= DELHI_BOX[1] for p in g):
                err(f"geo_overlay.json: edge {o.get('u')}-{o.get('v')} geometry has a point that is not [lng,lat] in Delhi")
        est += o.get("minutesSource") == "estimate"
    return {"minutesSource": ov.get("minutesSource"), "estimate_edges": est, "osrm_edges": len(oedges) - est}


def cpp_check(folder: Path, exe: Path) -> bool:
    if not exe.exists():
        err(f"--cpp: executable not found: {exe}")
        return False
    with tempfile.TemporaryDirectory(prefix="cb-scenario-check-") as tmp:
        copy = Path(tmp) / "scenario"
        shutil.copytree(folder, copy)
        res = subprocess.run([str(exe), str(copy)], input="0\n", capture_output=True, text=True,
                             encoding="utf-8", errors="replace", timeout=60, cwd=tmp)
        print(f"--- C++ loader on a temp copy: {exe.name} <temp copy of {folder.name}>, stdin '0' ---")
        print((res.stdout or "").rstrip())
        if res.stderr.strip():
            print("[stderr] " + res.stderr.rstrip())
        print(f"--- exit code {res.returncode} ---")
        # the temp copy may have been modified by the program (it saves tables); the real folder never is
        ok = res.returncode == 0 and "error" not in (res.stdout + res.stderr).lower()
        if not ok:
            err(f"C++ loader reported a problem (exit {res.returncode})")
        return ok


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", type=Path)
    ap.add_argument("--cpp", action="store_true", help="also run console-app/CommunityBridge.exe on a TEMP COPY")
    ap.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    args = ap.parse_args()
    folder = args.folder.resolve()

    s = check(folder)
    print(f"scenario: {folder}")
    print(f"nodes={s['nodes']} edges={s['edges']} donors={s['donors']} recipients={s['recipients']} "
          f"donations={s['donations']} transactions={s['transactions']}")
    print(f"graph components (id: size): {s['components']}")
    if s["minutes"]:
        m = sorted(s["minutes"])
        print(f"edge minutes min/median/max: {m[0]} / {m[len(m) // 2]} / {m[-1]}")
    if s["usable"]:
        u = sorted(x for x in s["usable"] if x != 999999999)
        print(f"usable_minutes (excluding 999999999) min/median/max: {u[0]} / {u[len(u) // 2]} / {u[-1]}")
    if s["overlay"]:
        print(f"geo_overlay.json: {s['overlay']}")
    if args.cpp:
        cpp_check(folder, args.exe)
    for w in warnings:
        print("WARNING:", w)
    for e in errors:
        print("ERROR:", e)
    print(f"RESULT: {'FAIL' if errors else 'OK'} ({len(errors)} errors, {len(warnings)} warnings)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
