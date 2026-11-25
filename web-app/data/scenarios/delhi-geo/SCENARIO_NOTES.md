# delhi-geo: scenario notes

## Method
1. Each of the 20 node labels was geocoded with Nominatim (OSM), bounded to a Delhi viewbox. Raw hits were read by hand; the chosen
   hit index and a required text fragment are pinned in `NODE_SPECS` of `tools/geo/build_delhi_pack.py`. Labels are generic
   ("... Crossing", "... Junction"), so most nodes sit on a nearby named OSM feature (metro station, locality centroid) rather than on
   the exact junction. Only names that match the label's landmark are marked `verified`.
2. For each of the 25 edge pairs of the lab scenario (same u,v rows, same order) one OSRM request (driving profile, u to v) gave duration
   and geometry. Minutes = ceil(duration / 60), minimum 1. Geometry is Douglas-Peucker simplified to at most 60 points.
   The engine treats roads as undirected; only the u-to-v direction was routed, so one-way streets can make v-to-u slightly different.
3. `graph_nodes.csv`: same ids and labels; `x_km`,`y_km` derived from lat/lng with origin at the centroid of nodes 0-15
   (lat0=28.639898, lng0=77.227940): `x=(lng-lng0)*111.32*cos(lat0)`, `y=-(lat-lat0)*111.32`, 2 decimals, y grows south.
4. donors, recipients, transactions copied byte-for-byte from `lab`. Nodes 16-19 keep their lab edges only (16-17 and 18-19), so the four
   isolated districts stay disconnected from the main cluster.

## Edge sources
- minutesSource: **osrm**; edges from OSRM: **25**, edges from the haversine estimate: **0**
  (estimate = straight line x 1.4 / 25 km/h, ceil, min 1; such edges carry `"minutesSource":"estimate"` in the overlay).
- OSRM waypoint snapping: the largest distance between a pin and the road it was snapped to was 28 m.
  Edges with a snap above 250 m: none.

## Minutes: lab (simulated) vs delhi-geo (OSRM), min / median / max
| Set | lab | delhi-geo |
|-----|-----|-----------|
| all 25 edges | 4 / 9 / 33 | 5 / 13 / 29 |
| main cluster (nodes 0-15), 23 edges | 4 / 9 / 33 | 5 / 13 / 29 |
| isolated districts, 2 edges | 5 / 6 / 7 | 8 / 8.5 / 9 |

## Per-edge table
| edge | lab min | real min | source | route km | straight km | max snap m | geometry pts (kept/raw) |
|------|---------|----------|--------|----------|-------------|------------|-------------------------|
| 0-2 | 8 | 13 | osrm | 9.7 | 7.1 | 21 | 33/294 |
| 2-5 | 17 | 13 | osrm | 10.1 | 8.0 | 21 | 42/338 |
| 0-4 | 6 | 14 | osrm | 10.7 | 7.8 | 21 | 28/244 |
| 4-6 | 9 | 9 | osrm | 6.3 | 4.6 | 21 | 19/120 |
| 0-8 | 12 | 10 | osrm | 8.3 | 6.4 | 13 | 17/191 |
| 2-7 | 14 | 18 | osrm | 14.3 | 11.3 | 21 | 42/349 |
| 6-3 | 5 | 5 | osrm | 3.4 | 2.2 | 9 | 16/97 |
| 3-7 | 7 | 14 | osrm | 11.9 | 6.9 | 16 | 25/238 |
| 0-3 | 25 | 8 | osrm | 6.2 | 5.4 | 6 | 23/188 |
| 1-3 | 4 | 7 | osrm | 6.5 | 5.5 | 6 | 15/136 |
| 1-7 | 9 | 8 | osrm | 5.1 | 3.6 | 16 | 16/128 |
| 4-10 | 8 | 12 | osrm | 8.2 | 4.5 | 26 | 28/170 |
| 2-10 | 6 | 23 | osrm | 17.6 | 14.6 | 26 | 59/630 |
| 5-9 | 11 | 18 | osrm | 15.2 | 11.2 | 28 | 31/319 |
| 9-13 | 6 | 16 | osrm | 13.4 | 10.5 | 28 | 38/304 |
| 13-14 | 10 | 14 | osrm | 11.7 | 9.9 | 10 | 27/241 |
| 14-11 | 13 | 29 | osrm | 24.6 | 22.0 | 20 | 56/527 |
| 11-12 | 8 | 18 | osrm | 14.5 | 11.7 | 20 | 30/335 |
| 12-15 | 9 | 14 | osrm | 9.8 | 7.8 | 19 | 44/293 |
| 15-6 | 16 | 11 | osrm | 7.4 | 6.0 | 9 | 34/215 |
| 10-14 | 22 | 21 | osrm | 17.2 | 14.0 | 26 | 35/371 |
| 8-12 | 18 | 13 | osrm | 10.6 | 9.0 | 19 | 22/275 |
| 1-9 | 33 | 8 | osrm | 5.9 | 4.6 | 28 | 17/142 |
| 16-17 | 7 | 8 | osrm | 5.7 | 3.7 | 11 | 20/106 |
| 18-19 | 5 | 9 | osrm | 7.2 | 5.7 | 17 | 20/127 |

## Per-node confidence
`verified` = the OSM feature's own name matches the landmark named in the label. `approx` = proxy feature, locality centroid or
ambiguous name. Two nodes are genuinely unsure: **12 (Ram Nagar, ambiguous name)** and **19 (Dwarka, no sector given)**; node 17 is a judgement call.
| id | label | lat | lng | confidence | OSM object | note |
|----|-------|-----|-----|------------|------------|------|
| 0 | Karol Bagh Market | 28.644012 | 77.188524 | approx | node/345234441 (railway/station) | Karol Bagh metro station used as proxy: Nominatim did not return the market itself (a query for 'Karol Bagh Market' gave only a restaurant in 'new market, karol bagh', 1.5 km north of the station). Distance to the real market not verified. |
| 1 | Preet Vihar Crossing | 28.641418 | 77.295282 | approx | node/9415047899 (railway/station) | Preet Vihar metro station on Vikas Marg as proxy for the 'crossing'; no mapped junction named in the label. |
| 2 | Azadpur Chowk Junction | 28.707045 | 77.180372 | approx | node/6453992652 (railway/station) | Azadpur metro station (Yellow Line) as proxy for Azadpur Chowk; 'Azadpur Chowk' itself returned no Nominatim hit. |
| 3 | ITO Junction | 28.628191 | 77.241044 | verified | node/9584236508 (railway/station) | OSM station named 'ITO' on Bahadur Shah Zafar Marg; name matches the label's landmark. Exact junction centre not separately checked. |
| 4 | Nizamuddin Bridge Junction | 28.603872 | 77.253581 | approx | node/6362085189 (highway/bus_stop) | OSM bus stop named 'Nizamuddin Road Bridge' on Mahatma Gandhi Marg; the exact junction is not mapped as a named point. |
| 5 | Paharganj Circle | 28.641499 | 77.214061 | approx | node/410058536 (place/suburb) | Centroid of the Paharganj locality (OSM place=suburb); 'Paharganj Circle' not found as a named feature. |
| 6 | Connaught Place Inner Circle | 28.631770 | 77.219380 | verified | node/561299050 (place/suburb) | OSM place named 'Connaught Place' (suburb centroid); name matches the label. The Inner Circle itself was not separately located. |
| 7 | Shahdara Crossing | 28.673430 | 77.289886 | approx | node/663758998 (railway/station) | Shahdara railway station as proxy for 'Shahdara Crossing'. Shahdara is a large area; the crossing itself is not pinned. |
| 8 | Rajouri Garden Junction | 28.649056 | 77.122720 | approx | node/6453992649 (railway/station) | Rajouri Garden metro station (Blue Line) as proxy for the junction; the Pink Line station is about 0.3 km away. |
| 9 | Dilshad Garden Metro Junction | 28.675886 | 77.321446 | verified | way/701576004 (railway/station) | OSM station named 'Dilshad Garden' on GT Road, matching 'Dilshad Garden Metro Junction' by name. |
| 10 | Lodhi Colony Crossing | 28.579754 | 77.216873 | approx | node/1988513582 (railway/station) | Lodhi Colony metro station as proxy for the crossing; the OSM locality centroid is 1.2 km away (north-east). |
| 11 | Tilak Nagar Junction | 28.636548 | 77.096496 | approx | node/6548798519 (railway/station) | Tilak Nagar metro station; its OSM address names a 'Tilak Nagar Round About' but the junction itself was not checked. |
| 12 | Ram Nagar Crossing | 28.648244 | 77.215412 | approx | node/4589452407 (place/neighbourhood) | UNSURE: at least three Ram Nagar localities exist in Delhi (Shahdara North, Sadar Bazar/Old Delhi, West Delhi). The Sadar Bazar one is chosen because it sits in the central cluster near Paharganj; the label gives no way to disambiguate. |
| 13 | Barakhamba Road Junction | 28.627473 | 77.229045 | approx | way/523568284 (highway/secondary) | Centre point of one OSM segment of Barakhamba Road (three segments returned); the junction in the label is not pinned. |
| 14 | Vivek Vihar Crossing | 28.668035 | 77.319471 | approx | node/10812202316 (place/suburb) | Centroid of the Vivek Vihar locality (OSM place=suburb), about 1 km from the Dilshad Garden pin. |
| 15 | Jangpura Extension Junction | 28.582129 | 77.243442 | approx | way/863797650 (landuse/residential) | Centroid of the OSM residential landuse 'Jangpura Extension'; the junction is not pinned. |
| 16 | Mayur Vihar Isolated Pocket | 28.609855 | 77.292632 | approx | node/10815246204 (place/suburb) | Centroid of the Mayur Vihar locality (OSM place=suburb). Disconnected district in the engine graph. |
| 17 | Patparganj Isolated Pocket | 28.639689 | 77.310283 | approx | relation/3808258 (boundary/administrative) | Judgement call: the plain 'Patparganj' hit lies only 0.28 km from the Mayur Vihar pin and would overlap on the map, so the centroid of 'Patparganj Industrial Area' (3.7 km away) is used. Disconnected district in the engine graph. |
| 18 | Najafgarh Remote Junction | 28.612450 | 76.986507 | approx | node/9172459622 (railway/station) | Najafgarh station on Shivaji Marg as proxy for 'Najafgarh Remote Junction'. Disconnected district in the engine graph. |
| 19 | Dwarka Sector Remote Point | 28.600724 | 77.042942 | approx | node/2203010089 (place/suburb) | UNSURE: the label names no sector, so the centroid of the Dwarka sub-city (OSM place=suburb) is used, not a specific sector. Disconnected district in the engine graph. |

## Oracle statistics (web-app/tools/oracle/oracle.py), lab vs delhi-geo
| Metric | lab | delhi-geo (usable_minutes untouched) | delhi-geo (final donations.csv) |
|--------|-----|----------------------------------------|----------------------------------|
| open donations | 43 | 43 | 43 |
| open donations with zero candidates | 3 (7%) | 8 (19%) | 8 (19%) |
| zero-candidate donation ids | DON008, DON032, DON045 | DON003, DON008, DON012, DON016, DON026, DON029, DON032, DON045 | DON003, DON008, DON012, DON016, DON026, DON029, DON032, DON045 |
| boundary cases (travel == usable) | 5 | 5 | 5 |
| candidate-count distribution (count:donations) | 0:3, 1:11, 2:4, 3:4, 4:1, 5:1, 6:4, 7:1, 8:10, 11:4 | 0:8, 1:9, 2:4, 3:1, 4:2, 6:3, 7:1, 8:10, 10:1, 11:4 | 0:8, 1:9, 2:4, 3:1, 4:2, 6:3, 7:1, 8:10, 10:1, 11:4 |

Boundary cases in `lab` are deliberate; in delhi-geo they are whatever the real minutes happen to produce: [('DON005', 'R004', 25), ('DON006', 'R011', 8), ('DON018', 'R012', 8), ('DON043', 'R006', 23), ('DON043', 'R007', 23)] before rescaling,
[('DON005', 'R004', 25), ('DON006', 'R011', 8), ('DON018', 'R012', 8), ('DON043', 'R006', 23), ('DON043', 'R007', 23)] after. No hand-crafted boundary fixtures should be expected from this scenario.

## usable_minutes rescale (degenerate check, threshold 40% of open donations with zero candidates)
Not degenerate: `donations.csv` is byte-identical to the lab file; no rescale and no `donations.lab-original.csv`.

## C++ loader and validator
```
scenario: web-app\data\scenarios\delhi-geo
nodes=20 edges=25 donors=12 recipients=26 donations=46 transactions=3
graph components (id: size): {0: 16, 1: 2, 2: 2}
edge minutes min/median/max: 5 / 13 / 29
usable_minutes (excluding 999999999) min/median/max: 7 / 30 / 180
geo_overlay.json: {'minutesSource': 'osrm', 'estimate_edges': 0, 'osrm_edges': 25}
--- C++ loader on a temp copy: CommunityBridge.exe <temp copy of delhi-geo>, stdin '0' ---
CommunityBridge: location-aware travel-time feasibility
(travel times are simulated minutes, not real road data)
Warning: R024 (node 16) is not connected to any donor; it can never be recommended.
Warning: R025 (node 17) is not connected to any donor; it can never be recommended.

1. Process the most urgent donation
2. Add a donation
3. Add a recipient
4. Show donations
5. Show recipients
6. Show transactions
7. Show road network
8. Show travel times from a donor
9. Show summary
0. Exit
Choice: 
Goodbye.
--- exit code 0 ---
WARNING: 2 recipient(s) not connected to any donor (loader warns, by design in lab): R024, R025
RESULT: OK (0 errors, 1 warnings)
```

## Problems and limits
- Node positions are approximate; labels do not name exact points. See the per-node table; 12 and 19 are the least certain.
- Minutes are OSRM car-profile free-flow estimates (no traffic, no turn restrictions beyond what OSM carries), rounded up. Real peak-hour Delhi times will be longer.
- Edge endpoints are fixed to the lab graph's 25 pairs. Real roads would connect different pairs (e.g. 16 Mayur Vihar and 17 Patparganj are
  road-reachable from the main cluster, but are kept disconnected on purpose; the overlay geometry for 16-17 and 18-19 is still real).
  The lab's hand-made "slow direct edge vs fast multi-hop path" cases are not preserved: edges whose minutes moved by 10 or more versus lab
  (lab to real): 0-3 (25 to 8), 2-10 (6 to 23), 9-13 (6 to 16), 14-11 (13 to 29), 11-12 (8 to 18), 1-9 (33 to 8). Do not expect the lab's Dijkstra-vs-direct demonstration to carry over.
- The engine's console banner still says "travel times are simulated minutes, not real road data" (console-app text, read-only here); the web app must not reuse that wording for this scenario.
- `transactions.csv` is copied from lab: its three `travel_minutes` (8, 15, 9) are the old simulated values and are not recomputed. The loader does not check them.
- The lab `usable_minutes` were tuned to simulated minutes, which is why the rescale check exists.
- Only the u-to-v direction of each road was routed.
