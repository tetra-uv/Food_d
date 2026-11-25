"""Latency of match.build over 1000 round trips through the pipe, and snapshot size. Run: python -I engine/tests/bench.py"""
import json, os, subprocess, time
H = os.path.dirname(os.path.abspath(__file__)); E = os.path.dirname(H); W = os.path.dirname(E)
exe = os.path.join(E, "build", "cb_engine.exe"); lab = os.path.join(W, "data", "scenarios", "lab")
p = subprocess.Popen([exe, "--data", lab], stdin=subprocess.PIPE, stdout=subprocess.PIPE); p.stdout.readline()
def call(i, cmd, **a):
    p.stdin.write((json.dumps({"id": i, "cmd": cmd, "args": a}) + "\n").encode()); p.stdin.flush()
    return p.stdout.readline()
ids = ["DON%03d" % (1 + (k % 46)) for k in range(1000)]
lat = []
for k, d in enumerate(ids):
    t = time.perf_counter(); call(k, "match.build", donationId=d); lat.append((time.perf_counter() - t) * 1000)
lat.sort()
print("match.build x1000: p50 %.3f ms  p95 %.3f ms  p99 %.3f ms  max %.3f ms" % (lat[500], lat[950], lat[990], lat[-1]))
lat = []
for k in range(1000):
    t = time.perf_counter(); call(k, "sys.ping"); lat.append((time.perf_counter() - t) * 1000)
lat.sort(); print("sys.ping x1000:    p50 %.3f ms  p95 %.3f ms" % (lat[500], lat[950]))
r = call(1, "read.snapshot"); print("read.snapshot: %.1f KB (%d bytes)" % (len(r) / 1024, len(r)))
r = call(2, "read.network"); print("read.network:  %.1f KB" % (len(r) / 1024))
r = call(3, "match.build", donationId="DON001"); print("match.build DON001: %.1f KB" % (len(r) / 1024))
lat = []
for k in range(200):
    t = time.perf_counter(); call(k, "read.snapshot"); lat.append((time.perf_counter() - t) * 1000)
lat.sort(); print("read.snapshot x200: p50 %.3f ms p95 %.3f ms" % (lat[100], lat[190]))
p.stdin.close(); p.wait()
