# UI PLAN (round ui6)

Principle for this round: the smallest code that works (user request). Change values in one place, reuse the existing shapes (pagetable, pagehome, guilayout), add checks only where the brief asks.

## Token map (guide value, our constant, file)
| Guide | Value | Our constant | File |
|-------|-------|--------------|------|
| page (T1, PAGE_STYLE 0) | soft off-white RGB(250, 248, 244); white RGB(255, 255, 255) when PAGE_STYLE 1 | palettepage | Theme.cpp |
| sidebar (T2) | RGB(240, 237, 230); RGB(241, 244, 247) when PAGE_STYLE 1 | palettesidebar | Theme.cpp |
| text-primary | RGB(10, 19, 23) | palettetext | Theme.cpp |
| text-secondary | RGB(78, 96, 111) | palettemuted | Theme.cpp |
| accent | RGB(0, 100, 224) | paletteaccent (app name only) | Theme.cpp |
| text-red, text-green, text-yellow, text-blue | RGB(123, 2, 16), RGB(9, 68, 31), RGB(117, 63, 7), RGB(4, 47, 151) | paletteerror, palettesuccess, palettewarning, paletteinfo | Theme.cpp |
| error, success, warning, info muted fills (over white) | RGB(249, 209, 216), RGB(206, 235, 210), RGB(249, 237, 204), RGB(204, 230, 254) | paletteerrorfill ... paletteinfofill | Theme.cpp |
| accent-muted (selection) | RGB(204, 230, 254) | paletteselect | Theme.cpp |
| body / label / caption / section / title / welcome | 14 400 / 14 500 / 12 400 / 17 600 / 24 600 / 35 400 px | FONT_BODY, FONT_LABEL, FONT_CAPTION, FONT_SECTION, FONT_TITLE, FONT_WELCOME | Theme.h/.cpp |
| spacing 4, 8, 12, 16, 24, 32 | design px | gaptiny, gapsmall, gapfield, gapbox, gapsection (= page margin), gaptop | GuiLayout.h |
| sidebar, control height, list row | 240, 32, 32 | sidebarwidth, controlheight, listrow | GuiLayout.h, Theme.cpp |
| form column | 640 | formwidth (FORM_FULL_WIDTH switch) | GuiLayout.h |

## File-by-file change list (GUI layer only; batches of at most 3 files)
- Step 2a: Theme.h, Theme.cpp, GuiLayout.h (palette, PAGE_STYLE, fonts in px, spacing tokens).
- Step 2b: gui_main.cpp (remove setdpiaware, add --dpi-log and --selftest-dpichange words), GuiShell.cpp (start at GetDpiForWindow of the main window, log WM_DPICHANGED, nav column), GuiSelfTest.cpp/.h (dpichange mode and its checks).
- Step 2c: run_gui.bat, make_dist.bat (stop on a failed build with exit 1, print the exe path and time, pass extra arguments).
- Step 3a: GuiLayout.cpp/.h (content box with equal margins, button row from the right, list column fitting helper) and GuiList.cpp.
- Step 3b: GuiShell.cpp (sidebar 240, nav column = nav client width, focus cues hidden for the mouse with WM_CHANGEUISTATE, layout on WM_SIZE and WM_DPICHANGED).
- Step 3c: PageTable.cpp (title, list full width, button row), GuiPage.cpp (title font and colour).
- Step 4a: PageWelcome.cpp, PageCoordinator.cpp, PageMatch.cpp. 4b: PageDonor.cpp. 4c: PageRecipient.cpp. GuiSelfTest.cpp for the LC and SC checks as needed.

## Order of work and checks per step
- Step 2: TK1 to TK3, DC1 to DC7, screenshots of welcome and one page per role at 96, 144, 192; phase test.
- Step 3: SC1 to SC5, LC1 to LC3 and LC6 on the shell and table pages, DC4 again; phase test.
- Step 4: LC1 to LC7 and UX per role, flows at 96 and 192, screenshots V1 to V17; phase test per role.
- Step 5: full review, version 3.0.0.0, release, dist, final regression and report.

## Risks
- GuiSelfTest.cpp is 2784 lines: add checks next to the existing checkpage, never rewrite it.
- Changing margins and sizes breaks the expected numbers of existing layout checks: update them and list each edit (before, after).
- The real scale change cannot be done by the agent: covered by --selftest-dpichange (real WM_DPICHANGED, no override) and the user's --dpi-log test.
- The old MinGW headers lack newer DPI functions: load them with GetProcAddress.
- PrintWindow pixel scans need the window visible and unobscured for PW_RENDERFULLCONTENT; the probe keeps the window in front.
