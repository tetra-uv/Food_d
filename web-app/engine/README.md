# cb_engine (web-app/engine)

JSON-lines bridge over the frozen console app + desktop app C++ backend. It compiles `../../console-app/src` (without ConsoleView/main) and
`../../desktop-app/src` (without Gui*/Page*/Theme/gui_main, WITH AppState.cpp) as they are; no backend file is changed or copied.
Single threaded (MinGW.org GCC 6.3, win32 threads), C++11/14 style, hand-written JSON, static 32-bit exe.

## Build
```
powershell -File web-app\scripts\build-engine.ps1        # manifest + cmake (Ninja, Release) + build -> engine\build\cb_engine.exe
```
The path contains a space; CMake/Ninja handle it (verified). `engine\tools\gen_manifest.ps1` writes `BACKEND_MANIFEST.txt`
(SHA-256 of the 47 backend files compiled or included) and the aggregate hash that `sys.hello` returns.

## Run by hand
```
cb_engine.exe --data <folder> [--protocol 1] [--self-test] [--version]
type requests.ndjson | engine\build\cb_engine.exe --data web-app\data\work\lab
```
stdout = protocol lines only (first line is `{"event":"ready",...}` or `{"event":"boot_failed",...}`); diagnostics on stderr.
Exit codes: 0 clean, 2 boot failed, 64 usage, 70 internal/out of memory. NEVER point `--data` at `data\scenarios\*` for write
commands: the engine rewrites CSV files in place (use `data\work\<name>` or a temp copy).

## Implemented commands (doc 04 subset)
Reads: `sys.hello` `sys.ping` `sys.shutdown` `sys.reload` `read.snapshot` `read.network` `read.queue` `read.stats` `match.build`
`match.preview` `match.compare` `net.field` `net.route` `offer.check` `donor.validate`.
Writes (each bumps `rev` on success, returns `effects`): `offer.send` `offer.accept` `offer.decline` `offer.withdraw`
`donation.create` `donation.createWithNewDonor` `donation.close` `donor.create` `recipient.create` `recipient.update`.
Debug: `sys.echo` (returns args), `sys.classify {message}`.
Not implemented: `--log-level` is accepted and ignored; no pre-flight on copies for `offer.accept`.
Error codes follow doc 04 section 7; unknown backend messages become `OPERATION_REFUSED`.

## Tests
```
python -I web-app\engine\tests\test_engine.py     # prints "ENGINE TESTS: n PASS, m FAIL"
python -I web-app\engine\tests\bench.py           # latency of match.build x1000, snapshot size
```
Covers: JSON round trips, malformed and fuzzed input, line limit, parity with an independent Python re-implementation of the six
rules + ranking for all 46 lab donations, DON001 README example, what-if, net.field/route, a full offer-accept flow on a temp
copy (CSVs reload, `sys.reload`, restart), refusal codes, and a scan of every message literal in desktop app `Workflow.cpp` and
`RecipientUpdate.cpp` against the classifier. Write tests use copies under `engine\testdata\work\` (removed afterwards).

## Known gaps
No pre-flight accept, no `--log-level` behaviour, no classifier unit test inside C++ (it is done through `sys.classify`), JSON
numbers above 2^53 are not exact (quantities are 32-bit so this never matters). The backend counts name length in bytes.
