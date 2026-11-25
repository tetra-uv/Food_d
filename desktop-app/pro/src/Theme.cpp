// Theme.cpp (professional program): the one place that knows colours, fonts and brushes.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>

#include "Theme.h"
#include "ProDraw.h"

#include <string.h>
#include <vector>

using namespace std;

// The property name under which a control remembers its role.
static const char* const roleproperty = "cbrole";

// The design sizes (at 96 dpi) of a list row and of a line of the name list.
static const int listrowdesign = 40;

static int dpivalue = 96;           // the dpi that every size follows now
static int dpioverride = 0;         // the dpi a test asked for (0 means none)
static HFONT fonts[FONT_COUNT];     // the fonts of the present dpi
static vector<HFONT> retired;       // fonts that were replaced and are deleted later
static HBRUSH pagebrush = NULL;     // page background
static HBRUSH sidebarbrush = NULL;  // sidebar background
static HBRUSH cardbrush = NULL;     // card surface
static HBRUSH bannerbrush = NULL;   // warning tint of a banner
static char textface[LF_FACESIZE] = "Segoe UI";    // the face for body sizes (Segoe UI Variable Text when it exists)
static char displayface[LF_FACESIZE] = "Segoe UI"; // the face for the big sizes (Segoe UI Variable Display when it exists)

// The palette of the old program (DESIGN_GUIDE.md) and the derived state tokens of the Fluent design. Index = themecolorkind.
static const COLORREF palette[COLOR_KINDS] = {
    RGB(250, 248, 244),  // COLOR_PAGE: soft warm paper
    RGB(240, 237, 230),  // COLOR_SIDEBAR: a little deeper than the page
    RGB(10, 19, 23),     // COLOR_TEXT
    RGB(78, 96, 111),    // COLOR_MUTED
    RGB(0, 100, 224),    // COLOR_ACCENT
    RGB(123, 2, 16),     // COLOR_ERROR
    RGB(204, 230, 254),  // COLOR_SELECT
    RGB(249, 209, 216),  // COLOR_ERRORFILL
    RGB(255, 255, 255),  // COLOR_CARD
    RGB(228, 225, 219),  // COLOR_BORDER: the page mixed about 9 percent toward the text
    RGB(243, 241, 237),  // COLOR_HOVER: the card mixed about 6 percent toward the text
    RGB(233, 230, 225),  // COLOR_PRESSED: about 10 percent
    RGB(0, 90, 202),     // COLOR_ACCENTHOVER: the accent 10 percent toward black
    RGB(0, 82, 184),     // COLOR_ACCENTPRESSED: 18 percent toward black
    RGB(224, 236, 252),  // COLOR_ACCENTTINT: 12 percent accent over white
    RGB(235, 243, 253),  // COLOR_SELECTFILL: 8 percent accent over white
    RGB(247, 245, 241),  // COLOR_HEADERTINT
    RGB(9, 68, 31),      // COLOR_SUCCESS
    RGB(206, 235, 210),  // COLOR_SUCCESSFILL
    RGB(117, 63, 7),     // COLOR_WARNING
    RGB(249, 237, 204),  // COLOR_WARNINGFILL
    RGB(236, 233, 227),  // COLOR_NEUTRALFILL
    RGB(124, 132, 138),  // COLOR_DISABLED
    RGB(233, 231, 227),  // COLOR_DISABLEDFILL
    RGB(255, 255, 255)   // COLOR_ONACCENT
};

// One colour by its kind.
COLORREF themecolorof(int kind)
{
    if (kind < 0 || kind >= COLOR_KINDS)
    {
        return palette[COLOR_PAGE];
    }

    return palette[kind];
}

// The answer of the font probe: the family exists.
static int CALLBACK facefound(const LOGFONTA* font, const TEXTMETRICA* metric, DWORD kind, LPARAM flag)
{
    (void)font;
    (void)metric;
    (void)kind;
    *(bool*)flag = true;
    return 0;
}

// True when a font family is installed.
static bool faceexists(const char* family)
{
    LOGFONTA want;
    ZeroMemory(&want, sizeof(want));
    want.lfCharSet = DEFAULT_CHARSET;
    lstrcpynA(want.lfFaceName, family, LF_FACESIZE);
    bool exists = false;
    HDC screen = GetDC(NULL);
    EnumFontFamiliesExA(screen, &want, (FONTENUMPROCA)facefound, (LPARAM)&exists, 0);
    ReleaseDC(NULL, screen);
    return exists;
}

// Segoe UI Variable has optical faces for body text and for display sizes; without it both are Segoe UI.
static void probefaces()
{
    if (faceexists("Segoe UI Variable Text") && faceexists("Segoe UI Variable Display"))
    {
        lstrcpynA(textface, "Segoe UI Variable Text", LF_FACESIZE);
        lstrcpynA(displayface, "Segoe UI Variable Display", LF_FACESIZE);
    }
}

// Makes one font: a size in design pixels at the present dpi, a weight, and the display face for the big sizes.
static HFONT makefont(int design, int weight)
{
    LOGFONTA font;
    ZeroMemory(&font, sizeof(font));
    lstrcpynA(font.lfFaceName, design >= 20 ? displayface : textface, LF_FACESIZE);
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfHeight = -MulDiv(design, dpivalue, 96);
    font.lfWeight = weight;
    font.lfQuality = CLEARTYPE_QUALITY;
    return CreateFontIndirectA(&font);
}

// Makes the fonts for the present dpi. The fonts that were there before are kept in the retired list.
static void makefonts()
{
    for (int kind = 0; kind < FONT_COUNT; kind++)
    {
        if (fonts[kind] != NULL)
        {
            retired.push_back(fonts[kind]);
        }
    }

    fonts[FONT_BODY] = makefont(14, FW_NORMAL);
    fonts[FONT_TITLE] = makefont(28, FW_SEMIBOLD);
    fonts[FONT_CAPTION] = makefont(12, FW_NORMAL);
    fonts[FONT_SECTION] = makefont(17, FW_SEMIBOLD);
    fonts[FONT_WELCOME] = makefont(40, FW_SEMIBOLD);
    fonts[FONT_LABEL] = makefont(14, FW_SEMIBOLD);
    fonts[FONT_STRONG] = makefont(14, FW_SEMIBOLD);
    fonts[FONT_SUBTITLE] = makefont(20, FW_SEMIBOLD);
    fonts[FONT_HEADER] = makefont(12, FW_SEMIBOLD);
    fonts[FONT_VALUE] = makefont(28, FW_SEMIBOLD);
    fonts[FONT_CARDTITLE] = makefont(18, FW_SEMIBOLD);
    fonts[FONT_EMPTY] = makefont(16, FW_SEMIBOLD);
}

// The Windows 10 functions that tell the dpi of a window and of the system (loaded by name, the old headers lack them).
typedef UINT(WINAPI* dpiforwindow)(HWND);
typedef UINT(WINAPI* dpiforsystem)(void);

// Reads the system dpi, probes the faces, makes the fonts and the brushes.
void themeinit()
{
    for (int kind = 0; kind < FONT_COUNT; kind++)
    {
        fonts[kind] = NULL;
    }

    HMODULE user = GetModuleHandleA("user32.dll");
    dpiforsystem system = user == NULL ? NULL : (dpiforsystem)GetProcAddress(user, "GetDpiForSystem");

    if (system != NULL)
    {
        dpivalue = (int)system();
    }
    else
    {
        HDC screen = GetDC(NULL);
        dpivalue = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(NULL, screen);
    }

    if (dpivalue < 96)
    {
        dpivalue = 96;
    }

    probefaces();
    makefonts();
    pagebrush = CreateSolidBrush(palette[COLOR_PAGE]);
    sidebarbrush = CreateSolidBrush(palette[COLOR_SIDEBAR]);
    cardbrush = CreateSolidBrush(palette[COLOR_CARD]);
    bannerbrush = CreateSolidBrush(palette[COLOR_WARNINGFILL]);
}

// The dpi that every size follows now.
int themedpi()
{
    return dpivalue;
}

// A new dpi: the fonts are made again for it.
void themesetdpi(int dpi)
{
    dpivalue = dpi < 96 ? 96 : dpi;
    makefonts();
}

// The replaced fonts can go now: every control has a new one.
void themereleasefonts()
{
    for (size_t pos = 0; pos < retired.size(); pos++)
    {
        DeleteObject(retired[pos]);
    }

    retired.clear();
}

// A test asks for a dpi that is not the one of the screen.
void themesetdpioverride(int dpi)
{
    dpioverride = dpi;
}

// True when a test asked for a dpi.
bool themedpiisoverridden()
{
    return dpioverride > 0;
}

// The dpi of a window.
int themedpiforwindow(HWND window)
{
    if (dpioverride > 0)
    {
        return dpioverride;
    }

    HMODULE user = GetModuleHandleA("user32.dll");
    dpiforwindow function = user == NULL ? NULL : (dpiforwindow)GetProcAddress(user, "GetDpiForWindow");

    if (function != NULL && window != NULL)
    {
        UINT value = function(window);

        if (value >= 96)
        {
            return (int)value;
        }
    }

    return dpivalue;
}

// The height of the body font at a dpi: 14 design pixels, negative like LOGFONT.
int themebodyheight(int dpi)
{
    return -MulDiv(14, dpi, 96);
}

// One of the fonts.
HFONT themefont(int kind)
{
    return fonts[kind];
}

// The brushes.
HBRUSH themepagebrush()
{
    return pagebrush;
}

HBRUSH themesidebarbrush()
{
    return sidebarbrush;
}

HBRUSH themecardbrush()
{
    return cardbrush;
}

// The role lives in a window property, so statics need no subclassing.
void themesetrole(HWND control, int role)
{
    SetPropA(control, roleproperty, (HANDLE)(INT_PTR)role);
}

// A control without the property has the role ROLE_TEXT (0).
int themegetrole(HWND control)
{
    return (int)(INT_PTR)GetPropA(control, roleproperty);
}

// The font of a role.
static int fontofrole(int role)
{
    if (role == ROLE_TITLE || role == ROLE_VALUE)
    {
        return role == ROLE_TITLE ? FONT_TITLE : FONT_VALUE;
    }

    if (role == ROLE_WELCOME)
    {
        return FONT_WELCOME;
    }

    if (role == ROLE_MUTED || role == ROLE_SIDEMUTED)
    {
        return FONT_CAPTION;
    }

    if (role == ROLE_APPNAME || role == ROLE_STRONG || role == ROLE_LABEL)
    {
        return FONT_STRONG;
    }

    if (role == ROLE_EMPTY)
    {
        return FONT_EMPTY;
    }

    return FONT_BODY;
}

// Called once for every control below the parent.
static BOOL CALLBACK applyfontproc(HWND control, LPARAM unused)
{
    (void)unused;
    SendMessageA(control, WM_SETFONT, (WPARAM)themefont(fontofrole(themegetrole(control))), FALSE);
    return TRUE;
}

// Every control below the parent gets the font of its role.
void themeapplyfonts(HWND parent)
{
    EnumChildWindows(parent, applyfontproc, 0);
}

// The old Windows headers do not know this flag: the list paints into an off-screen bitmap, which avoids flicker.
#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

// An empty image list of this height sets the row height of a report list; the old one is deleted.
static void setspacer(HWND list, int height)
{
    HIMAGELIST spacer = ImageList_Create(1, height, ILC_COLOR, 1, 1);
    HIMAGELIST old = (HIMAGELIST)SendMessageA(list, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)spacer);

    if (old != NULL)
    {
        ImageList_Destroy(old);
    }
}

// A list of a page sits on a card: card colours, rows of 40 design pixels, the header font for the header.
void themestylelist(HWND list)
{
    SendMessageA(list, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    SendMessageA(list, LVM_SETBKCOLOR, 0, (LPARAM)palette[COLOR_CARD]);
    SendMessageA(list, LVM_SETTEXTBKCOLOR, 0, (LPARAM)palette[COLOR_CARD]);
    SendMessageA(list, LVM_SETTEXTCOLOR, 0, (LPARAM)palette[COLOR_TEXT]);
    // the default list look: the "Explorer" look draws the column dividers down into the empty part of a list
    setspacer(list, MulDiv(listrowdesign, dpivalue, 96));
    HWND header = (HWND)SendMessageA(list, LVM_GETHEADER, 0, 0);

    if (header != NULL)
    {
        SendMessageA(header, WM_SETFONT, (WPARAM)fonts[FONT_HEADER], TRUE);
    }
}

// Called once for every window below the parent: lists and owner drawn list boxes follow the new dpi.
static BOOL CALLBACK restyleproc(HWND control, LPARAM unused)
{
    (void)unused;
    char name[64];
    name[0] = '\0';
    GetClassNameA(control, name, (int)sizeof(name));

    if (strcmp(name, "ListBox") == 0 && (GetWindowLongA(control, GWL_STYLE) & LBS_OWNERDRAWFIXED) != 0)
    {
        SendMessageA(control, LB_SETITEMHEIGHT, 0, (LPARAM)themelistboxheight());
    }

    if (strcmp(name, WC_LISTVIEWA) == 0)
    {
        themestylelist(control);
    }

    return TRUE;
}

// Every list below the parent is styled again for the present dpi.
void themerestylelists(HWND parent)
{
    EnumChildWindows(parent, restyleproc, 0);
}

// The header: the tint, 12 semibold muted titles 12 design pixels from the edge (right aligned titles for number columns),
// a 1 px line under it; the part right of the last column gets the tint as well.
LRESULT themedrawheader(NMCUSTOMDRAW* draw)
{
    HWND header = draw->hdr.hwndFrom;
    COLORREF tint = palette[COLOR_HEADERTINT];

    if (draw->dwDrawStage == CDDS_PREPAINT)
    {
        RECT all;
        GetClientRect(header, &all);
        HBRUSH brush = CreateSolidBrush(tint);
        FillRect(draw->hdc, &all, brush);
        DeleteObject(brush);
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    }

    if (draw->dwDrawStage == CDDS_ITEMPREPAINT)
    {
        char text[128];
        text[0] = '\0';
        HDITEMA item;
        ZeroMemory(&item, sizeof(item));
        item.mask = HDI_TEXT | HDI_FORMAT;
        item.pszText = text;
        item.cchTextMax = (int)sizeof(text);
        SendMessageA(header, HDM_GETITEMA, (WPARAM)draw->dwItemSpec, (LPARAM)&item);
        HBRUSH brush = CreateSolidBrush(tint);
        FillRect(draw->hdc, &draw->rc, brush);
        DeleteObject(brush);
        RECT area = draw->rc;
        int inset = MulDiv(12, dpivalue, 96);
        area.left = area.left + inset;
        area.right = area.right - inset;
        UINT align = (item.fmt & HDF_RIGHT) != 0 ? DT_RIGHT : DT_LEFT;
        protext(draw->hdc, text, area, palette[COLOR_MUTED], fonts[FONT_HEADER], align | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        return CDRF_SKIPDEFAULT;
    }

    if (draw->dwDrawStage == CDDS_POSTPAINT)
    {
        RECT all;
        GetClientRect(header, &all);
        int count = (int)SendMessageA(header, HDM_GETITEMCOUNT, 0, 0);
        RECT last;
        ZeroMemory(&last, sizeof(last));

        if (count > 0)
        {
            SendMessageA(header, HDM_GETITEMRECT, (WPARAM)(count - 1), (LPARAM)&last);
        }

        RECT rest = all;
        rest.left = last.right;
        HBRUSH brush = CreateSolidBrush(tint);

        if (rest.left < rest.right)
        {
            FillRect(draw->hdc, &rest, brush);
        }

        DeleteObject(brush);
        RECT line = all;
        line.top = line.bottom - 1;
        HBRUSH border = CreateSolidBrush(palette[COLOR_BORDER]);
        FillRect(draw->hdc, &line, border);
        DeleteObject(border);
    }

    return CDRF_DODEFAULT;
}

// A line of the name list is 40 design pixels high.
int themelistboxheight()
{
    return MulDiv(listrowdesign, dpivalue, 96);
}

// One line of the name list on its card: the selected line has the selection fill and a 3 px accent bar.
void themedrawlistbox(const DRAWITEMSTRUCT* item)
{
    if (item->itemID == (UINT)-1)
    {
        return;
    }

    bool chosen = (item->itemState & ODS_SELECTED) != 0;
    FillRect(item->hDC, &item->rcItem, cardbrush);

    if (chosen)
    {
        RECT fill = item->rcItem;
        InflateRect(&fill, -MulDiv(4, dpivalue, 96), -MulDiv(2, dpivalue, 96));
        proround(item->hDC, fill, MulDiv(4, dpivalue, 96), palette[COLOR_SELECTFILL], CLR_INVALID);
        RECT bar = {fill.left, fill.top + MulDiv(10, dpivalue, 96), fill.left + MulDiv(3, dpivalue, 96), fill.bottom - MulDiv(10, dpivalue, 96)};
        proround(item->hDC, bar, MulDiv(2, dpivalue, 96), palette[COLOR_ACCENT], CLR_INVALID);
    }

    char text[256];
    text[0] = '\0';
    SendMessageA(item->hwndItem, LB_GETTEXT, (WPARAM)item->itemID, (LPARAM)text);
    RECT area = item->rcItem;
    area.left = area.left + MulDiv(16, dpivalue, 96);
    protext(item->hDC, text, area, palette[COLOR_TEXT], (HFONT)SendMessageA(item->hwndItem, WM_GETFONT, 0, 0),
            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    if ((item->itemState & ODS_FOCUS) != 0 && (SendMessageA(item->hwndItem, WM_QUERYUISTATE, 0, 0) & 0x1) == 0)
    {
        RECT ring = item->rcItem;
        InflateRect(&ring, -MulDiv(2, dpivalue, 96), -MulDiv(1, dpivalue, 96));
        proring(item->hDC, ring, MulDiv(4, dpivalue, 96));
    }
}

// The text colour and the background of a control from its role, on the page tone or on a card.
bool themecolouron(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result, bool oncard)
{
    HDC dc = (HDC)wparam;

    if (msg == WM_CTLCOLORLISTBOX || msg == WM_CTLCOLOREDIT)
    {
        SetTextColor(dc, palette[COLOR_TEXT]);
        SetBkColor(dc, palette[COLOR_CARD]);
        result = (LRESULT)cardbrush;
        return true;
    }

    if (msg != WM_CTLCOLORSTATIC && msg != WM_CTLCOLORBTN)
    {
        return false;
    }

    int role = themegetrole((HWND)lparam);
    bool sidebar = (role == ROLE_SIDETEXT || role == ROLE_SIDEMUTED || role == ROLE_APPNAME);
    COLORREF text = palette[COLOR_TEXT];

    if (role == ROLE_MUTED || role == ROLE_SIDEMUTED || role == ROLE_DESC || role == ROLE_HINT)
    {
        text = palette[COLOR_MUTED];
    }
    else if (role == ROLE_ERROR)
    {
        text = palette[COLOR_ERROR];
    }

    if (role == ROLE_BANNER)
    {
        SetTextColor(dc, palette[COLOR_WARNING]);
        SetBkColor(dc, palette[COLOR_WARNINGFILL]);
        result = (LRESULT)bannerbrush;
        return true;
    }

    COLORREF back = sidebar ? palette[COLOR_SIDEBAR] : (oncard ? palette[COLOR_CARD] : palette[COLOR_PAGE]);
    SetTextColor(dc, text);
    SetBkColor(dc, back);
    result = (LRESULT)(sidebar ? sidebarbrush : (oncard ? cardbrush : pagebrush));
    return true;
}

// On the page tone.
bool themecolour(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result)
{
    return themecolouron(msg, wparam, lparam, result, false);
}
