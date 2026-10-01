# CommunityBridge

CommunityBridge is a menu-based C++ console decision-support tool for surplus-food redistribution. For a donation with limited usable time, it finds which recipient organizations can feasibly take it, and recommends which one should be offered first to a human coordinator. The coordinator makes the final choice.

## Note on simulated data

Edge weights in the graph are SIMULATED minutes. They are not real road or traffic data. This feature is called "location-aware travel-time feasibility".

## Folder structure (Phase2/)

- `include/` : header files
- `src/` : C++ source files
- `data/` : CSV files (donors, recipients, donations, transactions, graph nodes, graph edges)
- `tests/` : test programs, with `fixtures/` for test data

`Phase2/docs/` and `Phase2/reference/` are local-only. They are not part of this repository.

## Build

The project is built with plain g++ only. There are no build scripts. On Windows the output file gets a `.exe` extension.

Main program (run from the repository root):

`g++ -std=c++17 -IPhase2/include Phase2/src/*.cpp -o Phase2/communitybridge`

Tests: each test file has its own `main()`, so a test build lists only the .cpp files it needs and never `src/main.cpp`.

Example (test_dijkstra):

`g++ -std=c++17 -IPhase2/include Phase2/tests/test_dijkstra.cpp Phase2/src/Graph.cpp Phase2/src/Dijkstra.cpp -o Phase2/test_dijkstra`

Commands will work once source files are added.
