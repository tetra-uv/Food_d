# Plan-time oracles (prototypes)

Independent Python re-implementations of the console app decision rules, written while planning to (a) verify the project
README's worked example and (b) pick demo scenarios. They read CSVs only and share no code with the backend.

- `oracle.py <data-folder>` — six rules + ranking, dataset statistics, DON001 example, rejection histogram, multi-org nodes.
- `compare_baselines.py <data-folder>` — Dijkstra vs straight-line baseline, what-if checks.

Run (from this folder): `python -I oracle.py ../../../desktop-app/data`

In milestone M0/M1 these become the third leg of the golden-parity suite. They are tooling, not product code.
