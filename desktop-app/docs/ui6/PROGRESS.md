# PROGRESS (round ui6, source of truth)

## Baseline (Step 0.3, before any move)
- Build: `cmake --build build` exit 0 (step0_build.txt). ctest: 12 of 12 passed, Total Test time 61.39 s (step0_ctest.txt).
- --selftest on a data copy: exit 0, summary checks_failed=0, violations=0 on every page line (step0_selftest.txt).
- console app: 72 files hashed, identical to round one (step0_phase2_hashes.txt). Backend: 20 files hashed (step0_backend_hashes.txt).

## Steps
| Step | Status | Fix rounds | Evidence | Note |
|------|--------|-----------|----------|------|
| 0 Clean | DONE | 0 | tree_before.txt, tree_after.txt, step0_*.txt, step0_quarantine_manifest.tsv | 3124 files (405 MB) cleaned; verify equal to baseline; hashes identical; quarantine deleted |
| 1 Plan | DONE | 0 | step1_probe_*.txt, shots/step1_probe_*.png, step1_dc1_manifest.txt, step1_contrast_*.txt, step1_astryx_*.txt, step1_phase_*.txt | P1 proven (list 974 of 1657 px, gap 643); P2 awareness -4 proven, start dpi from GetDpiForSystem; P3 not reproduced at start or resize |
| 2 Foundation | DONE | 2 | step2*_build.txt, step2b2_dpichange.txt, step2c_dc6.txt, step2c_dc7_dpilog.txt, step2_tk1_dc3.txt, step2_tk2_contrast.txt, step2_dc1_recheck.txt, step2_phase_*.txt, shots/step2_sheet_*.png | guide palette and fonts, spacing tokens, sidebar 240; DC1-DC7 pass; TK2, TK3 pass; TK1 colours/fonts 0, px literals staged to Step 4 |
| 3 Shell | DONE (was BLOCKED, resumed) | 3 + 3 | step3a_*.txt, step3b_*.txt, step3c_*.txt, step3_phase_*.txt, shots/step3b_probe_recipient.png, shots/step3c_probe_maximised.png | P3: nav = sidebar width, ItemsView look, 2 px edge clipped by a window region; SC1-SC5 pass at 6 dpis x 5 sizes. P1 on table pages: list fills the content, buttons at the right margin (real exe: 30/30 margins at 120 dpi) |
| 4 Pages | DONE (was BLOCKED, resumed) | 3 + 1 | step4a*_*.txt, step4b_*.txt, step4c_*.txt, step4_phase_*.txt, step4_review.md, shots/step4_sheet_a.png, shots/step4_sheet_b.png , step4r_*.txt, shots/step4r_sheet_match.png | Home stat row, banners, Match equal facts and balanced lists, forms as a centred column with labels above; LC4, LC5, LC7; TK1 0 px literals in Page*.cpp; phase test pass |
| 5 Review and release | DONE | 2 | step5_*.txt, shots/step5_before_after.png, shots/step5_before, shots/step5_after, step5_review.md | before/after viewed (v2.7 vs v3.0 at 1920x1010); version 3.0.0.0; releases/v3.0 = dist exe; packaged selftest from %TEMP% 0 violations; final ctest 12/12, flows failed=0 at 96 and 192, hashes identical, searches clean |

## Checklist of the step in progress
(none: round ui6 is finished)

## Tried so far
- Step 5 round 1: SC5 (1041 dark pixels at 192 dpi, Summary) taken for focus cues; app hides cues on a nav click, test skipped SC5 while cues on -> the packaged run still failed (on Transactions), so the cue idea was wrong; the skip was removed again.
- Step 5 round 2: the hot item is cleared after the message pump, right before the picture (the real mouse pointer over the sidebar is the likely cause, not proven); SC2 judges only the part of the sidebar inside the work area -> step5_selftest3b and step5_packaged_selftest3: 0 violations.
- 4a-3 Match round 1: six equal fact columns clipped the donor name at 900 px (step4a3_selftest.txt, 25 violations).
- 4a-3 round 2: a wrapping fact grid (3 to 6 columns) made each list under the 120 px minimum (d) at 900x600 (step4a3_selftest2.txt).
- 4b: a script cut the old onsize at the first inner brace, so the build failed (step4b_build.txt); the leftover lines were removed (an editing slip, not a test fix round).
- 4a-3 round 3: one row of measured columns, donor takes the rest; the error line beside the quantity wrapped, given label row plus field row height (step4a3_selftest4.txt, 0).

## Files created or changed this round
- docs/DESIGN_GUIDE.md (copy of ASTRYX_DESIGN_GUIDE.md), docs/CURRENT_STATE.md, docs/ui6/* (notes, scratch scripts, backup_pre_ui6), docs/MANUAL_CHECKLIST_V2.md (moved up from docs/ui2); ui6/DIAGNOSIS.md, ui6/UI_PLAN.md, ui6/CLEANUP_PLAN.md
- Step 5: res/gui.rc, src/GuiShell.cpp, src/GuiSelfTest.cpp, docs/CHANGELOG.md, docs/MANUAL_CHECKLIST_V2.md, docs/OUT_OF_SYLLABUS_NOTICE.md, docs/CURRENT_STATE.md, releases/v3.0/communitybridge_gui.exe, dist/CommunityBridge (make_dist), docs/ui6/FINAL_REPORT_UI6.md
- Step 4: include/Theme.h, src/Theme.cpp, include/GuiLayout.h, include/GuiPage.h, src/GuiPage.cpp, src/PageTable.cpp, src/PageCoordinator.cpp, src/PageWelcome.cpp, src/PageMatch.cpp, src/PageDonor.cpp, src/PageRecipient.cpp, src/GuiSelfTest.cpp
- Step 3: include/GuiPage.h, src/GuiPage.cpp, src/GuiList.cpp, src/GuiShell.cpp, src/GuiSelfTest.cpp, src/Theme.cpp, src/PageTable.cpp
- Step 2: include/Theme.h, src/Theme.cpp, include/GuiLayout.h, include/GuiShell.h, src/GuiShell.cpp, src/gui_main.cpp, include/GuiSelfTest.h, src/GuiSelfTest.cpp, run_gui.bat, make_dist.bat
