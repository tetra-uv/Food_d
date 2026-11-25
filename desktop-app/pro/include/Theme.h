#ifndef THEME_H
#define THEME_H

// Theme.h (professional program): every colour, font and size token. No colour value and no font name appears anywhere else.
// The palette is the one of the old program (soft off-white page, one blue accent, the text, muted, error, success and warning
// colours); the state tokens of the Fluent design (card, border, hover, pressed, focus, selection, chips) are derived here only.
// Type follows the Windows 11 ramp in Segoe UI Variable (fallback Segoe UI): caption 12, body 14, body strong 14 semibold,
// subtitle 20 semibold, title 28 semibold. Every size follows the dpi of the window.

#include <windows.h>
#include <commctrl.h>

// The kind of text a static control shows. A control without a role is normal text.
enum themerole
{
    ROLE_TEXT,       // normal text (body 14)
    ROLE_TITLE,      // the page title (title 28 semibold)
    ROLE_MUTED,      // captions and helper lines (caption 12, muted)
    ROLE_ERROR,      // the one validation or error line (body, error colour)
    ROLE_APPNAME,    // the program name next to the logo in the sidebar (body strong)
    ROLE_SIDETEXT,   // normal text on the sidebar
    ROLE_SIDEMUTED,  // a muted caption on the sidebar
    ROLE_WELCOME,    // the big title of the welcome page (title large 40 semibold)
    ROLE_VALUE,      // the number of a stat tile (title 28 semibold)
    ROLE_LABEL,      // the label above a field (body strong 14)
    ROLE_BANNER,     // the text of a warning banner (body, warning colour on the warning tint)
    ROLE_DESC,       // the one line under a page title (body 14, muted)
    ROLE_EMPTY,      // the first line of an empty state (16 semibold)
    ROLE_HINT,       // a muted body line (second line of an empty state, helper lines)
    ROLE_STRONG      // body strong text (the name in the account card)
};

// The fonts, in design pixels at 96 dpi.
enum themefontkind
{
    FONT_BODY,      // 14 normal
    FONT_TITLE,     // 28 semibold
    FONT_CAPTION,   // 12 normal
    FONT_SECTION,   // 17 semibold (kept for the old roles)
    FONT_WELCOME,   // 40 semibold
    FONT_LABEL,     // 14 semibold
    FONT_STRONG,    // 14 semibold
    FONT_SUBTITLE,  // 20 semibold
    FONT_HEADER,    // 12 semibold (list headers and sidebar section labels)
    FONT_VALUE,     // 28 semibold (stat tiles)
    FONT_CARDTITLE, // 18 semibold (role cards)
    FONT_EMPTY,     // 16 semibold (empty states)
    FONT_COUNT
};

// The colours of the palette and the derived state tokens.
enum themecolorkind
{
    COLOR_PAGE,           // the page background
    COLOR_SIDEBAR,        // the sidebar background
    COLOR_TEXT,           // normal text
    COLOR_MUTED,          // captions and helper lines
    COLOR_ACCENT,         // the one blue accent
    COLOR_ERROR,          // error text
    COLOR_SELECT,         // the chosen line of the welcome name list
    COLOR_ERRORFILL,      // the error tint (flagged rows)
    COLOR_CARD,           // the card surface
    COLOR_BORDER,         // the 1 px card and field border
    COLOR_HOVER,          // a hovered row, item or secondary button
    COLOR_PRESSED,        // a pressed secondary button or item
    COLOR_ACCENTHOVER,    // a hovered primary button
    COLOR_ACCENTPRESSED,  // a pressed primary button
    COLOR_ACCENTTINT,     // the avatar circle of the account card in the sidebar
    COLOR_SELECTFILL,     // a selected list row or sidebar item
    COLOR_HEADERTINT,     // the header row of a list
    COLOR_SUCCESS,        // success text
    COLOR_SUCCESSFILL,    // success tint
    COLOR_WARNING,        // warning text
    COLOR_WARNINGFILL,    // warning tint (banners)
    COLOR_NEUTRALFILL,    // neutral chip tint
    COLOR_DISABLED,       // disabled text
    COLOR_DISABLEDFILL,   // disabled fill
    COLOR_ONACCENT,       // text on the accent (white)
    COLOR_KINDS
};

// Reads the dpi and the fonts, makes the brushes. Call once, before any window is made.
void themeinit();

// The dpi that all sizes follow now (96 means 100 percent).
int themedpi();

// A new dpi: the fonts are made again; the old ones live until themereleasefonts().
void themesetdpi(int dpi);

// Deletes the fonts that themesetdpi replaced.
void themereleasefonts();

// Test modes only: makes themedpiforwindow() answer this dpi (0 switches the override off).
void themesetdpioverride(int dpi);

// Tells whether a test dpi is set.
bool themedpiisoverridden();

// The dpi of a window: the override, else GetDpiForWindow (loaded at run time), else the dpi that themeinit read.
int themedpiforwindow(HWND window);

// One of the fonts (it lives until the next dpi change).
HFONT themefont(int kind);

// The height of the body font at a dpi (negative, as in LOGFONT).
int themebodyheight(int dpi);

// One colour of the palette or the state tokens.
COLORREF themecolorof(int kind);

// Remembers the text role of a static control.
void themesetrole(HWND control, int role);

// The role of a control (ROLE_TEXT when none was set).
int themegetrole(HWND control);

// Gives every control below the parent the font of its role.
void themeapplyfonts(HWND parent);

// A list of a page: card colours and rows of 40 design pixels. Call again after a dpi change.
void themestylelist(HWND list);

// Styles every list below the parent again (after a dpi change).
void themerestylelists(HWND parent);

// Draws the header of a list: 12 semibold muted titles on the header tint with a 1 px line under it.
LRESULT themedrawheader(NMCUSTOMDRAW* draw);

// The height of a line of an owner drawn list box (the welcome name list): 40 design pixels.
int themelistboxheight();

// Draws one line of the welcome name list: the selected line has the selection fill and a 3 px accent bar.
void themedrawlistbox(const DRAWITEMSTRUCT* item);

// Answers the WM_CTLCOLOR messages of statics, buttons, edit boxes and list boxes on the page tone.
bool themecolour(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result);

// The same, for a control that sits on a card (the card colour is its background).
bool themecolouron(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result, bool oncard);

// The brushes of the page, the sidebar and the cards.
HBRUSH themepagebrush();
HBRUSH themesidebarbrush();
HBRUSH themecardbrush();

#endif
