// GuiShell.cpp (professional program): the main window with its sidebar (logo tile, section labels, icon items, account card),
// the page host and the toast for success messages. Rounded window corners and the caption colour come from DWM.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <gdiplus/gdiplus.h>

#include "GuiShell.h"
#include "GuiLayout.h"
#include "PageDonorEdit.h"
#include "Pages.h"
#include "ProDraw.h"
#include "ProIcons.h"
#include "ProMotion.h"
#include "Theme.h"
#include "resource.h"

#include <fstream>
#include <vector>

using namespace std;

// The old Windows headers do not know the message that says the dpi of the window changed.
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

// The old Windows headers do not know the keyboard cue flags.
#ifndef UIS_SET
#define UIS_SET 1
#endif
#ifndef UISF_HIDEFOCUS
#define UISF_HIDEFOCUS 0x1
#endif
#ifndef WM_QUERYUISTATE
#define WM_QUERYUISTATE 0x0129
#endif

// One item of the sidebar: the page it opens, its text, its glyph and the section label shown above it (NULL for none).
struct navitem
{
    int page;
    const char* text;
    unsigned int glyph;
    const char* section;
};

// The sidebar of the coordinator: OVERVIEW, MANAGE, REPORTS.
static const navitem coordnav[] = {{PAGE_C_HOME, "Home", ICON_HOME, "OVERVIEW"},
                                   {PAGE_C_DONATIONS, "Donations", ICON_PACKAGE, "MANAGE"},
                                   {PAGE_C_OFFERS, "Offers", ICON_SEND, NULL},
                                   {PAGE_C_RECIPIENTS, "Recipients", ICON_PEOPLE, NULL},
                                   {PAGE_C_TRANSACTIONS, "Transactions", ICON_LIST, NULL},
                                   {PAGE_C_SUMMARY, "Summary", ICON_BULLETS, "REPORTS"},
                                   {PAGE_NONE, NULL, 0, NULL}};

// The sidebar of a donor: OVERVIEW, MY WORK.
static const navitem donornav[] = {{PAGE_D_HOME, "Home", ICON_HOME, "OVERVIEW"},
                                   {PAGE_D_DONATIONS, "My donations", ICON_PACKAGE, "MY WORK"},
                                   {PAGE_D_OFFERS, "Offers", ICON_SEND, NULL},
                                   {PAGE_D_GIVEN, "Given out", ICON_ACCEPT, NULL},
                                   {PAGE_D_ADD, "Add donation", ICON_ADD, NULL},
                                   {PAGE_NONE, NULL, 0, NULL}};

// The sidebar of a recipient: OVERVIEW, OFFERS, MY ORGANISATION.
static const navitem recipnav[] = {{PAGE_R_HOME, "Home", ICON_HOME, "OVERVIEW"},
                                   {PAGE_R_INCOMING, "Incoming offers", ICON_MAIL, "OFFERS"},
                                   {PAGE_R_PAST, "Past offers", ICON_CALENDAR, NULL},
                                   {PAGE_R_RECEIVED, "Received food", ICON_CART, "MY ORGANISATION"},
                                   {PAGE_R_EDIT, "My details", ICON_CONTACT, NULL},
                                   {PAGE_NONE, NULL, 0, NULL}};

// The class names of the main window, the page host, the sidebar panel and the toast.
static const char* const mainclass = "CommunityBridgeProMain";
static const char* const hostclass = "CommunityBridgeProHost";
static const char* const sideclass = "CommunityBridgeProSidebar";
static const char* const toastclass = "CommunityBridgeProToast";

// The timer that hides the toast after four seconds.
static const UINT_PTR toasttimer = 41;

// Everything the shell keeps in memory.
struct shellstate
{
    shellparts parts;            // the windows
    HWND whocaption;             // "Signed in as"
    HWND whorole;                // the role of the person
    HWND whoname;                // the name of the person
    HWND switchuser;             // the Switch user button
    HWND toast;                  // the toast window (a layered popup owned by the main window)
    vector<guipage*> pages;      // every page, made once
    vector<int> navpages;        // the pages of the sidebar items, in order
    vector<int> navrows;         // the list box row of each sidebar item (the section labels are rows too)
    vector<unsigned int> navglyphs; // the glyph of each list box row (0 for a section label)
    RECT accountcard;            // the account card in sidebar coordinates
    int navhot;                  // the list box row under the mouse, or -1
    int current;                 // the page that is shown
    int previous;                // the page that was shown before
    int who;                     // who is signed in
    string personid;             // id of the person
    string personname;           // name of the person
    bool sidebar;                // whether the sidebar is shown
    bool syncing;                // true while the program itself changes the selected sidebar item
    string statustext;           // the text of the last toast
    HWND lastfocus;              // the control that had the focus when the window was deactivated
    bool allowhuge;              // true in a test: the window may be bigger than the screen
    string dpilog;               // the dpi log file (--dpi-log), empty when there is none
    int dpiline;                 // the source line of the WM_DPICHANGED case once it ran
};

static shellstate state;

// Makes every page once. They are only shown and hidden after this.
static void buildpages(HWND host)
{
    state.pages.push_back(newpagewelcome());
    state.pages.push_back(newpagedonorhome());
    state.pages.push_back(newpagedonordonations());
    state.pages.push_back(newpagedonoroffers());
    state.pages.push_back(newpagedonorgiven());
    state.pages.push_back(newpagedonoradd());
    state.pages.push_back(newpagedonoredit());
    state.pages.push_back(newpagecoordhome());
    state.pages.push_back(newpagecoorddonations());
    state.pages.push_back(newpagecoordoffers());
    state.pages.push_back(newpagecoordrecipients());
    state.pages.push_back(newpagecoordtransactions());
    state.pages.push_back(newpagecoordsummary());
    state.pages.push_back(newpagematch());
    state.pages.push_back(newpagerecipienthome());
    state.pages.push_back(newpagerecipientincoming());
    state.pages.push_back(newpagerecipientpast());
    state.pages.push_back(newpagerecipientreceived());
    state.pages.push_back(newpagerecipientedit());

    for (size_t pos = 0; pos < state.pages.size(); pos++)
    {
        state.pages[pos]->build(host);
    }
}

// Finds a page by number.
guipage* shellpage(int pageid)
{
    for (size_t pos = 0; pos < state.pages.size(); pos++)
    {
        if (state.pages[pos]->id() == pageid)
        {
            return state.pages[pos];
        }
    }

    return NULL;
}

// The windows of the shell.
const shellparts& shellgetparts()
{
    return state.parts;
}

// The page that is shown.
int shellcurrent()
{
    return state.current;
}

// The page before the present one.
int shellprevious()
{
    return state.previous;
}

// Who is signed in.
int shellwho()
{
    return state.who;
}

// The id of the person.
const string& shellpersonid()
{
    return state.personid;
}

// The name of the person.
const string& shellpersonname()
{
    return state.personname;
}

// The sidebar is only shown for a signed-in person.
bool shellsidebarshown()
{
    return state.sidebar;
}

// Returns the sidebar items of a role.
static const navitem* navof(int who)
{
    if (who == WHO_COORDINATOR)
    {
        return coordnav;
    }

    if (who == WHO_DONOR)
    {
        return donornav;
    }

    if (who == WHO_RECIPIENT)
    {
        return recipnav;
    }

    return NULL;
}

// Number of sidebar items (the section labels do not count).
int shellnavcount()
{
    return (int)state.navpages.size();
}

// The page that a sidebar item opens.
int shellnavpage(int index)
{
    if (index < 0 || index >= (int)state.navpages.size())
    {
        return PAGE_NONE;
    }

    return state.navpages[index];
}

// The selected sidebar item (from the selected list box row).
int shellnavselected()
{
    int row = (int)SendMessageA(state.parts.nav, LB_GETCURSEL, 0, 0);

    for (size_t pos = 0; pos < state.navrows.size(); pos++)
    {
        if (state.navrows[pos] == row)
        {
            return (int)pos;
        }
    }

    return -1;
}

// The rectangle of a sidebar item on the screen.
RECT shellnavrect(int index)
{
    RECT item;
    ZeroMemory(&item, sizeof(item));

    if (index >= 0 && index < (int)state.navrows.size())
    {
        SendMessageA(state.parts.nav, LB_GETITEMRECT, (WPARAM)state.navrows[index], (LPARAM)&item);
        MapWindowPoints(state.parts.nav, NULL, (POINT*)&item, 2);
    }

    return item;
}

// Selects a sidebar item as a click does: the row is selected and its page opens.
void shellnavselect(int index)
{
    if (index < 0 || index >= (int)state.navrows.size())
    {
        return;
    }

    SendMessageA(state.parts.nav, LB_SETCURSEL, (WPARAM)state.navrows[index], 0);
    shellshow(state.navpages[index], false);
}

// The toast window: shadow layers, a card in the success tint with a thin border and the text (no bar, no glyph), drawn with per pixel
// alpha (GDI+ for the shapes, GDI ClearType for the text on the opaque card) and shown with UpdateLayeredWindow.
static void drawtoast(const string& text)
{
    int shadow = px(12);
    int height = px(56);
    HDC screen = GetDC(NULL);
    HGDIOBJ oldfont = SelectObject(screen, themefont(FONT_BODY));
    SIZE size;
    GetTextExtentPoint32A(screen, text.c_str(), (int)text.length(), &size);
    SelectObject(screen, oldfont);
    int cardwidth = size.cx + px(40);

    if (cardwidth > px(440))
    {
        cardwidth = px(440);
    }

    int width = cardwidth + 2 * shadow;
    int total = height + 2 * shadow;
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -total;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    HDC memory = CreateCompatibleDC(screen);
    HGDIOBJ oldbitmap = SelectObject(memory, bitmap);
    ZeroMemory(bits, (size_t)width * total * 4);

    {
        Gdiplus::Bitmap canvas(width, total, width * 4, PixelFormat32bppPARGB, (BYTE*)bits);
        Gdiplus::Graphics graphics(&canvas);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // the soft shadow: a few translucent rounded rectangles, each a little larger and fainter
        for (int layer = 3; layer >= 1; layer--)
        {
            int grow = layer * px(3);
            Gdiplus::GraphicsPath path;
            float x = (float)(shadow - grow);
            float y = (float)(shadow - grow + px(2));
            float w = (float)(cardwidth + 2 * grow);
            float h = (float)(height + 2 * grow);
            float d = (float)(px(8) + grow) * 2.0f;
            path.AddArc(x, y, d, d, 180, 90);
            path.AddArc(x + w - d, y, d, d, 270, 90);
            path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
            path.AddArc(x, y + h - d, d, d, 90, 90);
            path.CloseFigure();
            Gdiplus::SolidBrush brush(Gdiplus::Color((BYTE)(8 + (3 - layer) * 6), 10, 19, 23));
            graphics.FillPath(&brush, &path);
        }

        Gdiplus::GraphicsPath card;
        float d = (float)px(8) * 2.0f;
        float x = (float)shadow;
        float y = (float)shadow;
        float w = (float)cardwidth;
        float h = (float)height;
        card.AddArc(x, y, d, d, 180, 90);
        card.AddArc(x + w - d, y, d, d, 270, 90);
        card.AddArc(x + w - d, y + h - d, d, d, 0, 90);
        card.AddArc(x, y + h - d, d, d, 90, 90);
        card.CloseFigure();
        // the thin border is the whole card in the border colour, the tint is the same card 1 px smaller on every side
        COLORREF edge = promix(themecolorof(COLOR_SUCCESSFILL), themecolorof(COLOR_SUCCESS), 40);
        Gdiplus::SolidBrush edgebrush(Gdiplus::Color(255, GetRValue(edge), GetGValue(edge), GetBValue(edge)));
        graphics.FillPath(&edgebrush, &card);
        Gdiplus::GraphicsPath inner;
        float bx = x + 1.0f;
        float by = y + 1.0f;
        float bw = w - 2.0f;
        float bh = h - 2.0f;
        float bd = d - 2.0f;
        inner.AddArc(bx, by, bd, bd, 180, 90);
        inner.AddArc(bx + bw - bd, by, bd, bd, 270, 90);
        inner.AddArc(bx + bw - bd, by + bh - bd, bd, bd, 0, 90);
        inner.AddArc(bx, by + bh - bd, bd, bd, 90, 90);
        inner.CloseFigure();
        COLORREF tint = themecolorof(COLOR_SUCCESSFILL);
        Gdiplus::SolidBrush tintbrush(Gdiplus::Color(255, GetRValue(tint), GetGValue(tint), GetBValue(tint)));
        graphics.FillPath(&tintbrush, &inner);
    }

    RECT label = {shadow + px(20), shadow, shadow + cardwidth - px(16), shadow + height};
    protext(memory, text, label, themecolorof(COLOR_TEXT), themefont(FONT_BODY), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // GDI clears the alpha of the pixels it writes: the inside of the card is opaque, so its alpha is set again
    unsigned char* pixel = (unsigned char*)bits;

    for (int row = shadow + px(4); row < shadow + height - px(4); row++)
    {
        for (int column = shadow + px(8); column < shadow + cardwidth - px(4); column++)
        {
            pixel[(row * width + column) * 4 + 3] = 255;
        }
    }

    RECT client;
    GetClientRect(state.parts.main, &client);
    POINT corner = {client.right - width - px(8), client.bottom - total - px(8)};
    ClientToScreen(state.parts.main, &corner);
    POINT origin = {0, 0};
    SIZE extent = {width, total};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(state.toast, screen, &corner, &extent, memory, &origin, 0, &blend, ULW_ALPHA);
    SelectObject(memory, oldbitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(NULL, screen);
}

// Shows the toast (or hides it for an empty text) and remembers the text for the tests.
void shellstatus(const string& text)
{
    state.statustext = text;

    if (state.toast == NULL)
    {
        return;
    }

    if (text.empty() || !IsWindowVisible(state.parts.main) || IsIconic(state.parts.main))
    {
        ShowWindow(state.toast, SW_HIDE);
        KillTimer(state.parts.main, toasttimer);
        return;
    }

    drawtoast(text);
    ShowWindow(state.toast, SW_SHOWNOACTIVATE);
    SetTimer(state.parts.main, toasttimer, 4000, NULL);
}

// The text of the last toast.
string shellstatustext()
{
    return state.statustext;
}

// Returns the size of the client area.
void shellgetclient(int& width, int& height)
{
    RECT area;
    GetClientRect(state.parts.main, &area);
    width = area.right - area.left;
    height = area.bottom - area.top;
}

// Puts every control of the main window in place: the sidebar and the host, then the controls of the sidebar (logo row,
// list, Signed in as, the account card with avatar, name, role and Switch user), then the page that is shown.
static void layoutmain(int width, int height)
{
    int sidewidth = state.sidebar ? px(sidebarwidth) : 0;
    int hostwidth = width - sidewidth;
    guilayout layout;
    layout.place(state.parts.host, sidewidth, 0, hostwidth, height);
    layout.place(state.parts.sidebar, 0, 0, sidewidth, height);
    layout.apply();

    if (state.sidebar)
    {
        guilayout inside;
        int gap = px(gapbox);
        int card = px(gapfield);
        RECT account = {card, height - card - px(116), sidewidth - card, height - card};
        state.accountcard = account;
        int navtop = px(80);
        int navheight = account.top - px(32) - navtop;

        if (navheight < px(40))
        {
            navheight = px(40);
        }

        inside.place(state.parts.appname, gap + px(44), px(24), sidewidth - gap - px(44) - gap, px(32));
        inside.place(state.parts.nav, px(gapsmall), navtop, sidewidth - 2 * px(gapsmall), navheight);
        inside.place(state.whocaption, gap, account.top - px(24), sidewidth - 2 * gap, px(captionline));
        int textleft = account.left + px(12) + px(36) + px(10);
        inside.place(state.whoname, textleft, account.top + px(12), account.right - px(12) - textleft, px(20));
        inside.place(state.whorole, textleft, account.top + px(12) + px(20), account.right - px(12) - textleft, px(16));
        inside.place(state.switchuser, account.left + px(12), account.bottom - px(12) - px(controlheight), account.right - account.left - px(24),
                     px(controlheight));
        inside.apply();
        InvalidateRect(state.parts.sidebar, NULL, FALSE);
    }

    guipage* page = shellpage(state.current);

    if (page != NULL)
    {
        SetWindowPos(page->window(), NULL, 0, 0, hostwidth, height, SWP_NOZORDER | SWP_NOACTIVATE);
        page->onsize(hostwidth, height);
    }
}

// Lays the window out for its present client size.
static void layoutnow()
{
    int width = 0;
    int height = 0;
    shellgetclient(width, height);
    layoutmain(width, height);
}

// The Windows 10 function that adds the frame of a window for a given dpi (loaded by name, the old headers lack it).
typedef BOOL(WINAPI* adjustfordpi)(LPRECT, DWORD, BOOL, DWORD, UINT);

// Grows a client rectangle to the rectangle of the whole window for the window's own dpi.
static void frameforwindow(HWND window, RECT& frame)
{
    DWORD style = (DWORD)GetWindowLongA(window, GWL_STYLE);
    DWORD exstyle = (DWORD)GetWindowLongA(window, GWL_EXSTYLE);
    HMODULE user = GetModuleHandleA("user32.dll");
    adjustfordpi function = user == NULL ? NULL : (adjustfordpi)GetProcAddress(user, "AdjustWindowRectExForDpi");

    if (function != NULL && !themedpiisoverridden())
    {
        function(&frame, style, FALSE, exstyle, (UINT)themedpiforwindow(window));
    }
    else
    {
        AdjustWindowRectEx(&frame, style, FALSE, exstyle);
    }
}

// Takes a new dpi: fonts, list rows, the size of the sidebar rows, the window and the page that is shown follow it.
void shellapplydpi(int dpi, const RECT* suggested)
{
    themesetdpi(dpi);
    themeapplyfonts(state.parts.main);
    themereleasefonts();
    themerestylelists(state.parts.main);

    for (int row = 0; row < (int)state.navglyphs.size(); row++)
    {
        SendMessageA(state.parts.nav, LB_SETITEMHEIGHT, (WPARAM)row, (LPARAM)(state.navglyphs[row] == 0 ? px(32) : px(40)));
    }

    if (suggested != NULL)
    {
        SetWindowPos(state.parts.main, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    layoutnow();
    guipage* page = shellpage(state.current);

    if (page != NULL)
    {
        page->onshow();
        layoutnow();
    }

    RedrawWindow(state.parts.main, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

// Appends one line with the time to the dpi log (only with --dpi-log).
static void dpilogline(const string& text)
{
    if (state.dpilog.empty())
    {
        return;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);
    ofstream out(state.dpilog.c_str(), ios::app);
    out << numtext(now.wHour) << ":" << numtext(now.wMinute) << ":" << numtext(now.wSecond) << " " << text << endl;
}

// Writes a rectangle as left,top,right,bottom.
static string recttext(const RECT& box)
{
    return numtext(box.left) + "," + numtext(box.top) + "," + numtext(box.right) + "," + numtext(box.bottom);
}

// The Windows 10 functions that tell the dpi awareness of a window (loaded by name, the old headers lack them).
typedef HANDLE(WINAPI* contextofwindow)(HWND);
typedef BOOL(WINAPI* contextsequal)(HANDLE, HANDLE);

// Compares the awareness of the main window with the four standard values.
int shellawareness()
{
    HMODULE user = GetModuleHandleA("user32.dll");
    contextofwindow get = user == NULL ? NULL : (contextofwindow)GetProcAddress(user, "GetWindowDpiAwarenessContext");
    contextsequal equal = user == NULL ? NULL : (contextsequal)GetProcAddress(user, "AreDpiAwarenessContextsEqual");

    if (get == NULL || equal == NULL || state.parts.main == NULL)
    {
        return 0;
    }

    HANDLE context = get(state.parts.main);

    for (int value = -4; value <= -1; value++)
    {
        if (equal(context, (HANDLE)(INT_PTR)value))
        {
            return value;
        }
    }

    return 0;
}

// Remembers the log file and writes the line of the start.
void shelldpilog(const string& path)
{
    state.dpilog = path;
    RECT box;
    GetWindowRect(state.parts.main, &box);
    dpilogline("start awareness=" + numtext(shellawareness()) + " dpi=" + numtext(themedpi()) + " window=" + recttext(box));
}

// The line is set by the WM_DPICHANGED case itself.
int shelldpihandlerline()
{
    return state.dpiline;
}

// A test may make the window bigger than the screen.
void shellallowhuge(bool allow)
{
    state.allowhuge = allow;
}

// Shows or hides the sidebar panel (and with it every control in it).
static void showsidebar(bool visible)
{
    state.sidebar = visible;
    ShowWindow(state.parts.sidebar, visible ? SW_SHOWNA : SW_HIDE);
}

// Selects the sidebar row that belongs to a page, if there is one (not a click: the page is not opened again).
static void selectnavfor(int pageid)
{
    for (size_t pos = 0; pos < state.navpages.size(); pos++)
    {
        if (state.navpages[pos] == pageid)
        {
            state.syncing = true;
            SendMessageA(state.parts.nav, LB_SETCURSEL, (WPARAM)state.navrows[pos], 0);
            state.syncing = false;
            return;
        }
    }
}

// Switches pages inside the host. The new page is laid out and filled before it is shown, and the host does not repaint in
// between, so the switch is quick and does not flicker.
void shellshow(int pageid, bool takefocus)
{
    guipage* next = shellpage(pageid);

    if (next == NULL)
    {
        return;
    }

    guipage* old = shellpage(state.current);
    SendMessageA(state.parts.host, WM_SETREDRAW, FALSE, 0);

    if (old != NULL && old != next)
    {
        ShowWindow(old->window(), SW_HIDE);
    }

    if (state.current != pageid)
    {
        state.previous = state.current;
    }

    state.current = pageid;
    shellstatus("");
    selectnavfor(next->navid());
    layoutnow();
    next->onshow();
    ShowWindow(next->window(), SW_SHOWNA);
    SendMessageA(state.parts.host, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(state.parts.host, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);

    // the new page fades in from the page tone (only between two pages, and only when motion is wanted)
    if (old != NULL && old != next)
    {
        promotionfade(state.parts.main, state.parts.host);
    }

    if (takefocus)
    {
        HWND first = GetNextDlgTabItem(next->window(), NULL, FALSE);

        if (first != NULL)
        {
            SetFocus(first);
        }
    }
}

// Back to the page before; without one, the first page of the role.
void shellback()
{
    int target = state.previous;
    const navitem* items = navof(state.who);

    if (target == PAGE_NONE || target == PAGE_WELCOME || shellpage(target) == NULL)
    {
        target = items != NULL ? items[0].page : PAGE_WELCOME;
    }

    shellshow(target, true);
}

// Fills the sidebar list: a section label row before each group, one row per item.
static void fillnav(const navitem* items)
{
    SendMessageA(state.parts.nav, LB_RESETCONTENT, 0, 0);
    state.navpages.clear();
    state.navrows.clear();
    state.navglyphs.clear();
    state.navhot = -1;

    for (int pos = 0; items != NULL && items[pos].text != NULL; pos++)
    {
        if (items[pos].section != NULL)
        {
            int row = (int)SendMessageA(state.parts.nav, LB_ADDSTRING, 0, (LPARAM)items[pos].section);
            SendMessageA(state.parts.nav, LB_SETITEMDATA, (WPARAM)row, (LPARAM)PAGE_NONE);
            SendMessageA(state.parts.nav, LB_SETITEMHEIGHT, (WPARAM)row, (LPARAM)px(32));
            state.navglyphs.push_back(0);
        }

        int row = (int)SendMessageA(state.parts.nav, LB_ADDSTRING, 0, (LPARAM)items[pos].text);
        SendMessageA(state.parts.nav, LB_SETITEMDATA, (WPARAM)row, (LPARAM)items[pos].page);
        SendMessageA(state.parts.nav, LB_SETITEMHEIGHT, (WPARAM)row, (LPARAM)px(40));
        state.navpages.push_back(items[pos].page);
        state.navrows.push_back(row);
        state.navglyphs.push_back(items[pos].glyph);
    }
}

// Fills the sidebar for the role, then opens the first page of that role.
void shellsignin(int who, const string& personid, const string& personname)
{
    const navitem* items = navof(who);
    state.who = who;
    state.personid = personid;
    state.personname = personname;
    fillnav(items);
    string role = "Coordinator";

    if (who == WHO_DONOR)
    {
        role = "Donor";
    }
    else if (who == WHO_RECIPIENT)
    {
        role = "Recipient";
    }

    // the coordinator has no person name: the card shows the role once
    SetWindowTextA(state.whorole, personname.empty() ? "" : role.c_str());
    SetWindowTextA(state.whoname, personname.empty() ? role.c_str() : personname.c_str());
    SetWindowTextA(state.parts.main, "CommunityBridge");
    showsidebar(true);

    if (items != NULL)
    {
        shellshow(items[0].page, false);
    }
}

// Back to the welcome page: nobody is signed in and the sidebar is hidden.
void shellswitchuser()
{
    state.who = WHO_NONE;
    state.personid = "";
    state.personname = "";
    fillnav(NULL);
    SetWindowTextA(state.parts.main, "CommunityBridge");
    showsidebar(false);
    shellshow(PAGE_WELCOME, true);
}

// Gives the window this client size, keeping its top left corner.
void shellsetclient(int width, int height)
{
    RECT frame = {0, 0, width, height};
    frameforwindow(state.parts.main, frame);
    SetWindowPos(state.parts.main, NULL, 0, 0, frame.right - frame.left, frame.bottom - frame.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
}

// The initials of the avatar: the first letters of the first two words of the name (the role for the coordinator).
static string initialsof(const string& name)
{
    string letters = "";
    bool start = true;

    for (size_t pos = 0; pos < name.length() && letters.length() < 2; pos++)
    {
        char c = name[pos];
        bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');

        if (letter && start)
        {
            letters += (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
        }

        start = !letter;
    }

    return letters;
}

// Paints the sidebar panel: its tone, the logo tile with CB, and the account card with the avatar circle and its initials.
static void paintsidebar(HWND window, HDC dc)
{
    RECT area;
    GetClientRect(window, &area);
    FillRect(dc, &area, themesidebarbrush());
    int gap = px(gapbox);
    RECT logo = {gap, px(24), gap + px(32), px(56)};
    proround(dc, logo, px(8), themecolorof(COLOR_ACCENT), CLR_INVALID);
    protext(dc, "CB", logo, themecolorof(COLOR_ONACCENT), themefont(FONT_STRONG), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (state.who != WHO_NONE)
    {
        proround(dc, state.accountcard, px(8), themecolorof(COLOR_CARD), themecolorof(COLOR_BORDER));
        RECT avatar = {state.accountcard.left + px(12), state.accountcard.top + px(12), state.accountcard.left + px(48),
                       state.accountcard.top + px(48)};
        procircle(dc, avatar, themecolorof(COLOR_ACCENTTINT));
        string name = state.personname.empty() ? "Coordinator" : state.personname;
        protext(dc, initialsof(name), avatar, themecolorof(COLOR_ACCENT), themefont(FONT_STRONG), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

// Draws one row of the sidebar list: a section label (12 semibold capitals, muted) or an item with its glyph and text, the
// hover fill, the selected fill with a 3 px accent bar and the focus ring.
static void drawnavrow(const DRAWITEMSTRUCT* item)
{
    if (item->itemID == (UINT)-1)
    {
        return;
    }

    RECT box = item->rcItem;
    HDC dc = item->hDC;
    FillRect(dc, &box, themesidebarbrush());
    char text[128];
    text[0] = '\0';
    SendMessageA(item->hwndItem, LB_GETTEXT, (WPARAM)item->itemID, (LPARAM)text);
    unsigned int glyph = item->itemID < state.navglyphs.size() ? state.navglyphs[item->itemID] : 0;

    if (glyph == 0)
    {
        RECT label = {box.left + px(gapsmall), box.top, box.right, box.bottom - px(gaptiny)};
        protext(dc, text, label, themecolorof(COLOR_MUTED), themefont(FONT_HEADER), DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
        return;
    }

    bool selected = (item->itemState & ODS_SELECTED) != 0;
    RECT fill = box;
    InflateRect(&fill, 0, -px(2));

    if (selected || (int)item->itemID == state.navhot)
    {
        proround(dc, fill, px(4), selected ? themecolorof(COLOR_PRESSED) : themecolorof(COLOR_HOVER), CLR_INVALID);
    }

    if (selected)
    {
        RECT bar = {fill.left, fill.top + px(10), fill.left + px(3), fill.bottom - px(10)};
        proround(dc, bar, px(2), themecolorof(COLOR_ACCENT), CLR_INVALID);
    }

    RECT mark = {box.left + px(12), box.top, box.left + px(32), box.bottom};
    proglyph(dc, glyph, mark, selected ? themecolorof(COLOR_ACCENT) : themecolorof(COLOR_TEXT), 16);
    RECT label = {box.left + (proiconsready() ? px(44) : px(16)), box.top, box.right - px(gapsmall), box.bottom};
    protext(dc, text, label, themecolorof(COLOR_TEXT), selected ? themefont(FONT_STRONG) : themefont(FONT_BODY),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if ((item->itemState & ODS_FOCUS) != 0 && (SendMessageA(item->hwndItem, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) == 0)
    {
        proring(dc, fill, px(4));
    }
}

// Tracks the row under the mouse in the sidebar list (for the hover fill).
static LRESULT CALLBACK navproc(HWND list, UINT msg, WPARAM wparam, LPARAM lparam)
{
    WNDPROC old = (WNDPROC)GetPropA(list, "cboldproc");

    if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE)
    {
        int row = -1;

        if (msg == WM_MOUSEMOVE)
        {
            DWORD hit = (DWORD)SendMessageA(list, LB_ITEMFROMPOINT, 0, lparam);
            row = HIWORD(hit) == 0 ? (int)LOWORD(hit) : -1;
            TRACKMOUSEEVENT track;
            ZeroMemory(&track, sizeof(track));
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = list;
            TrackMouseEvent(&track);
        }

        if (row != state.navhot)
        {
            state.navhot = row;
            InvalidateRect(list, NULL, FALSE);
        }
    }
    else if (msg == WM_NCDESTROY)
    {
        SetWindowLongPtrA(list, GWLP_WNDPROC, (LONG_PTR)old);
        RemovePropA(list, "cboldproc");
    }

    return CallWindowProcA(old, list, msg, wparam, lparam);
}

// A section label cannot be chosen: the selection moves on to the item after it (or before it at the end).
static void navchanged()
{
    int row = (int)SendMessageA(state.parts.nav, LB_GETCURSEL, 0, 0);

    if (row < 0 || state.syncing)
    {
        return;
    }

    int page = (int)SendMessageA(state.parts.nav, LB_GETITEMDATA, (WPARAM)row, 0);

    if (page == PAGE_NONE)
    {
        int count = (int)SendMessageA(state.parts.nav, LB_GETCOUNT, 0, 0);
        int next = row + 1 < count ? row + 1 : row - 1;
        SendMessageA(state.parts.nav, LB_SETCURSEL, (WPARAM)next, 0);
        page = (int)SendMessageA(state.parts.nav, LB_GETITEMDATA, (WPARAM)next, 0);
    }

    if (page != PAGE_NONE && page != state.current)
    {
        shellshow(page, false);
    }
}

// The main window procedure: size, minimum size, dpi, default button, commands and the toast timer.
static LRESULT CALLBACK mainproc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT colour = 0;

    if (themecolour(msg, wparam, lparam, colour))
    {
        return colour;
    }

    if (msg == WM_SIZE)
    {
        if (wparam != SIZE_MINIMIZED && state.parts.host != NULL)
        {
            layoutmain(LOWORD(lparam), HIWORD(lparam));
        }

        if (state.toast != NULL && IsWindowVisible(state.toast))
        {
            shellstatus(wparam == SIZE_MINIMIZED ? "" : state.statustext);
        }

        return 0;
    }

    if (msg == WM_MOVE && state.toast != NULL && IsWindowVisible(state.toast))
    {
        drawtoast(state.statustext);
        return 0;
    }

    if (msg == WM_TIMER && promotiontick(window, wparam))
    {
        return 0;
    }

    if (msg == WM_TIMER && wparam == toasttimer)
    {
        KillTimer(window, toasttimer);
        ShowWindow(state.toast, SW_HIDE);
        return 0;
    }

    if (msg == WM_GETMINMAXINFO)
    {
        MINMAXINFO* info = (MINMAXINFO*)lparam;
        RECT frame = {0, 0, px(900), px(600)};
        frameforwindow(window, frame);
        info->ptMinTrackSize.x = frame.right - frame.left;
        info->ptMinTrackSize.y = frame.bottom - frame.top;

        if (state.allowhuge)
        {
            info->ptMaxTrackSize.x = 16000;
            info->ptMaxTrackSize.y = 16000;
        }

        return 0;
    }

    // the window moved to a monitor with another scale, or the scale changed: the new dpi is in the low word
    if (msg == WM_DPICHANGED)
    {
        state.dpiline = __LINE__;
        dpilogline("dpichanged old=" + numtext(themedpi()) + " new=" + numtext(LOWORD(wparam)) + " suggested=" + recttext(*(const RECT*)lparam));
        shellapplydpi((int)LOWORD(wparam), (const RECT*)lparam);
        return 0;
    }

    if (msg == DM_GETDEFID)
    {
        guipage* page = shellpage(state.current);

        if (page != NULL && page->defaultid() != 0)
        {
            HWND button = GetDlgItem(page->window(), page->defaultid());

            if (button != NULL && IsWindowEnabled(button) && (GetWindowLongA(button, GWL_STYLE) & WS_VISIBLE))
            {
                return MAKELRESULT(page->defaultid(), DC_HASDEFID);
            }
        }

        return 0;
    }

    if (msg == WM_ACTIVATE)
    {
        if (LOWORD(wparam) == WA_INACTIVE)
        {
            state.lastfocus = GetFocus();
        }
        else if (state.lastfocus != NULL && IsWindow(state.lastfocus))
        {
            SetFocus(state.lastfocus);
        }

        return 0;
    }

    if (msg == WM_COMMAND)
    {
        int id = LOWORD(wparam);
        int code = HIWORD(wparam);

        if (id == IDC_SWITCHUSER)
        {
            if (code == BN_CLICKED)
            {
                shellswitchuser();
            }
        }
        else if (id == IDC_NAV)
        {
            if (code == LBN_SELCHANGE)
            {
                navchanged();
            }
        }
        else
        {
            // Enter in a dialog-style window arrives here; the page decides what it means
            guipage* page = shellpage(state.current);

            if (page != NULL)
            {
                page->command(id, code);
            }
        }

        return 0;
    }

    if (msg == WM_DESTROY)
    {
        KillTimer(window, toasttimer);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

// The page host only holds pages; it has no background of its own (the pages paint everything).
static LRESULT CALLBACK hostproc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_ERASEBKGND)
    {
        RECT area;
        GetClientRect(window, &area);
        FillRect((HDC)wparam, &area, themepagebrush());
        return 1;
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

// True when a sidebar control lies on the account card.
static bool onaccount(HWND control)
{
    RECT place;
    GetWindowRect(control, &place);
    MapWindowPoints(NULL, state.parts.sidebar, (POINT*)&place, 2);
    return place.left >= state.accountcard.left && place.top >= state.accountcard.top && place.right <= state.accountcard.right &&
           place.bottom <= state.accountcard.bottom;
}

// The sidebar panel paints its tone, the logo and the account card; the list rows and the Switch user button are drawn here.
static LRESULT CALLBACK sideproc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_CTLCOLORLISTBOX)
    {
        SetBkColor((HDC)wparam, themecolorof(COLOR_SIDEBAR));
        return (LRESULT)themesidebarbrush();
    }

    if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN)
    {
        LRESULT colour = 0;

        if (themecolouron(msg, wparam, lparam, colour, onaccount((HWND)lparam)))
        {
            return colour;
        }
    }

    if (msg == WM_ERASEBKGND)
    {
        return 1;
    }

    if (msg == WM_PAINT)
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT area;
        GetClientRect(window, &area);

        {
            probuffer buffer(dc, area);
            paintsidebar(window, buffer.dc());
        }

        EndPaint(window, &paint);
        return 0;
    }

    if (msg == WM_MEASUREITEM)
    {
        MEASUREITEMSTRUCT* measure = (MEASUREITEMSTRUCT*)lparam;
        measure->itemHeight = (UINT)px(40);
        return TRUE;
    }

    if (msg == WM_DRAWITEM)
    {
        const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lparam;

        if (item->CtlType == ODT_LISTBOX)
        {
            drawnavrow(item);
        }
        else
        {
            prodrawbutton(item, onaccount(item->hwndItem));
        }

        return TRUE;
    }

    if (msg == WM_COMMAND || msg == WM_NOTIFY)
    {
        return SendMessageA(GetParent(window), msg, wparam, lparam);
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

// The toast does not take clicks or the focus.
static LRESULT CALLBACK toastproc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_MOUSEACTIVATE)
    {
        return MA_NOACTIVATE;
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

// Registers the window classes. None of them has CS_HREDRAW or CS_VREDRAW.
static void registerclasses(HINSTANCE instance)
{
    WNDCLASSA description;
    ZeroMemory(&description, sizeof(description));
    description.lpfnWndProc = mainproc;
    description.hInstance = instance;
    description.hCursor = LoadCursor(NULL, IDC_ARROW);
    description.hbrBackground = themepagebrush();
    description.lpszClassName = mainclass;
    RegisterClassA(&description);

    description.lpfnWndProc = hostproc;
    description.hbrBackground = NULL;
    description.lpszClassName = hostclass;
    RegisterClassA(&description);

    description.lpfnWndProc = sideproc;
    description.lpszClassName = sideclass;
    RegisterClassA(&description);

    description.lpfnWndProc = toastproc;
    description.lpszClassName = toastclass;
    RegisterClassA(&description);

    guiregisterpageclass(instance);
}

// Makes the window, its controls and all pages. The window starts at 1000 by 650, centred on the work area.
bool shellcreate(HINSTANCE instance)
{
    state.parts.main = NULL;
    state.parts.host = NULL;
    state.parts.sidebar = NULL;
    state.parts.appname = NULL;
    state.parts.nav = NULL;
    state.parts.status = NULL;
    state.toast = NULL;
    state.current = PAGE_NONE;
    state.previous = PAGE_NONE;
    state.who = WHO_NONE;
    state.sidebar = false;
    state.syncing = false;
    state.lastfocus = NULL;
    state.allowhuge = false;
    state.dpiline = 0;
    state.navhot = -1;
    ZeroMemory(&state.accountcard, sizeof(state.accountcard));

    registerclasses(instance);

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    DWORD exstyle = WS_EX_CONTROLPARENT;
    RECT frame = {0, 0, px(1000), px(650)};
    AdjustWindowRectEx(&frame, style, FALSE, exstyle);
    int width = frame.right - frame.left;
    int height = frame.bottom - frame.top;
    RECT area;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &area, 0);
    width = width > area.right - area.left ? area.right - area.left : width;
    height = height > area.bottom - area.top ? area.bottom - area.top : height;
    int left = area.left + (area.right - area.left - width) / 2;
    int top = area.top + (area.bottom - area.top - height) / 2;
    state.parts.main = CreateWindowExA(exstyle, mainclass, "CommunityBridge", style, left, top, width, height, NULL, NULL, instance, NULL);

    if (state.parts.main == NULL)
    {
        return false;
    }

    HWND main = state.parts.main;
    prodwmround(main);
    prodwmcaption(main, themecolorof(COLOR_PAGE));

    // the icon of the program for the title bar, the taskbar and Alt+Tab, in the sizes Windows asks for
    HICON big = (HICON)LoadImageA(instance, MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON small = (HICON)LoadImageA(instance, MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    SendMessageA(main, WM_SETICON, ICON_BIG, (LPARAM)big);
    SendMessageA(main, WM_SETICON, ICON_SMALL, (LPARAM)small);

    // the sidebar comes first, so Tab goes through it before the page: the list, then Switch user
    state.parts.sidebar = CreateWindowExA(WS_EX_CONTROLPARENT, sideclass, "", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 10, 10,
                                          main, NULL, instance, NULL);
    HWND panel = state.parts.sidebar;
    state.parts.appname = makelabel(panel, "CommunityBridge", ROLE_APPNAME);
    state.parts.nav = CreateWindowExA(0, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS |
                                                            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                      0, 0, 10, 10, panel, (HMENU)(INT_PTR)IDC_NAV, instance, NULL);
    SetPropA(state.parts.nav, "cboldproc", (HANDLE)GetWindowLongPtrA(state.parts.nav, GWLP_WNDPROC));
    SetWindowLongPtrA(state.parts.nav, GWLP_WNDPROC, (LONG_PTR)navproc);
    state.whocaption = makelabel(panel, "Signed in as", ROLE_SIDEMUTED);
    state.whoname = makelabel(panel, "", ROLE_STRONG);
    SetWindowLongA(state.whoname, GWL_STYLE, GetWindowLongA(state.whoname, GWL_STYLE) | SS_ENDELLIPSIS);
    markdata(state.whoname);
    state.whorole = makelabel(panel, "", ROLE_MUTED);
    state.switchuser = makebutton(panel, "Switch user", IDC_SWITCHUSER, false);

    state.parts.host = CreateWindowExA(WS_EX_CONTROLPARENT, hostclass, "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 10,
                                       10, main, NULL, instance, NULL);
    state.toast = CreateWindowExA(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, toastclass, "", WS_POPUP, 0, 0, 10, 10, main, NULL,
                                  instance, NULL);

    buildpages(state.parts.host);
    themeapplyfonts(main);

    // like a dialog: the focus ring stays hidden after mouse clicks and appears once the keyboard is used
    SendMessageA(main, WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
    showsidebar(false);

    // the window may be on a monitor whose scale is not the one the program started with (or a test asked for one)
    int windowdpi = themedpiforwindow(main);

    if (windowdpi != themedpi())
    {
        shellapplydpi(windowdpi, NULL);
    }

    shellshow(PAGE_WELCOME, true);
    layoutnow();
    return true;
}

// The message loop. IsDialogMessage gives the Tab key its usual meaning across all the controls of the window.
int shellrun()
{
    MSG msg;

    while (GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessage(state.parts.main, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    return (int)msg.wParam;
}

// Deletes the pages (their windows are already gone with the main window).
void shelldestroy()
{
    for (size_t pos = 0; pos < state.pages.size(); pos++)
    {
        delete state.pages[pos];
    }

    state.pages.clear();
}
