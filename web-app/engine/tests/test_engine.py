"""Engine tests: drives cb_engine.exe over its JSON-lines protocol. Run: python -I engine/tests/test_engine.py
Prints PASS/FAIL counts; exit code 1 if any check failed. Read-only on web-app/data/scenarios/lab (copies for writes)."""
import csv, heapq, json, os, random, re, shutil, subprocess, sys, tempfile, time, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE_DIR = os.path.dirname(HERE)
WEB = os.path.dirname(ENGINE_DIR)
REPO = os.path.dirname(WEB)
EXE = os.environ.get("CB_ENGINE") or os.path.join(ENGINE_DIR, "build", "cb_engine.exe")
# CB_ENGINE_CMD runs another engine with the same command line, e.g. the WASM build through its Node shim:
#   CB_ENGINE_CMD='["node", "web-app/engine-wasm/test/shim.mjs"]'   (JSON list) or a plain string split like a shell
def _engine_cmd():
    c = os.environ.get("CB_ENGINE_CMD")
    if not c: return [EXE]
    if c.lstrip().startswith("["): return json.loads(c)
    import shlex
    return [x.strip('"') for x in shlex.split(c, posix=False)]
ENGINE = _engine_cmd()
LAB = os.path.join(WEB, "data", "scenarios", "lab")
WORK = os.path.join(ENGINE_DIR, "testdata", "work")

passed = failed = 0
def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
    else:
        failed += 1
        print("FAIL:", name, ("-- " + str(detail)[:300]) if detail else "")

class Eng:
    def __init__(self, data):
        self.p = subprocess.Popen(ENGINE + ["--data", data], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE)
        self.nid = 0
        self.ready = json.loads(self.p.stdout.readline().decode("utf-8"))
    def raw(self, data: bytes):
        self.p.stdin.write(data); self.p.stdin.flush()
        return self.p.stdout.readline()
    def call(self, cmd, **args):
        self.nid += 1
        line = json.dumps({"v": 1, "id": self.nid, "cmd": cmd, "args": args}) + "\n"
        resp = json.loads(self.raw(line.encode("utf-8")).decode("utf-8"))
        assert resp["id"] == self.nid, resp
        return resp
    def close(self):
        try:
            self.call("sys.shutdown")
        except Exception:
            pass
        try:
            self.p.stdin.close(); self.p.wait(timeout=5)
        except Exception:
            self.p.kill()
        self.p.stdout.close(); self.p.stderr.close()

def fresh_copy(name):
    d = os.path.join(WORK, name)
    if os.path.exists(d): shutil.rmtree(d)
    os.makedirs(d)
    for f in ["donors", "recipients", "donations", "transactions", "graph_nodes", "graph_edges"]:
        shutil.copy(os.path.join(LAB, f + ".csv"), d)
    return d

# ---------------------------------------------------------------------------------------------- oracle (independent)
def rows(folder, n): return list(csv.DictReader(open(os.path.join(folder, n + ".csv"), newline="")))

class Oracle:
    def __init__(self, folder):
        self.donors = {r["donor_id"]: r for r in rows(folder, "donors")}
        self.recips = rows(folder, "recipients")
        self.dons = rows(folder, "donations")
        self.adj = collections.defaultdict(list)
        for e in rows(folder, "graph_edges"):
            u, v, w = int(e["u"]), int(e["v"]), int(e["minutes"]); self.adj[u].append((v, w)); self.adj[v].append((u, w))
    def dij(self, s):
        d = {s: 0}; pq = [(0, s)]
        while pq:
            dist, u = heapq.heappop(pq)
            if dist > d.get(u, 1 << 60): continue
            for v, w in self.adj[u]:
                nd = dist + w
                if nd < d.get(v, 1 << 60): d[v] = nd; heapq.heappush(pq, (nd, v))
        return d
    def match(self, don):
        dist = self.dij(int(self.donors[don["donor_id"]]["node_id"]))
        open_ = don["status"] in ("PENDING", "PARTIAL")
        rem = int(don["remaining_quantity"]) if open_ else 0
        use = int(don["usable_minutes"]); cands = []; rej = {}
        for r in self.recips:
            t = dist.get(int(r["node_id"])); rid = r["recipient_id"]
            if r["accepting"] != "1": rej[rid] = "NOT_ACCEPTING"; continue
            if don["food_type"] not in r["accepted_types"].split(";"): rej[rid] = "FOOD_INCOMPATIBLE"; continue
            cap = int(r["available_capacity"])
            if cap <= 0: rej[rid] = "NO_CAPACITY"; continue
            if r["can_pickup"] != "1": rej[rid] = "NO_PICKUP"; continue
            if t is None: rej[rid] = "UNREACHABLE"; continue
            if t > use: rej[rid] = "TOO_FAR"; continue
            acc = min(rem, cap)
            if acc < 1: continue
            cands.append((-acc, t, cap - acc, int(rid[1:]), rid))
        cands.sort()
        return [(c[4], -c[0], c[1], c[2]) for c in cands], rej

# ---------------------------------------------------------------------------------------------- tests
def test_json_and_robustness():
    e = Eng(LAB)
    try:
        check("ready event", e.ready.get("event") == "ready" and e.ready["protocol"] == 1)
        samples = [
            {"a": 1}, {"s": "héllo wörld ✓"}, {"emoji": "😀 𝄞"}, {"esc": "q\" b\\ s/ \n\r\t\b\f \u0001 \u001f"},
            {"nested": [[[1, 2, [3]], {"x": [None, True, False]}]]}, {"nums": [0, -1, 2147483647, -2147483648, 123456789012345, 1.5, -0.25, 1e10, 2.5e-5]},
            {"empty": {}, "e2": [], "e3": ""}, {"key é": "v"}, {"zero": 0, "neg0": -0.0},
        ]
        for s in samples:
            r = e.call("sys.echo", **s)
            back = r.get("data")
            check("echo round trip " + json.dumps(s)[:40], r["ok"] and back == s, (r, s))
        # surrogate pair escape and unicode escapes sent as raw escapes
        raw = e.raw(b'{"id":7,"cmd":"sys.echo","args":{"p":"\\ud83d\\ude00 \\u00e9 \\u0041"}}\n')
        check("unicode escapes decode", json.loads(raw.decode())["data"]["p"] == "\U0001F600 \u00e9 A", raw)
        # non-ascii names in CSV-free path are returned as UTF-8
        bad = [
            b"garbage\n", b"{\n", b"{}\n", b"[]\n", b"null\n", b'{"id":1}\n', b'{"id":1,"cmd":5}\n', b'{"id":1,"cmd":"x","args":[]}\n',
            b'{"id":1,"cmd":"sys.ping",}\n', b'{"id":1,"cmd":"sys.ping"} x\n', b'{"id":1,"cmd":"sys.ping" /*c*/}\n',
            b'{"id":1,"cmd":"sys.echo","args":{"a":"\xff\xfe"}}\n', b'{"id":1,"cmd":"sys.echo","args":{"a":"\\ud800"}}\n',
            b'{"id":1,"cmd":"sys.echo","args":{"a":01}}\n', b'{"id":1,"cmd":"sys.echo","args":{"a":"\x01"}}\n',
            b'{"id":1,"cmd":"sys.echo","args":{"a":1.}}\n', b'{"id":1,"cmd":"sys.echo","args":{"a":-}}\n',
            b'{"id":1,"cmd":"nope"}\n', b'{"v":2,"id":1,"cmd":"sys.ping"}\n',
            (b"[" * 100 + b"]" * 100 + b"\n"), (b'{"id":1,"cmd":"sys.echo","args":' + b'{"a":' * 40 + b"1" + b"}" * 40 + b"}\n"),
        ]
        for i, b in enumerate(bad):
            out = e.raw(b)
            try:
                r = json.loads(out.decode("utf-8"))
                check("malformed #%d answered with an error" % i, r.get("ok") is False and "code" in r.get("error", {}), out)
            except Exception as ex:
                check("malformed #%d answered with valid JSON" % i, False, (out, ex))
        # line too long
        out = e.raw(b'{"id":1,"cmd":"sys.echo","args":{"a":"' + b"x" * (1024 * 1024 + 10) + b'"}}\n')
        check("line too long -> LINE_TOO_LONG", json.loads(out)["error"]["code"] == "LINE_TOO_LONG", out[:200])
        # random fuzz
        rnd = random.Random(12345)
        base = [b'{"v":1,"id":3,"cmd":"match.build","args":{"donationId":"DON001"}}', b'{"id":"a","cmd":"sys.echo","args":{"x":[1,2,{"y":"z"}]}}']
        ok_lines = 0
        for k in range(1500):
            b = bytearray(rnd.choice(base))
            for _ in range(rnd.randint(1, 6)):
                op = rnd.random(); pos = rnd.randrange(len(b)) if b else 0
                if op < 0.4 and b: b[pos] = rnd.randrange(256)
                elif op < 0.7 and b: del b[pos:pos + rnd.randint(1, 5)]
                else: b[pos:pos] = bytes(rnd.randrange(256) for _ in range(rnd.randint(1, 4)))
            b = bytes(x for x in b if x != 10)
            out = e.raw(b + b"\n")
            try:
                json.loads(out.decode("utf-8")); ok_lines += 1
            except Exception:
                pass
        check("fuzz: 1500 mutated lines each answered with one valid JSON line", ok_lines == 1500, ok_lines)
        r = e.call("sys.ping")
        check("engine alive and responsive after malformed input", r["ok"] and r["data"]["pong"] is True)
        check("reads never change rev", r["rev"] == 0, r["rev"])
        # stdout has only protocol lines: a bare empty line is ignored
        out = e.raw(b"\n" + b'{"id":99,"cmd":"sys.ping"}\n')
        check("empty line ignored", json.loads(out)["id"] == 99)
    finally:
        e.close()

def test_parity():
    e = Eng(LAB); o = Oracle(LAB)
    try:
        mism = 0; n = 0
        for don in o.dons:
            r = e.call("match.build", donationId=don["donation_id"])
            n += 1
            if not r["ok"]:
                mism += 1; print("  parity: engine error", don["donation_id"], r); continue
            d = r["data"]
            want_c, want_r = o.match(don)
            got_c = [(c["recipientId"], c["accept"], c["travelMinutes"], c["unusedCapacity"]) for c in d["candidates"]]
            got_r = {x["recipientId"]: x["reason"] for x in d["rejected"]}
            if got_c != want_c or got_r != want_r:
                mism += 1; print("  parity mismatch", don["donation_id"], got_c, want_c)
            # ranks and gate arithmetic
            if [c["rank"] for c in d["candidates"]] != list(range(1, len(got_c) + 1)): mism += 1
            g = d["gates"]; ent = 26
            for x in g:
                if x["entering"] != ent: mism += 1
                ent -= x["rejected"]
            if ent != d["survivors"]: mism += 1
        check("parity: all %d donations equal the independent oracle (order/accept/travel/unused + rejection reasons)" % n, mism == 0 and n == 46, mism)
        # DON001 README example
        d = e.call("match.build", donationId="DON001")["data"]
        c = d["candidates"]
        check("DON001 R013 rank 1: 60 / 8 min / spare 190", (c[0]["recipientId"], c[0]["accept"], c[0]["travelMinutes"], c[0]["unusedCapacity"]) == ("R013", 60, 8, 190))
        check("DON001 R004 rank 2: 60 / 12 min / spare 20, on boundary", (c[1]["recipientId"], c[1]["accept"], c[1]["travelMinutes"], c[1]["unusedCapacity"], c[1]["onBoundary"]) == ("R004", 60, 12, 20, True))
        rj = {x["recipientId"]: x for x in d["rejected"]}
        check("DON001 reasons", all(rj[k]["reason"] == v for k, v in {"R017": "NOT_ACCEPTING", "R002": "FOOD_INCOMPATIBLE", "R020": "NO_CAPACITY", "R021": "NO_PICKUP", "R024": "UNREACHABLE", "R003": "TOO_FAR"}.items()))
        check("DON001 R003 TOO_FAR 22 vs 12", rj["R003"]["travelMinutes"] == 22 and rj["R003"]["overByMinutes"] == 10)
        check("DON001 unreachable has null travel and reachable=false", rj["R024"]["travelMinutes"] is None and rj["R024"]["reachable"] is False and rj["R024"]["route"] is None)
        seq = [26] + [x["entering"] - x["rejected"] for x in d["gates"]]
        check("DON001 gate funnel 26->23->17->15->13->11->2", seq == [26, 23, 17, 15, 13, 11, 2] or [d["gates"][0]["entering"]] + [x["entering"] - x["rejected"] for x in d["gates"]] == [26, 23, 17, 15, 13, 11, 2], seq)
        check("DON001 candidate route text", c[0]["routeText"] == "Karol Bagh Market -> Azadpur Chowk Junction")
        # separatedFromPrevious agrees with the engine's order for every adjacent pair of all donations
        bad = 0
        for don in o.dons:
            cs = e.call("match.build", donationId=don["donation_id"])["data"]["candidates"]
            for a, b in zip(cs, cs[1:]):
                key = "ACCEPT" if a["accept"] != b["accept"] else "TRAVEL" if a["travelMinutes"] != b["travelMinutes"] else "UNUSED" if a["unusedCapacity"] != b["unusedCapacity"] else "ID"
                if b["separatedFromPrevious"] != key: bad += 1
                if (-a["accept"], a["travelMinutes"], a["unusedCapacity"], int(a["recipientId"][1:])) > (-b["accept"], b["travelMinutes"], b["unusedCapacity"], int(b["recipientId"][1:])): bad += 1
        check("separatedFromPrevious consistent with the ranking order", bad == 0, bad)
        # match.compare: straight line baseline from the plan (DON001 picks R001 and R012, misses R013 and R004)
        cmp_ = e.call("match.compare", donationId="DON001")["data"]
        sl = cmp_["diff"]["straightline"]
        check("compare DON001: straight-line false positives include R001,R012 and misses R013,R004",
              {"R001", "R012"} <= set(sl["falsePositives"]) and {"R013", "R004"} <= set(sl["falseNegatives"]), sl)
    finally:
        e.close()

def test_preview_and_field():
    e = Eng(LAB)
    try:
        def ids(r): return [c["recipientId"] for c in r["data"]["candidates"]]
        want = {12: ["R004", "R013"], 11: ["R013"], 7: []}
        for use, w in want.items():
            r = e.call("match.preview", donorId="DN001", food="COOKED", quantity=60, usableMinutes=use)
            check("preview usable %d -> %s" % (use, w), r["ok"] and sorted(ids(r)) == w and r["data"]["donation"]["hypothetical"] is True, r)
        r = e.call("match.preview", node=16, food="PACKAGED", quantity=50, usableMinutes=30)
        check("preview from an island node works and is pure", r["ok"] and r["rev"] == 0)
        for bad, code in [(dict(donorId="DN999", food="COOKED", quantity=1, usableMinutes=1), "DONOR_NOT_FOUND"),
                          (dict(node=99, food="COOKED", quantity=1, usableMinutes=1), "NODE_NOT_FOUND"),
                          (dict(node=1, food="COOKED", quantity=0, usableMinutes=1), "BAD_QUANTITY"),
                          (dict(node=1, food="SOUP", quantity=1, usableMinutes=1), "BAD_ARGS")]:
            r = e.call("match.preview", **bad)
            check("preview refusal " + code, not r["ok"] and r["error"]["code"] == code, r)
        r = e.call("net.field", source=0)["data"]
        want_dist = [0, 24, 8, 20, 6, 25, 15, 22, 12, 36, 14, 38, 30, 42, 36, 31, None, None, None, None]
        check("net.field source 0 dist", r["dist"] == want_dist, r["dist"])
        check("net.field order and reachable count", r["order"] == [0, 4, 2, 8, 10, 6, 3, 7, 1, 5, 12, 15, 9, 14, 11, 13] and r["reachableCount"] == 16, r["order"])
        check("net.field prev is the engine's own", r["prev"][:5] == [-1, 3, 0, 6, 0], r["prev"])
        check("net.field bad source", e.call("net.field", source=99)["error"]["code"] == "NODE_NOT_FOUND")
        rt = e.call("net.route", **{"from": 0, "to": 5})["data"]
        check("net.route 0->5", rt["minutes"] == 25 and rt["nodes"] == [0, 2, 5] and [l["minutes"] for l in rt["legs"]] == [8, 17], rt)
        rt = e.call("net.route", **{"from": 0, "to": 16})["data"]
        check("net.route unreachable", rt["reachable"] is False and rt["minutes"] is None and rt["nodes"] == [])
        net = e.call("read.network")["data"]
        check("network: 20 nodes 25 edges 3 components", len(net["nodes"]) == 20 and len(net["edges"]) == 25 and len(net["components"]) == 3, (len(net["nodes"]), len(net["edges"])))
        n13 = net["nodes"][13]
        check("network: node 13 hosts DN007,R008,R014,R018", n13["donors"] == ["DN007"] and n13["recipients"] == ["R008", "R014", "R018"], n13)
        comps = {c["id"]: c for c in net["components"]}
        check("network: island B has no donor, island C has donor and recipient",
              sorted(c["nodeIds"] for c in comps.values())[0][0] == 0 and any(c["nodeIds"] == [16, 17] and not c["hasDonor"] and c["hasRecipient"] for c in comps.values())
              and any(c["nodeIds"] == [18, 19] and c["hasDonor"] and c["hasRecipient"] for c in comps.values()))
        q = e.call("read.queue")["data"]
        check("queue: 40 entries (open with food), urgency ordered", len(q["order"]) >= 40 and all(a["usableMinutes"] <= b["usableMinutes"] for a, b in zip(q["order"], q["order"][1:])) and q["nextId"] == q["order"][0]["donationId"], len(q["order"]))
        s = e.call("read.snapshot")
        check("snapshot shape", s["ok"] and len(s["data"]["donations"]) == 46 and len(s["data"]["recipients"]) == 26 and len(s["data"]["transactions"]) == 3 and s["data"]["offers"] == [] and s["data"]["summary"]["allocated"] == 200)
        h = e.call("sys.hello")["data"]
        check("hello counts and manifest", h["counts"]["donations"] == 46 and h["backendManifest"]["files"] > 40 and len(h["backendManifest"]["sha256"]) == 64, h)
        st = e.call("read.stats", role="recipient", id="R013")
        check("read.stats recipient", st["ok"] and st["data"]["received"] == 60, st)
    finally:
        e.close()

def snapshot_without_boot(e):
    s = e.call("read.snapshot")["data"]
    return s

def test_write_flow():
    d = fresh_copy("write_flow")
    e = Eng(d)
    try:
        before = snapshot_without_boot(e)
        r13 = [x for x in before["recipients"] if x["id"] == "R013"][0]
        check("R013 starts with capacity 250", r13["availableCapacity"] == 250)
        s = e.call("offer.send", donationId="DON001", recipientId="R013", quantity=40)
        check("offer.send ok, rev 1, OFF001", s["ok"] and s["rev"] == 1 and s["data"]["offer"]["id"] == "OFF001" and s["data"]["offer"]["status"] == "PENDING", s)
        check("offer.send message", "Offer OFF001 sent to" in s.get("message", ""), s.get("message"))
        fx = s["effects"]
        check("offer.send effects: new offer + donation pendingOffered", len(fx["offers"]) == 1 and fx["offers"][0]["after"]["quantity"] == 40 and
              fx["donations"][0]["after"] == {"pendingOffered": 40, "effectiveRemaining": 20}, fx)
        check("offers.csv created on disk", os.path.exists(os.path.join(d, "offers.csv")))
        m = e.call("match.build", donationId="DON001")["data"]
        check("match after offer uses effective remaining 20", m["donation"]["effectiveRemaining"] == 20 and m["candidates"][0]["accept"] == 20 and m["donation"]["pendingOffered"] == 40, m["donation"])
        a = e.call("offer.accept", offerId="OFF001")
        check("offer.accept ok rev 2 T004", a["ok"] and a["rev"] == 2 and a["data"]["transaction"]["id"] == "T004" and "recorded as T004" in a["message"], a)
        fx = a["effects"]
        don = fx["donations"][0]
        check("accept effects: donation remaining 60->20 PARTIAL", don["id"] == "DON001" and don["before"]["remaining"] == 60 and don["after"]["remaining"] == 20 and don["after"]["status"] == "PARTIAL", don)
        rec = fx["recipients"][0]
        check("accept effects: R013 availableCapacity 250->210", rec["id"] == "R013" and rec["before"]["availableCapacity"] == 250 and rec["after"]["availableCapacity"] == 210, rec)
        check("accept effects: offer ACCEPTED with txnId, new transaction row", fx["offers"][0]["after"]["status"] == "ACCEPTED" and fx["offers"][0]["after"]["txnId"] == "T004" and fx["transactions"][0]["id"] == "T004" and fx["transactions"][0]["after"]["quantity"] == 40, fx)
        after = snapshot_without_boot(e)
        check("snapshot reflects the accept", [x for x in after["recipients"] if x["id"] == "R013"][0]["availableCapacity"] == 210 and len(after["transactions"]) == 4)
        files = open(os.path.join(d, "transactions.csv")).read()
        check("transactions.csv on disk has T004", "T004,DON001,R013,40,8" in files, files)
        # reload keeps the same data, new bootId, rev 0
        old_boot = e.ready["bootId"]
        rl = e.call("sys.reload")
        check("sys.reload: rev reset and new bootId", rl["ok"] and rl["rev"] == 0 and rl["data"]["bootId"] != old_boot, rl)
        again = snapshot_without_boot(e)
        check("sys.reload shows the same data as before", again == after)
        n = e.call("offer.send", donationId="DON001", recipientId="R004", quantity=20)
        check("ids continue after reload (OFF002)", n["ok"] and n["data"]["offer"]["id"] == "OFF002", n)
        e.call("offer.withdraw", offerId="OFF002")
    finally:
        e.close()
    # restart from disk
    e2 = Eng(d)
    try:
        s2 = snapshot_without_boot(e2)
        check("restart from the saved CSVs gives the same recipients, donations and transactions",
              s2["recipients"] == after["recipients"] and s2["donations"] == after["donations"] and s2["transactions"] == after["transactions"])
        check("restart: offers persisted (OFF001 ACCEPTED, OFF002 WITHDRAWN)", [(o["id"], o["status"]) for o in s2["offers"]] == [("OFF001", "ACCEPTED"), ("OFF002", "WITHDRAWN")], s2["offers"])
        # second recipient of the leftover 20 after first accept and a declined recipient reappears as declined
        o3 = e2.call("offer.send", donationId="DON001", recipientId="R013", quantity=20)
        d3 = e2.call("offer.decline", offerId=o3["data"]["offer"]["id"])
        check("offer.decline ok", d3["ok"] and d3["data"]["offer"]["status"] == "DECLINED")
        m = e2.call("match.build", donationId="DON001")["data"]
        check("declined recipient moves to declined list", [x["recipientId"] for x in m["declined"]] == ["R013"] and "R013" not in [c["recipientId"] for c in m["candidates"]], m["declined"])
        nc = e2.call("offer.send", donationId="DON001", recipientId="R013", quantity=1)
        check("offer.send to earlier-decliner -> NOT_A_CANDIDATE DECLINED_EARLIER", not nc["ok"] and nc["error"]["code"] == "NOT_A_CANDIDATE" and nc["error"]["details"]["reason"] == "DECLINED_EARLIER", nc)
    finally:
        e2.close()

def test_refusals():
    d = fresh_copy("refusals")
    e = Eng(d)
    try:
        def code(r): return r["error"]["code"] if not r["ok"] else "OK"
        check("unknown offer -> OFFER_NOT_FOUND", code(e.call("offer.accept", offerId="OFF999")) == "OFFER_NOT_FOUND")
        check("unknown donation -> DONATION_NOT_FOUND", code(e.call("offer.send", donationId="DON999", recipientId="R013", quantity=1)) == "DONATION_NOT_FOUND")
        check("unknown recipient -> RECIPIENT_NOT_FOUND", code(e.call("offer.send", donationId="DON001", recipientId="R999", quantity=1)) == "RECIPIENT_NOT_FOUND")
        r = e.call("offer.send", donationId="DON001", recipientId="R017", quantity=1)
        check("rejected recipient -> NOT_A_CANDIDATE reason NOT_ACCEPTING", code(r) == "NOT_A_CANDIDATE" and r["error"]["details"]["reason"] == "NOT_ACCEPTING", r)
        r = e.call("offer.send", donationId="DON001", recipientId="R013", quantity=61)
        check("quantity above what the candidate can take -> QUANTITY_OUT_OF_RANGE {min,max}", code(r) == "QUANTITY_OUT_OF_RANGE" and r["error"]["details"] == {"min": 1, "max": 60}, r)
        check("quantity 0 -> QUANTITY_OUT_OF_RANGE", code(e.call("offer.send", donationId="DON001", recipientId="R013", quantity=0)) == "QUANTITY_OUT_OF_RANGE")
        check("quantity as string -> BAD_ARGS", code(e.call("offer.send", donationId="DON001", recipientId="R013", quantity="5")) == "BAD_ARGS")
        check("missing arg -> BAD_ARGS", code(e.call("offer.send", donationId="DON001")) == "BAD_ARGS")
        check("failed writes do not change rev", e.call("sys.ping")["rev"] == 0)
        # EXCEEDS_REMAINING: two offers whose sum is more than what is left
        e.call("offer.send", donationId="DON001", recipientId="R013", quantity=50)
        r = e.call("offer.send", donationId="DON001", recipientId="R004", quantity=20)
        check("second offer larger than what is left -> QUANTITY_OUT_OF_RANGE (candidate accept is 10)", code(r) == "QUANTITY_OUT_OF_RANGE" and r["error"]["details"]["max"] == 10, r)
        # offer already answered
        o = e.call("offer.send", donationId="DON001", recipientId="R004", quantity=10)["data"]["offer"]["id"]
        check("withdraw ok", e.call("offer.withdraw", offerId=o)["ok"])
        check("withdraw twice -> OFFER_ALREADY_ANSWERED", code(e.call("offer.withdraw", offerId=o)) == "OFFER_ALREADY_ANSWERED")
        check("accept a withdrawn offer -> OFFER_ALREADY_ANSWERED", code(e.call("offer.accept", offerId=o)) == "OFFER_ALREADY_ANSWERED")
        check("decline a withdrawn offer -> OFFER_ALREADY_ANSWERED", code(e.call("offer.decline", offerId=o)) == "OFFER_ALREADY_ANSWERED")
        # accept answered twice
        a1 = e.call("offer.accept", offerId="OFF001")
        check("accept ok", a1["ok"])
        check("accept twice -> OFFER_ALREADY_ANSWERED", code(e.call("offer.accept", offerId="OFF001")) == "OFFER_ALREADY_ANSWERED")
        # exceeds capacity at accept: offer to R004 (cap 80), then the recipient lowers its capacity
        e.call("offer.send", donationId="DON001", recipientId="R004", quantity=10)  # OFF00x for 10
        oid = [x for x in e.call("read.snapshot")["data"]["offers"] if x["status"] == "PENDING"][0]["id"]
        up = e.call("recipient.update", recipientId="R004", capacity=3)
        check("recipient.update capacity", up["ok"] and up["data"]["recipient"]["availableCapacity"] == 3 and up["effects"]["recipients"][0]["before"] == {"availableCapacity": 80}, up)
        chk = e.call("offer.check", offerId=oid)["data"]
        check("offer.check predicts EXCEEDS_CAPACITY", chk["wouldSucceed"] is False and chk["code"] == "EXCEEDS_CAPACITY" and chk["details"]["max"] == 3, chk)
        r = e.call("offer.accept", offerId=oid)
        check("accept above capacity -> EXCEEDS_CAPACITY max 3 and offer stays pending", code(r) == "EXCEEDS_CAPACITY" and r["error"]["details"]["max"] == 3 and
              [x["status"] for x in e.call("read.snapshot")["data"]["offers"] if x["id"] == oid] == ["PENDING"], r)
        check("recipient.update negative -> INVALID_CAPACITY", code(e.call("recipient.update", recipientId="R004", capacity=-1)) == "INVALID_CAPACITY")
        check("recipient.update without fields -> BAD_ARGS", code(e.call("recipient.update", recipientId="R004")) == "BAD_ARGS")
        up = e.call("recipient.update", recipientId="R004", accepting=False)
        check("recipient.update merges missing fields", up["ok"] and up["data"]["recipient"]["availableCapacity"] == 3 and up["data"]["recipient"]["accepting"] is False)
        # close donation withdraws pending offers
        cl = e.call("donation.close", donationId="DON001", )
        check("donation.close ok, wasted = remaining, effects show withdrawn offer", cl["ok"] and cl["data"]["donation"]["status"] == "CLOSED" and cl["data"]["wasted"] == 10 and
              any(x["after"]["status"] == "WITHDRAWN" for x in cl["effects"]["offers"]), cl)
        check("close twice -> DONATION_NOT_OPEN", code(e.call("donation.close", donationId="DON001")) == "DONATION_NOT_OPEN")
        check("send for a closed donation -> DONATION_NOT_OPEN", code(e.call("offer.send", donationId="DON001", recipientId="R013", quantity=1)) == "DONATION_NOT_OPEN")
        check("close unknown -> DONATION_NOT_FOUND", code(e.call("donation.close", donationId="DONXXX")) == "DONATION_NOT_FOUND")
        # creates
        r = e.call("donation.create", donorId="DN001", food="PACKAGED", quantity=25, usableMinutes=90)
        check("donation.create DON047", r["ok"] and r["data"]["donation"]["id"] == "DON047" and r["effects"]["donations"][0]["after"]["remaining"] == 25, r)
        check("donation.create bad donor", code(e.call("donation.create", donorId="DN999", food="COOKED", quantity=1, usableMinutes=1)) == "DONOR_NOT_FOUND")
        check("donation.create quantity 0", code(e.call("donation.create", donorId="DN001", food="COOKED", quantity=0, usableMinutes=1)) == "BAD_QUANTITY")
        check("donation.create minutes 0", code(e.call("donation.create", donorId="DN001", food="COOKED", quantity=1, usableMinutes=0)) == "INVALID_MINUTES")
        r = e.call("donor.create", name="Test, Donor", type="hotel", node=1)
        check("donor.create comma -> INVALID_NAME", code(r) == "INVALID_NAME")
        check("donor.create control char -> INVALID_NAME", code(e.call("donor.create", name="Bad\tName", type="hotel", node=1)) == "INVALID_NAME")
        check("donor.create bad type -> INVALID_TYPE", code(e.call("donor.create", name="Ok", type="ho!tel", node=1)) == "INVALID_TYPE")
        check("donor.create bad node -> INVALID_NODE", code(e.call("donor.create", name="Ok", type="hotel", node=77)) == "INVALID_NODE")
        r = e.call("donor.create", name="Ünïcode Bakery", type="corner shop", node=1)
        check("donor.create ok (UTF-8 name, type upper-cased)", r["ok"] and r["data"]["donor"]["type"] == "CORNER_SHOP" and r["data"]["donor"]["name"] == "Ünïcode Bakery", r)
        r = e.call("recipient.create", name="New Kitchen", type="kitchen", foods=[], capacity=10, accepting=True, pickup=True, node=1)
        check("recipient.create no foods -> NO_FOOD_TYPES", code(r) == "NO_FOOD_TYPES")
        r = e.call("recipient.create", name="New Kitchen", type="kitchen", foods=["COOKED", "COOKED"], capacity=10, accepting=True, pickup=True, node=1)
        check("recipient.create ok R027", r["ok"] and r["data"]["recipient"]["id"] == "R027" and r["data"]["recipient"]["acceptedTypes"] == ["COOKED"], r)
        r = e.call("recipient.create", name="Island Pantry", type="pantry", foods=["COOKED"], capacity=5, accepting=True, pickup=True, node=16)
        check("recipient.create on an island without donor carries a warning", r["ok"] and "no donor is connected" in r["data"].get("warning", ""), r)
        r = e.call("donation.createWithNewDonor", donorName="Fresh Donor", donorType="cafe", node=0, food="BAKERY", quantity=5, usableMinutes=30)
        check("donation.createWithNewDonor ok with donor and donation rows", r["ok"] and r["data"]["donor"]["id"] == "DN014" and r["data"]["donation"]["donorId"] == "DN014" and len(r["effects"]["donors"]) == 1, r)
        r = e.call("donation.createWithNewDonor", donorName="x", donorType="cafe", node=0, food="BAKERY", quantity=0, usableMinutes=30)
        check("createWithNewDonor bad quantity saves nothing", code(r) == "BAD_QUANTITY" and e.call("sys.ping")["data"]["pong"])
        # a "mutated" file must still load
        e2 = Eng(d)
        try:
            s = e2.call("read.snapshot")["data"]
            check("files written by all the writes load again", any(x["id"] == "DN013" for x in s["donors"]) and any(x["id"] == "DON047" for x in s["donations"]) and len(s["recipients"]) == 28)
        finally:
            e2.close()
        check("unknown command -> UNKNOWN_COMMAND", code(e.call("nope.nope")) == "UNKNOWN_COMMAND")
    finally:
        e.close()

def test_classifier():
    e = Eng(LAB)
    try:
        sources = [os.path.join(REPO, "desktop-app", "src", "Workflow.cpp"), os.path.join(REPO, "desktop-app", "src", "RecipientUpdate.cpp")]
        samples = []
        for p in sources:
            text = open(p, encoding="utf-8", errors="replace").read()
            for stmt in re.findall(r"\bmessage\s*=\s*(.*?);", text, re.S):
                parts = [x.strip() for x in re.split(r'\+(?=(?:[^"]*"[^"]*")*[^"]*$)', stmt)]
                if not parts or not parts[0].startswith('"'):
                    continue
                s = "".join(x[1:-1] if x.startswith('"') and x.endswith('"') else "7" for x in parts)
                if s: samples.append(s)
        samples = sorted(set(samples))
        success = re.compile(r"^(Offer OFF\d+7|Offer 7|The offer was (declined|withdrawn)|The donation was closed|Donation 7|Recipient 7|Donor 7|Your details were saved|Offer 7 sent to 7)")
        allow_success = [s for s in samples if success.match(s) or s.startswith("The offer was accepted")]
        unknown = []
        for s in samples:
            if s in allow_success: continue
            r = e.call("sys.classify", message=s)
            if r["data"]["code"] == "OPERATION_REFUSED": unknown.append(s)
        check("classifier: %d desktop app message literals found, none unclassified" % len(samples), len(samples) > 20 and not unknown, unknown)
        r = e.call("sys.classify", message="something nobody wrote")
        check("classifier: unknown message -> OPERATION_REFUSED", r["data"]["code"] == "OPERATION_REFUSED")
        for msg, want in [("The donation is not open it is completed or closed so the offer was withdrawn", "DONATION_NOT_OPEN"),
                          ("The quantity is more than the capacity of the recipient", "EXCEEDS_CAPACITY"),
                          ("The recipient is no longer feasible for this donation", "NOT_FEASIBLE"),
                          ("The quantity is more than what is left of the donation", "EXCEEDS_REMAINING")]:
            check("classifier alloc text -> " + want, e.call("sys.classify", message=msg)["data"]["code"] == want, msg)
    finally:
        e.close()

def test_boot_failure():
    d = fresh_copy("bad_boot")
    with open(os.path.join(d, "donations.csv"), "a") as f: f.write("DON999,DN404,COOKED,1,1,1,PENDING\n")
    p = subprocess.run(ENGINE + ["--data", d], input=b"", capture_output=True, timeout=20)
    ev = json.loads(p.stdout.decode().splitlines()[0])
    check("bad data: boot_failed event and exit code 2", ev.get("event") == "boot_failed" and p.returncode == 2 and ev["error"]["code"] == "DATA_INVALID", (p.returncode, p.stdout[:200]))
    p = subprocess.run(ENGINE, capture_output=True, timeout=20)
    check("no args: exit code 64", p.returncode == 64)
    p = subprocess.run(ENGINE + ["--data", os.path.join(WORK, "does_not_exist")], capture_output=True, timeout=20)
    check("missing folder: exit code 2", p.returncode == 2, p.returncode)

def main():
    print("engine command:", " ".join(ENGINE))
    if ENGINE == [EXE] and not os.path.exists(EXE):
        print("cb_engine.exe not found at", EXE, "- run scripts\\build-engine.ps1"); return 2
    os.makedirs(WORK, exist_ok=True)
    t0 = time.time()
    for name, fn in [("json+robustness", test_json_and_robustness), ("parity", test_parity), ("preview+field+reads", test_preview_and_field),
                     ("write flow", test_write_flow), ("refusals+creates", test_refusals), ("classifier", test_classifier), ("boot failure", test_boot_failure)]:
        before = (passed, failed)
        try:
            fn()
        except Exception as ex:
            import traceback; traceback.print_exc()
            check(name + " raised an exception", False, ex)
        print("  %-22s +%d pass +%d fail" % (name, passed - before[0], failed - before[1]))
    shutil.rmtree(WORK, ignore_errors=True)
    print("ENGINE TESTS: %d PASS, %d FAIL (%.1fs)" % (passed, failed, time.time() - t0))
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
