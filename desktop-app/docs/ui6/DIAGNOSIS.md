# DIAGNOSIS (round ui6, Step 1)

All probes ran the REAL exe in normal mode (no --dpi override, no test mode) on a copy of the data, measured from outside with Win32 calls, and captured with PrintWindow (docs/ui6/scratch/probe.ps1). Display: 120 dpi (125 percent).

## P1 Layout (pages hug the sidebar)
| Cause | Verdict | Evidence | Reasoning |
|-------|---------|----------|-----------|
| pagetable::onsize makes the list only as wide as its content (list_.neededwidth(), at least px(560)) instead of the content width | PROVEN | step1_probe_maximised.txt, shots/step1_probe_maximised.png, src/PageTable.cpp lines 133-143 | maximised: host 1657 px, list 974 px (303 to 1277), right gap 643 px, the same numbers the user saw (308 to 1277, about 635 px) |
| the button row is aligned to the list's right edge (edge = left + listwidth), not to the page | PROVEN | same files, PageTable.cpp line 169 | Process donation right edge 1277 = list right edge |
| the empty-state text is centred in the narrow list | PROVEN | shots/step1_probe_recipient.png (viewed) | "No offers are waiting for you" sits near x=660 of a 1657 px host |
| home pages and the donor list share the same narrow column | PROVEN | step1_probe_donor.txt (list 700 px, right gap 917 px) | same shape code |
| page margin is px(32), not the guide's 24 | PROVEN | PageTable.cpp line 128 | left_margin=40 at 120 dpi = 32 design px |

## P2 DPI (the app does not adapt to a scale change)
| Cause | Verdict | Evidence | Reasoning |
|-------|---------|----------|-----------|
| the manifest is missing or not per-monitor v2 | REJECTED | step1_dc1_manifest.txt | one RT_MANIFEST, id 1, PerMonitorV2 (2016) and dpiAware (2005) |
| the window is not per-monitor v2 in normal use | REJECTED | step1_probe_maximised.txt | GetWindowDpiAwarenessContext of the real main window = -4, GetDpiForWindow 120, GetDpiForSystem 120 |
| there is no WM_DPICHANGED handler | REJECTED | src/GuiShell.cpp line 567 (mainproc) | the case calls shellapplydpi with the suggested rectangle |
| the starting dpi comes from GetDpiForSystem, which Windows keeps at the value of the sign-in | REJECTED (corrected in Step 2) | src/GuiShell.cpp end of shellcreate | shellcreate calls shellapplydpi(themedpiforwindow(main)) when the window dpi differs, so the contents follow the real dpi; only the first window size is computed at the system dpi (a cosmetic size, not P2) |
| a running copy (for example the desktop icon's dist exe) swallows a new start: the mutex makes run_gui.bat's new exe only bring the old window forward | NOT PROVEN (plausible) | src/gui_main.cpp lines 276-288 (CreateMutexA, bringfirstforward) | the user may have looked at an older running window; run_gui.bat gives no sign of it |
| stray SetProcessDPIAware fallback | PROVEN harmless, not needed | grep in Step 1: gui_main.cpp lines 62 and 69 | the manifest already decides; the call can only fail; removed in Step 2 (DC3) |
| the DPI checks of earlier rounds used the --dpi override (themesetdpioverride) and AdjustWindowRectEx, not the normal path | PROVEN | GuiSelfTest.cpp runcycle (usedpi(96)), Theme.cpp themedpiforwindow | P4: the test path differs from the user's path |

## P3 Sidebar (narrow nav, inner line, dotted outline)
| Cause | Verdict | Evidence | Reasoning |
|-------|---------|----------|-----------|
| the nav control is narrower than the sidebar at start or after a resize | REJECTED (start, resize) | step1_probe_maximised.txt, step1_probe_1200.txt, step1_probe_recipient.txt, step1_probe_donor.txt, step1_probe_dist.txt, step1_probe_v27.txt | sidebar 263, nav 268 and nav column 268 for every role, both sizes and all three exes; no off-colour column in the empty sidebar; the selection colour 4 px from both edges |
| the one nav column is sized with LVSCW_AUTOSIZE_USEHEADER; on a list without a header this fits the longest item ("Incoming offers") in some states (likely after a dpi change or while hidden) | NOT PROVEN | GuiShell.cpp layoutmain | matches the user's 138 px at 120 dpi (about 110 design px, the width of "Incoming offers"); the dpi-change path can only be driven inside the program (Step 2 DC4, Step 3 SC1) |
| the dotted focus outline appears because the main window is not a dialog, so Windows never hides focus cues for mouse users | NOT PROVEN | no WM_CHANGEUISTATE in the sources (grep) | posted clicks in the probe did not focus the list; Step 3 SC5 measures it in-process |
| the "inner line" is the Explorer theme's edge line of the column | NOT PROVEN | GuiShell.cpp comment (stage U3): the list is made px(4) wider than the panel to clip that line | with a full-width column the line falls outside the panel; Step 3 tests with and without the theme |

## P4 Earlier checks
PROVEN: the earlier --selftest measured its own test path (dpi override) and accepted any list width between the content width and the content area, so P1 passed; it never measured the nav column. This round's checks use the real path: probe.ps1 (outside), the manifest in the exe, a mode without the dpi override.

## Launch path
run_gui.bat starts build\communitybridge_gui.exe (newest, 10:19 today). dist\CommunityBridge and releases\v2.7 hold an identical older exe (07-11 22:55). All three behave the same at start (step1_probe_*.txt).
