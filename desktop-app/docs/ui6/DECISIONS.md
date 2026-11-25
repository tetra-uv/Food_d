# DECISIONS (round ui6)

Size tags: SMALL, MEDIUM (HUGE ones are only proposals, at the end).

## Decisions of the brief (T1 to T12)
- T1 Page and list background = the existing soft off-white of Theme; the guide's white page is the alternative behind one switch PAGE_STYLE in Theme.cpp. Lists use the page colour.
- T2 Sidebar tone = the existing Theme sidebar tone; the guide's cool grey is the alternative behind the same switch.
- T3 Every other colour from the guide (text, muted, accent, error, success, warning, info text and fills). Accent only on the app name (and the selection mark if possible). Page title in the text colour, semibold.
- T4 Type in design pixels: body 14 normal, label 14 weight 500, caption 12, section 17 weight 600, title 24 weight 600, welcome 35 normal; Segoe UI; lfHeight = -MulDiv(px, dpi, 96).
- T5 Spacing 4, 8, 12, 16, 24, 32; page margin 24 both sides; sidebar 240; controls 32; button gap 8; field gap 12; section gap 24; 32 above the title; list rows 32.
- T6 Content = page width minus two equal margins; lists fill width and free height with fitted columns (at most 1.6 times content width, remainder to the last column); one button row bottom right, primary rightmost; forms one centred column of at most 640; Home stat rows caption over value in equal columns; Match facts in equal columns and two full-width lists; welcome block centred (centre at 38 to 46 percent of the height).
- T7 Sidebar navigation fills the full sidebar width, full-width selection, no inner line, no dotted outline after a mouse click, equal item heights, one tone, user box at the bottom with Switch user.
- T8 Components per guide section 5 with overrides: no cards; banner = standard static coloured through WM_CTLCOLORSTATIC, only for the coordinator's data notes; form errors one line in error colour; success to the status bar; no sidebar captions; statuses are words; empty states explain; destructive actions ask first.
- T9 No shadows, animation, icons in buttons or dark mode; native controls keep the system look.
- T10 DPI per-monitor v2 effective in the normal window, a real WM_DPICHANGED handler, --dpi-log, a run_gui.bat that cannot start an old exe.
- T11 Plain words, no punctuation, one page title per page.
- T12 Page files contain no colour value, font name or pixel number.

## Decisions taken in this round
- S0-1 (SMALL, Step 0) The file law protects live project sources. Source-type files inside old backup and scratch folders under docs, and generated files in build-dist, are copies, not project sources: cleaned with their folder. legacy (project-level sources, not built) stays as UNSURE.
- S0-2 (SMALL, Step 0) resource.h and the Gui*/Page* headers that do not include windows.h belong to the GUI layer (file names), not to the backend hash list. Backend = 20 files.
- S0-3 (SMALL, Step 0) The newest manual checklist docs/ui2/MANUAL_CHECKLIST_V2.md was moved up to docs/MANUAL_CHECKLIST_V2.md before docs/ui2 was cleaned; docs/MANUAL_CHECKLIST.md (round one) was the older one and was cleaned.

- S1-1 (SMALL, Step 1) The Astryx CLI (0.7.11) agrees with DESIGN_GUIDE.md on every number used (colours, spacing, type sizes); no value changed. Evidence step1_astryx_*.txt.
- S1-2 (SMALL, Step 1) px() keeps one dpi for the whole program: there is one top-level window, and every page and control is its child, so the dpi of the main window is the dpi of every call. The value is taken from GetDpiForWindow(main) at start (not GetDpiForSystem) and from WM_DPICHANGED later.
- S1-3 (SMALL, Step 1) P3 is fixed by the smallest safe changes even though it did not reproduce at start: the nav column is set to the nav client width (no LVSCW_AUTOSIZE_USEHEADER), focus cues are hidden for the mouse (WM_CHANGEUISTATE), each measured in-process by SC1 to SC5 including after WM_DPICHANGED.
- S1-4 (SMALL, Step 1) Code stays minimal (user request): values change in Theme and GuiLayout, pages keep their shape classes, new checks go next to the existing checkpage.

- S2-1 (SMALL, Step 2) FONT_BOLD became FONT_SECTION (17 px semibold, the program name) and FONT_LABEL (14 px, 500) was added; all fonts are design pixels with lfHeight = -MulDiv(px, dpi, 96).
- S2-2 (SMALL, Step 2) The page title and the welcome title use the text colour; the accent is only on the program name (T3).
- S2-3 (SMALL, Step 2) The sidebar width comes from the token sidebarwidth (240), so the existing dpi cycle check expects 240 instead of 210 (test edit, see the list below).
- S2-4 (SMALL, Step 2) The redundant setdpiaware() (SetProcessDpiAwarenessContext with a SetProcessDPIAware fallback) was removed: the manifest decides, and the probe shows awareness -4 without it (DC3).
- S2-5 (SMALL, Step 2) In --selftest-dpichange the list header is checked for fitting its text (font height + 4 up to + 12 design px), not for exact proportional scaling: the native header adds padding that Windows computes itself (36 for 38 at 144 dpi).
- S2-6 (SMALL, Step 2) TK1 is staged: colours, CreateFont and font names are already 0 in the page files; the 130 literal px(N) values sit in the page layout code that Steps 3 and 4 rewrite, and TK1 must be 0 at the end of Step 4.
- S2-7 (SMALL, Step 2) --dpi-log also works together with a test mode, so the line for every WM_DPICHANGED is written by the real handler when --selftest-dpichange sends the messages (DC7 evidence); the real scale change remains the user's test.

- S3-1 (SMALL, Step 3) Two shared helpers in GuiPage: contentbox() (equal margins of 24) and placebuttons() (row from the right, primary rightmost, 32 high, gap 8), so pages need no own numbers.
- S3-2 (SMALL, Step 3) fitcolumns() caps the growth of a column at 1.6 times its natural width; the rest goes to the last column (T6).
- S3-3 (SMALL, Step 3) The nav is exactly as wide as the sidebar and its one column exactly as wide as the nav (no LVSCW_AUTOSIZE_USEHEADER); WM_CHANGEUISTATE(UIS_SET, UISF_HIDEFOCUS) on the main window hides focus outlines until the keyboard is used.
- S3-4 (SMALL, Step 3, replaced by S3-5) The plain look removed the edge line but left the selection off the left padding and made the unfocused selection 240,240,240 on the 240,237,230 sidebar (almost invisible).
- S3-5 (SMALL, Step 3) The nav uses the Windows "ItemsView" look (soft selection 192,218,235 that stays visible without focus) with a 1 px spacer; any themed list draws a 2 pixel edge line at the end of its column at every dpi (measured 96 to 192), so a rectangular window region (SetWindowRgn) leaves out the nav's last 2 pixel columns. The control keeps the full sidebar width; this is a size and region of a standard control, not painting. Constant themeedgeline = 2 in GuiShell.cpp.
- S3-6 (SMALL, Step 3) LC2 keeps whole list rows (the earlier rule "rows not cut"): the list bottom must lie within one row (32 design px + 2) above the button row minus 16, or all rows must be shown; it may never reach below it.
- S3-7 (MEDIUM, Step 3) pagetable::onsize rewritten with contentbox() and placebuttons(): the list fills the content width and the free height, the button row ends at the right and bottom margins (P1 for all table pages).
- S4-1 (MEDIUM, Step 4a) New text roles ROLE_VALUE (Home numbers, FONT_SECTION), ROLE_BANNER (coordinator data notes, warning text on the warning fill) and ROLE_LABEL (label above a field, FONT_LABEL). The guide's roles had no GDI role yet.
- S4-2 (SMALL, Step 4a) New layout tokens textline 24, captionline 20, welcometitle 52, rolebuttonwidth 200, rolebuttonheight 56, welcomewidth 560, namelistheight 320, namelistminimum 120, numberwidth 160, so the page files hold no pixel number (T12). Small welcome gaps moved to the nearest scale token (6 to 4, 20 to 16, 12 to 8 between the step buttons).
- S4-3 (MEDIUM, Step 4a) Match facts: one row of columns as wide as their caption or value, the donor column takes the rest and wraps to two lines. Equal columns were tried and rejected: they clip the donor name at 900 px, and a wrapping grid leaves each list under the 120 px minimum at 900x600.
- S4-5 (MEDIUM, Step 4, after Continue) Match facts use equal columns when every fact fits its share (wide windows) and the measured row of S4-3 otherwise; the first list takes the rows it needs (hugheight, at most half the room) and the second list the rest.
- S4-6 (SMALL, Step 4b) Forms keep the button row at the bottom right of the form column (as on the table pages), so every page has its buttons in the same place.
- S4-4 (SMALL, Step 4a) Helper textwidth() in GuiPage; buttonwidth() now uses it.
- S5-1 (SMALL, Step 5) A mouse click in the sidebar (NM_CLICK) sends WM_CHANGEUISTATE UIS_SET UISF_HIDEFOCUS, so the focus outline goes away after a click even when a key press showed it before (P3); a key press still shows it, as Windows does.
- S5-2 (SMALL, Step 5) Version 3.0.0.0; releases/v3.0 holds the Release exe made by make_dist.bat (same hash as dist).

## Test edits (before and after)
- GuiSelfTest.cpp runcycle (q): before `side.right - side.left == MulDiv(210, newdpi, 96)`; after `== MulDiv(sidebarwidth, newdpi, 96)` (S2-3).

- GuiSelfTest.cpp checkfluid (p): before, a list with at most 3 columns in a host of 1400+ px that spanned the content width was a violation ("stretches over the whole width"); after, that part is removed because the user's decision P1/T6 makes every list fill the content width, and checktable (LC2) now checks that the columns use the width. Reason: the old rule asserted the narrow-strip layout the user rejected.

- GuiSelfTest.cpp checkfluid (p) Home: before, the Home content column had to be at most px(560); after, LC5 checks that the stat values have equal widths and the first starts at the left margin. Reason: T6 makes Home use the full content width as a stat row.

- GuiSelfTest.cpp checksidebar: before, the hot item was cleared before the redraw and the message pump; after, it is cleared after the pump and the list is updated right before the picture, and an SC5 message names the hot item. Reason: the pump delivers moves of the real mouse pointer, which made an item hot again (failed twice at 192 dpi on the item under the pointer).
- GuiSelfTest.cpp checksidebar (sc2): before, fewer than 3 scanned rows was a violation even when the empty sidebar part lay below the work area; after, the row count is only judged when at least 3 rows are inside the work area (bad pixels are always judged). Reason: at 192 dpi the window is taller than the screen.

## Proposals for the user (HUGE, not done)
(none yet)
