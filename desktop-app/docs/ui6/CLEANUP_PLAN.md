# CLEANUP PLAN (Step 0.4)

Every top level item of desktop-app and of desktop-app/docs with its bucket. Nothing outside desktop-app is touched.

| Bucket | Item | Files | Bytes | Reason |
|--------|------|-------|-------|--------|
| KEEP | src | 24 | 327052 | project sources |
| KEEP | include | 20 | 45894 | project headers |
| KEEP | res | 4 | 42812 | resources |
| KEEP | tests | 12 | 125994 | tests |
| KEEP | tools | 2 | 6600 | development tools |
| KEEP | CMakeLists.txt | 1 | 6720 | build file |
| KEEP | run_gui.bat | 1 | 621 | launcher |
| KEEP | reset_data.bat | 1 | 974 | script |
| KEEP | make_dist.bat | 1 | 1071 | script |
| KEEP | create_desktop_shortcut.bat | 1 | 1168 | script |
| KEEP | remove_desktop_shortcut.bat | 1 | 662 | script |
| KEEP | data | 6 | 5452 | data folder, content untouched |
| KEEP | dist | 14 | 1580194 | used by the desktop icon |
| KEEP | build | 318 | 18518909 | regenerable build folder |
| KEEP | releases\v2.7 | 1 | 1568256 | newest release |
| KEEP | docs\DESIGN_GUIDE.md | 1 | 13191 | design source |
| KEEP | docs\OUT_OF_SYLLABUS_NOTICE.md | 1 | 2420 | kept by the brief |
| KEEP | docs\CHANGELOG.md | 1 | 15900 | kept by the brief |
| KEEP | docs\CURRENT_STATE.md | 1 | 5040 | kept by the brief |
| KEEP | docs\ui2\MANUAL_CHECKLIST_V2.md | 1 | 17603 | newest manual checklist, moved up to docs\MANUAL_CHECKLIST_V2.md |
| KEEP | docs\ui6 | 9 | 280196 | this round |
| UNSURE | legacy | 7 | 62550 | old dialog sources (.cpp/.h); nothing builds them, but Step 0 never deletes a source file |
| CLEAN | build-dist | 83 | 8180725 | regenerable release build folder |
| CLEAN | releases\v0.1 | 1 | 2912326 | older release |
| CLEAN | releases\v0.2 | 1 | 3060365 | older release |
| CLEAN | releases\v0.3 | 1 | 3098261 | older release |
| CLEAN | releases\v0.4 | 1 | 3119264 | older release |
| CLEAN | releases\v1.0 | 1 | 3119264 | older release |
| CLEAN | releases\v1.1 | 1 | 3142305 | older release |
| CLEAN | releases\v2.6 | 1 | 1517568 | older release |
| CLEAN | docs\ui2 | 848 | 33928025 | old notes folder (its checklist is kept) |
| CLEAN | docs\ui3 | 2015 | 339612073 | old notes folder |
| CLEAN | docs\evidence | 96 | 1146290 | old evidence |
| CLEAN | docs\scratch | 66 | 2395857 | old scratch |
| CLEAN | docs\ASTRYX_DESIGN_GUIDE.md | 1 | 13191 | same text as DESIGN_GUIDE.md |
| CLEAN | docs\BUILD_PLAN.md | 1 | 5082 | old notes |
| CLEAN | docs\DECISIONS.md | 1 | 9713 | old notes |
| CLEAN | docs\FINAL_REPORT.md | 1 | 8504 | old notes |
| CLEAN | docs\KNOWN_ISSUES.md | 1 | 7470 | old notes |
| CLEAN | docs\MANUAL_CHECKLIST.md | 1 | 9542 | older manual checklist |
| CLEAN | docs\PROGRESS.md | 1 | 3722 | old notes |
| CLEAN | docs\TESTING.md | 1 | 12134 | old notes |
| CLEAN | docs\UNDERSTANDING.md | 1 | 5623 | old notes |
| CLEAN | docs\WORKLOG.md | 1 | 6472 | old notes |

- KEEP : 421 files, 22566729 bytes
- CLEAN : 3125 files, 405313776 bytes
- UNSURE : 7 files, 62550 bytes

Decision S0-1: the file law protects the live project sources (src, include, res, tests, CMakeLists.txt, the .bat files). Source-type files inside old backup and scratch folders under docs and generated files in build-dist are copies, not project sources, so they are cleaned with their folder. legacy is a source folder at project level and stays (UNSURE).
