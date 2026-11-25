# delhi-real: scenario notes

An **imitation of Delhi**: real roads (all of NCT Delhi, main road classes) and real place locations from OpenStreetMap;
organisation names, capacities, accepted food, flags, donations and history are simulated with a fixed seed (20261125).
Rebuild: `python -I tools/realmap/build_delhi_real.py --offline` (byte-identical from the cache).

## Counts
- Graph: **2062 nodes, 3609 edges**, one connected component; edge minutes histogram {1: 988, 2: 1220, 3: 648, 4: 349, 5: 145, 6: 105, 7: 46, 8: 34, 9: 22, 10: 14, 11: 11, 12: 10, 13: 8, 14: 1, 15: 3, 17: 2, 18: 1, 20: 1, 27: 1}.
- Organisations: **60 donors** {'HOTEL': 17, 'MARKET': 12, 'BAKERY': 13, 'CATERER': 8, 'CANTEEN': 10}; **140 recipients** {'SHELTER': 5, 'FOOD_BANK': 1, 'COMMUNITY_KITCHEN': 52, 'CARE_HOME': 15, 'CHILDRENS_HOME': 3, 'COMMUNITY_CENTRE': 52, 'RELIEF_POINT': 12}.
- Donations: **200** {'CLOSED': 8, 'COMPLETED': 10, 'PARTIAL': 12, 'PENDING': 170}; **28 transactions**.
- Snap distance place -> coarse node: median 218 m, p95 582 m, max 850 m (limit 1000 m).
- Files: donations.csv 7,870 B, donors.csv 2,606 B, geo_overlay.json 835,242 B, graph_edges.csv 39,045 B, graph_nodes.csv 97,772 B, recipients.csv 11,975 B, transactions.csv 701 B.

## 1. Road network (fine graph)
Overpass returned 14181 way pieces after clipping to the NCT boundary (motorway 105.1, trunk 409.2, primary 450.8, secondary 1170.5, tertiary 2292.4, motorway_link 19.7, trunk_link 100.3, primary_link 50.8, secondary_link 63.2, tertiary_link 15.0 km). Junctions = way endpoints and nodes
shared by two or more ways (16253 junctions, 22251 segments). Segment length = sum of haversine
distances between consecutive OSM nodes. Roads are treated as **two-way** (the engine graph is undirected; `oneway` is ignored),
turn restrictions and signals are ignored. 28 connected components; the largest is kept (4664.6 km), the
others are dropped: 66 junctions, 12.5 km (mostly short unconnected pieces, e.g.
(unnamed tertiary) x30, (unnamed secondary) x4, Loni Road x2, (unnamed motorway) x1, Road No 56 x1). Degree-2 chains are contracted: 10499 nodes,
16520 chains.

**Speed assumption** (average urban speed including congestion, km/h; an assumption, not measured, no live traffic):

| class | km/h |
|---|---|
| motorway | 45 |
| trunk | 35 |
| primary | 28 |
| secondary | 22 |
| tertiary | 18 |
| motorway_link | 30 |
| trunk_link | 25 |
| primary_link | 20 |
| secondary_link | 18 |
| tertiary_link | 15 |

## 2. Coarsening for the engine (integer minutes >= 1)
Cluster centres are chosen greedily (major-road junctions first, then higher degree, then OSM id) so that no two centres are
closer than the spacing; every junction joins the centre that is nearest **along the roads** (multi-source Dijkstra), so each
cluster is a connected piece of road. Two clusters are joined when a road chain connects them; the edge's minutes =
round(shortest fine-grained time between the two centre junctions / 60), minimum 1. The node position is the centre junction's
real OSM position (`confidence` = `verified` when the cluster is that single junction, else `approx` with the cluster size in
`note`). Spacing used: **300 m** (430 single-junction nodes).

Error measurement: 500 random node pairs (50 sources x 10 targets, seeded), fine-grained shortest time vs engine-style
integer shortest time: **median abs 1.74 min, p95 abs 5.5 min; median relative
3.9%, p95 relative 11.0%**; median signed relative 3.7% (the coarse
graph is slightly slower: routes are forced through centre junctions and short edges are rounded up to 1 minute). Pairs
under 15 fine minutes: 21 (median relative 4.0%). Fine travel times:
median 48.9 min, max 116.9 min.

Tuning (`--tune`, each row its own seeded sample):

| min centre spacing (m) | nodes | edges | median abs (min) | p95 abs (min) | median rel | p95 rel | median signed rel |
|---|---|---|---|---|---|---|---|
| 200 | 2589 | 4332 | 2.24 | 6.18 | 5.1% | 11.9% | 5.0% |
| 250 | 2317 | 3965 | 2.28 | 6.79 | 5.3% | 11.7% | 5.1% |
| 300 | 2062 | 3609 | 1.74 | 5.5 | 3.9% | 11.0% | 3.7% |
| 350 | 1826 | 3263 | 1.68 | 5.63 | 3.9% | 11.7% | 3.5% |
| 400 | 1634 | 2966 | 1.93 | 6.0 | 4.1% | 15.5% | 3.9% |
| 500 | 1342 | 2498 | 2.26 | 8.02 | 4.9% | 14.0% | 4.7% |
| 700 | 939 | 1799 | 3.26 | 9.39 | 5.9% | 19.1% | 5.9% |
| 1000 | 604 | 1212 | 4.62 | 10.48 | 8.0% | 19.1% | 8.0% |

Larger spacing gives fewer nodes but more detour error; spacings below ~300 m add many 1-minute edges (rounding up). 300 m was
picked as the best measured error inside the 1,500-4,000 node target.

## 3. Places (real locations, imitation names)
OSM places found inside Delhi (role|type, `far` = more than 1 km from any coarse node, dropped):

| role type | count |
|---|---|
| D BAKERY | 140 |
| D BAKERY far | 1 |
| D CANTEEN | 29 |
| D CATERER | 8 |
| D HOTEL | 448 |
| D HOTEL far | 3 |
| D MARKET | 113 |
| D MARKET far | 1 |
| D RESTAURANT | 1033 |
| D RESTAURANT far | 5 |
| R CARE_HOME | 15 |
| R CHILDRENS_HOME | 3 |
| R COMMUNITY_CENTRE | 113 |
| R COMMUNITY_CENTRE far | 2 |
| R COMMUNITY_KITCHEN | 111 |
| R FOOD_BANK | 1 |
| R RELIEF_POINT | 12 |
| R SHELTER | 5 |

Type mapping: tourism=hotel -> HOTEL; craft=caterer / shop=catering -> CATERER; amenity=marketplace -> MARKET; shop=bakery ->
BAKERY; amenity=canteen / food_court -> CANTEEN; amenity=food_bank or social_facility=food_bank -> FOOD_BANK; Sikh place of
worship (gurdwara, langar) and social_facility=soup_kitchen -> COMMUNITY_KITCHEN; social_facility for children/orphans ->
CHILDRENS_HOME; social_facility=shelter or homeless -> SHELTER; nursing_home / assisted_living / group_home / senior ->
CARE_HOME; other social_facility -> RELIEF_POINT; amenity=community_centre -> COMMUNITY_CENTRE. Restaurants were fetched as a
reserve and not needed. Sampling: per type, round-robin over ~4 km cells in seeded order (spread across the city). Shortfalls
(OSM has fewer mapped places than the quota; spare quota moved to types with more places): D CATERER: only 8 usable OSM places (quota 10); R SHELTER: only 5 usable OSM places (quota 22); R FOOD_BANK: only 1 usable OSM places (quota 8); R CARE_HOME: only 15 usable OSM places (quota 18); R CHILDRENS_HOME: only 3 usable OSM places (quota 14); R RELIEF_POINT: only 12 usable OSM places (quota 18).
So SHELTER, FOOD_BANK and CHILDRENS_HOME are rare because they are rarely mapped in OSM Delhi, not because they are rare in reality.

Names: "<Type> Â· <locality> #n" where locality = nearest OSM suburb/neighbourhood/quarter with an ASCII name within 3 km
(else "Delhi"). Real organisation names are never used.

## 4. Synthetic attributes (seeded, assumptions)
| recipient type | capacity (uniform, step 5) | accepted food (base) |
|---|---|---|
| SHELTER | 40-200 | COOKED;BAKERY |
| FOOD_BANK | 200-800 | PACKAGED;PRODUCE |
| COMMUNITY_KITCHEN | 150-600 | PRODUCE;PACKAGED |
| CARE_HOME | 20-80 | COOKED;PRODUCE |
| CHILDRENS_HOME | 20-100 | COOKED;BAKERY;PACKAGED |
| COMMUNITY_CENTRE | 50-250 | COOKED;PACKAGED |
| RELIEF_POINT | 50-300 | COOKED;PACKAGED |

Per recipient: 15% drop one accepted type, 15% add one other type; 3% capacity 0; accepting 90% yes, can_pickup 85% yes
(independent draws). Donations: every donor gets one, the rest are random donors. Food by donor type: HOTEL: COOKED 60%, BAKERY 20%, PACKAGED 20%; CATERER: COOKED 85%, PACKAGED 15%; MARKET: PRODUCE 80%, PACKAGED 20%; BAKERY: BAKERY 90%, PACKAGED 10%; CANTEEN: COOKED 70%, PACKAGED 20%, BAKERY 10%. Quantity
(portions, uniform, step 5; 3% are x5): COOKED 20-150, BAKERY 20-200, PRODUCE 20-300, PACKAGED 20-400.

**usable_minutes** (tried in order; the first variant with a zero-candidate share in 10-30% is used):

| variant | ranges (minutes) | zero-candidate share | median candidates |
|---|---|---|---|
| V0 uniform | COOKED 30-180, BAKERY 120-600, PRODUCE 240-1440, PACKAGED 1440-10080 | 0.0% | 54 |
| V1 log-uniform | COOKED 15-180, BAKERY 60-600, PRODUCE 240-1440, PACKAGED 1440-10080 | 0.5% | 44 |
| V2 log-uniform | COOKED 10-120, BAKERY 30-480, PRODUCE 120-1440, PACKAGED 1440-10080 | 0.5% | 33 |
| V3 log-uniform | COOKED 6-90, BAKERY 15-360, PRODUCE 60-1440, PACKAGED 720-10080 | 4.9% | 17 |
| V4 log-uniform | COOKED 4-60, BAKERY 10-240, PRODUCE 30-1440, PACKAGED 480-10080 | 11.0% | 10 |

Used: **V4 log-uniform** (log). With the brief's suggested ranges (V0) every open donation has feasible
recipients (0%), because 140 recipients on a dense road graph put a compatible recipient within a few minutes of almost
every donor. Reaching the 10-30% target needed short cooked/bakery windows (cooked down to a few minutes = food that must be
eaten almost at once) plus narrower accepted-food lists. This is a tuning choice for the demo, not a fact about Delhi.

History: 12 PARTIAL, 10 COMPLETED, 5 CLOSED with transactions and 3 CLOSED without. Each transaction goes to the engine's rank-1
candidate at that moment (same six rules and ranking), quantity <= that recipient's capacity, capacity reduced afterwards
(`available_capacity` is remaining capacity), `travel_minutes` = shortest integer travel time. Transactions sum to
original - remaining for every donation; ids ascend.

## 5. Matching statistics (build's own rules implementation; the oracle agrees, see 6)
Open donations 182; zero candidates 20 (11.0%) by food {'BAKERY': 6, 'COOKED': 14} (open by food
{'BAKERY': 51, 'COOKED': 75, 'PACKAGED': 33, 'PRODUCE': 23}); candidates per donation median 10, max 66, buckets {'31+': 66, '4-10': 49, '1-3': 24, '0': 20, '11-30': 23};
boundary passes (travel == usable): 63; rejection reasons {'NOT_ACCEPTING': 4550, 'FOOD_INCOMPATIBLE': 10541, 'OK': 4453, 'NO_PICKUP': 1987, 'NO_CAPACITY': 502, 'TOO_FAR': 3447}.

## 6. Validation
Result of the last `validate_delhi_real.py` run (stored in `tools/realmap/output/validation.json`):
- Static checks (loader rules from `console-app/src/Datamanager.cpp`, overlay agreement, transaction travel = shortest integer
  path): 0 errors, 0 warnings; graph components: 1.
- Native engine `engine/build/cb_engine.exe --data <temp copy> --self-test`: exit 0, ok =
  True, invariant problems [], 182 matches built for
  182 open donations, wall time [2.206, 1.988, 2.037] s (3 runs, includes boot + all matches).
  Plain boot to the `ready` event: 0.099 s.
- 100 x `match.build` over stdio (one process, Python client, includes JSON parsing): p50 171.54 ms,
  p95 246.83 ms, max 282.76 ms; response size median
  137852 bytes, max 227574 bytes (every rejected
  recipient carries a route). `read.snapshot` 66742 bytes, `read.network` 369626 bytes.
- Engine candidate counts vs an independent rules re-implementation: 182 open
  donations, 0 mismatches. Engine `net.route` minutes for all
  28 transactions equal `travel_minutes`: bad =
  [].
- `tools/oracle/oracle.py`: open 182, zero-candidate donations 20 (11.0%),
  boundary passes (travel == usable) 63, reasons {'NOT_ACCEPTING': 4550, 'FOOD_INCOMPATIBLE': 10541, 'NO_PICKUP': 1987, 'NO_CAPACITY': 502, 'TOO_FAR': 3447}.

## 7. Engine size limits (read in console-app source, then tested)
`hashindex` rehashes at load 0.75 with no cap; `graph` and the heap/stack/queue structures grow by doubling; ids need >= 3 digits
(T001...). `labelcomponents` is a **recursive** DFS (depth up to the component size; 2062 nodes loaded fine in the
self-test, but a much larger graph could overflow the 32-bit exe's stack - not tested beyond this size). The engine was not
modified. Practical cost of the size: `match.build` is ~100x slower than on `lab` and its response is ~100-200 KB, because
every rejected recipient carries a full route.

## 8. Limitations / not verified
- Speeds are assumptions; no turn costs, signals, one-way rules or traffic. The coarse graph overestimates by about
  3.7% at the median.
- OSM completeness: many real shelters/food banks/children's homes are not mapped; place kinds come from OSM tags as mapped.
- The organisations, their capacities, food rules and all donations are simulated; locations are real places of that kind,
  but the organisation at that spot does not take part in anything.
- `x_km`/`y_km`: equirectangular projection around lat0/lng0 = mean of the contracted junctions, y grows **south**.
