# CURRENT STATE of desktop app (written in round ui6, Step 0, from the files as they are)

## What the app is
CommunityBridge desktop app: one native Windows window (raw Win32, C++17, CMake + Ninja, MinGW.org GCC 6.3, 32-bit, static exe) on top of the frozen console app console program. The window has a sidebar, a page host (one page at a time, pages are child windows made once and shown or hidden) and a status bar. Three roles: Coordinator, Donor, Recipient. Offers are soft: the coordinator sends one, the recipient accepts (console app allocation) or declines.

## Pages and their buttons (from Page*.cpp)
- Welcome (two steps in one page): step 1 Coordinator, Donor, Recipient; step 2 a name list, Back, Continue (primary).
- Coordinator: Home (caption and value lines, notes); Donations (Close donation, Process donation primary); Offers (Withdraw offer primary); Recipients; Transactions; Summary; Match a donation (Back, Send offer primary).
- Donor: Home; My donations; Offers; Given out; Add donation form (Cancel, Save primary).
- Recipient: Home; Incoming offers (Decline offer, Accept offer primary); Past offers; Received food; My details form (Accepting donations and Can collect food check boxes, Save primary).
- Sidebar: program name, the navigation list of the role, a user box with Switch user.

## Backend (no windows.h; read-only in this round; SHA256 in ui6/evidence/step0_backend_hashes.txt)
AppState (start-up: load, seed id issuers, travel times, offers), Dashboard (Home numbers), Match (match view struct), Offer and OfferStore (offers.csv), Queries (positions per person), RecipientUpdate (capacity and flags), Tables (text of every list), TextTools (text helpers), Workflow (every action). console app: src, include, data (72 files hashed, identical to round one).

## GUI layer (include windows.h or named Gui*, Page*, Theme*, gui_main*, plus resource.h)
- gui_main.cpp: start-up, command line, single instance, test modes.
- GuiShell.cpp/.h: main window, sidebar, page host, status bar, sign in and out, shellapplydpi.
- GuiLayout.cpp/.h: px() scaling, centredcolumn(), guilayout batch positioning (DeferWindowPos).
- GuiList.cpp/.h: the list view of every page.
- GuiPage.cpp/.h: page base class and control makers (makebutton, makecheck, ...), page ids.
- GuiMessage.cpp/.h: message boxes (log lines in test modes).
- GuiSelfTest.cpp/.h: the test modes (layout measurements, flows, screenshots).
- PageTable.cpp/.h: the two shared page shapes (pagetable: title, one list, buttons bottom right; pagehome: title and caption/value lines).
- PageWelcome, PageCoordinator, PageMatch, PageDonor, PageRecipient .cpp: the pages. Pages.h declares them.
- Theme.cpp/.h: colours, fonts, brushes, list and sidebar styling, WM_CTLCOLOR answers.
- res/gui.rc (manifest, version 3.0.0.0, icon), res/app.manifest, res/app.ico, res/dist_reset_data.bat; resource.h.

## Tests (CTest, 12): last result 12 of 12 passed (ui6/evidence/step0_ctest.txt)
test_p2link, test_offer, test_offerstore, test_queries, test_tables, test_recipientupdate (file test_recipient_edit.exe), test_workflow, test_dashboard, test_text, test_notes, gui_selftest, gui_selftest_flow (the last two run the exe through tests/RunGuiTest.cmake on a copy of desktop-app/data). Per-program PASS counts were not re-measured in Step 0.
Self-test on a copy (ui6/evidence/step0_selftest.txt): exit 0, summary checks_failed=0, violations=0 on every page line.

## Test modes and scripts
- `communitybridge_gui.exe [datafolder] [--selftest | --selftest-flow | --screenshots <folder>] [--dpi <n>] [--multi]`; writes selftest.txt next to the exe. Exit 0 fine, 1 start problem, 2 start problem in a test mode, 3 a check failed.
- run_gui.bat (configure if needed, build, start build\communitybridge_gui.exe on desktop-app\data), make_dist.bat (Release build in build-dist, install to dist\CommunityBridge), reset_data.bat, create_desktop_shortcut.bat, remove_desktop_shortcut.bat.
- tools/make_icon.ps1, tools/bmp2png.ps1 (development helpers).

## Decisions that still stand (from earlier rounds and the user)
One window, pages as children, no menu bar, no search, no sorting, no cards, no own keyboard shortcuts, light mode only, every action has one home, no punctuation in screen text, statuses as words, destructive actions ask first, one primary button per page. Design basis: docs/DESIGN_GUIDE.md (Astryx values and rules, no code; user approved 2026-11-07).

## Open issues (user, ui6 brief PART 2)
P1 pages hug the sidebar in a narrow strip; P2 the app does not adapt when the Windows display scale changes; P3 the sidebar navigation list is narrower than the sidebar (inner line, selection and dotted outline stop at it); P4 earlier checks passed while P1 to P3 were visible.

## Last release
releases/v3.0 (08-11-2026) and releases/v2.7 (07-11-2026); dist/CommunityBridge (the desktop shortcut uses it). Older releases v0.1 to v2.6 are cleaned in Step 0.

## Source files that nothing uses
legacy/ (GuiCommon, GuiCoordinator, GuiDonor, GuiRecipient, GuiStart .cpp, GuiCommon.h, GuiScreens.h): the round one dialogs; CMakeLists.txt does not build them. Kept (Step 0 never removes source files).

## Not known
Whether the manifest in the built exe is really per-monitor v2 and whether WM_DPICHANGED reaches the window in normal use (Step 1 measures it).
