"""osm_fetch.py - cache-first Overpass access for build_delhi_real.py (Python 3.12 standard library only).

Every successful response is stored as cache/<name>-<sha1(query)[:12]>.json (raw body) plus a .meta.json
(query, endpoint, UTC fetch time, byte size). A rerun never touches the network when the cache file exists.
Failures are never cached. Fair use: one request at a time, generic User-Agent, exponential backoff on 429/502/504.
"""
from __future__ import annotations

import hashlib
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
CACHE = HERE / "cache"
ENDPOINT = "https://overpass-api.de/api/interpreter"
USER_AGENT = "cb-realmap/1.0"

# The NCT of Delhi administrative boundary (ISO 3166-2 code IN-DL, admin_level 4) as an Overpass area.
AREA = 'area["ISO3166-2"="IN-DL"]["admin_level"="4"]->.a;'

QUERIES = {
    "roads": "[out:json][timeout:600];" + AREA +
             'way(area.a)["highway"~"^(motorway|trunk|primary|secondary|tertiary)(_link)?$"];'
             "(._;>;);out body qt;",
    "places": "[out:json][timeout:300];" + AREA + "("
              'nwr(area.a)["tourism"="hotel"];'
              'nwr(area.a)["amenity"="marketplace"];'
              'nwr(area.a)["shop"="bakery"];'
              'nwr(area.a)["craft"="caterer"];'
              'nwr(area.a)["shop"="catering"];'
              'nwr(area.a)["amenity"="canteen"];'
              'nwr(area.a)["amenity"="food_court"];'
              'nwr(area.a)["amenity"="social_facility"];'
              'nwr(area.a)["amenity"="food_bank"];'
              'nwr(area.a)["social_facility"="food_bank"];'
              'nwr(area.a)["amenity"="place_of_worship"]["religion"="sikh"];'
              'nwr(area.a)["amenity"="community_centre"];'
              'nwr(area.a)["amenity"="restaurant"];'
              ");out center tags qt;",
    "boundary": '[out:json][timeout:180];rel["ISO3166-2"="IN-DL"]["admin_level"="4"];out geom;',
    "localities": "[out:json][timeout:180];" + AREA +
                  'node(area.a)["place"~"^(suburb|neighbourhood|quarter)$"];out qt;',
}


def cache_paths(name: str) -> tuple[Path, Path]:
    key = hashlib.sha1(QUERIES[name].encode("utf-8")).hexdigest()[:12]
    return CACHE / f"{name}-{key}.json", CACHE / f"{name}-{key}.meta.json"


def fetch(name: str, offline: bool, max_tries: int = 8) -> tuple[dict, dict]:
    body_path, meta_path = cache_paths(name)
    if body_path.exists() and meta_path.exists():
        return json.loads(body_path.read_text(encoding="utf-8")), json.loads(meta_path.read_text(encoding="utf-8"))
    if offline:
        raise RuntimeError(f"--offline and no cache entry for query {name!r} ({body_path.name})")
    CACHE.mkdir(parents=True, exist_ok=True)
    data = urllib.parse.urlencode({"data": QUERIES[name]}).encode("ascii")
    wait = 30
    for attempt in range(1, max_tries + 1):
        req = urllib.request.Request(ENDPOINT, data=data, headers={"User-Agent": USER_AGENT})
        try:
            t0 = time.time()
            with urllib.request.urlopen(req, timeout=900) as resp:
                raw = resp.read()
            text = raw.decode("utf-8")
            parsed = json.loads(text)
            remark = parsed.get("remark", "")
            if remark and ("runtime error" in remark.lower() or "timed out" in remark.lower()):
                raise RuntimeError("Overpass remark: " + remark)
            body_path.write_text(text, encoding="utf-8")
            meta = {"name": name, "endpoint": ENDPOINT, "query": QUERIES[name], "user_agent": USER_AGENT,
                    "fetched_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                    "bytes": len(raw), "seconds": round(time.time() - t0, 1),
                    "osm_base": parsed.get("osm3s", {}).get("timestamp_osm_base", ""),
                    "area_base": parsed.get("osm3s", {}).get("timestamp_areas_base", ""),
                    "elements": len(parsed.get("elements", []))}
            meta_path.write_text(json.dumps(meta, indent=1), encoding="utf-8")
            print(f"[fetch] {name}: {meta['elements']} elements, {len(raw)} bytes, {meta['seconds']} s", flush=True)
            return parsed, meta
        except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError, RuntimeError, json.JSONDecodeError,
                ConnectionError, OSError) as exc:
            print(f"[fetch] {name}: attempt {attempt}/{max_tries} failed: {exc!r}; waiting {wait} s", flush=True)
            if attempt == max_tries:
                raise
            time.sleep(wait)
            wait = min(wait * 2, 600)
    raise RuntimeError("unreachable")


if __name__ == "__main__":
    import sys
    for n in (sys.argv[1:] or list(QUERIES)):
        fetch(n, offline=False)
        time.sleep(5)
