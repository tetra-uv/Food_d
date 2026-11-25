// GuiPage.cpp (professional program): the base of every page, its painting, and the helpers that make the Fluent controls.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "GuiPage.h"
#include "GuiList.h"
#include "ProDraw.h"
#include "ProMotion.h"
#include "Theme.h"

#include <string.h>

using namespace std;

// The class name of a page window.
static const char* const pageclass = "CommunityBridgeProPage";

// The window properties the helpers use: primary button, role card, hover state, old window procedure, text box.
static const char* const primaryproperty = "cbprimary";
static const char* const cardproperty = "cbrolecard";
static const char* const hotproperty = "cbhot";
static const char* const oldprocproperty = "cboldproc";
static const char* const fieldproperty = "cbfield";

// The old Windows headers do not know these values of the keyboard cue state (WM_QUERYUISTATE).
#ifndef UISF_HIDEFOCUS
#define UISF_HIDEFOCUS 0x1
#endif
#ifndef WM_QUERYUISTATE
#define WM_QUERYUISTATE 0x0129
#endif

// The muted line under the title of each page (no punctuation; listed in the final report).
static string descriptionof(int id)
{
    switch (id)
    {
    case PAGE_C_HOME:
        return "An overview of donations offers and notes about the data";
    case PAGE_C_DONATIONS:
        return "Every donation with its status ready to be offered or closed";
    case PAGE_C_OFFERS:
        return "Offers sent to recipients and their answers";
    case PAGE_C_RECIPIENTS:
        return "Every recipient with its room food types and settings";
    case PAGE_C_TRANSACTIONS:
        return "Food that has reached a recipient";
    case PAGE_C_SUMMARY:
        return "Totals of the food that was offered given out and wasted";
    case PAGE_C_MATCH:
        return "Recipients ranked for this donation and the reasons for the others";
    case PAGE_D_HOME:
        return "An overview of your donations and their offers";
    case PAGE_D_DONATIONS:
        return "Your donations with their status";
    case PAGE_D_OFFERS:
        return "Offers made for your donations";
    case PAGE_D_GIVEN:
        return "Your food that has reached a recipient";
    case PAGE_D_ADD:
        return "Tell us what food you can give";
    case PAGE_D_EDIT:
        return "Change a donation from which nothing was given out yet";
    case PAGE_R_HOME:
        return "An overview of your offers and the food you received";
    case PAGE_R_INCOMING:
        return "Offers that wait for your answer";
    case PAGE_R_PAST:
        return "Offers you answered before";
    case PAGE_R_RECEIVED:
        return "Food that reached you";
    case PAGE_R_EDIT:
        return "Your room the food you take and how you collect it";
    default:
        return "";
    }
}

// The one muted line of a welcome role card.
static string rolelineof(const string& role)
{
    if (role == "Coordinator")
    {
        return "Sees ranked recipients then sends offers and closes donations";
    }

    if (role == "Donor")
    {
        return "Adds donations and follows what happened to them";
    }

    if (role == "Recipient")
    {
        return "Accepts or declines offers and keeps its details current";
    }

    return "";
}

// True when the keyboard cues of the window are hidden (after mouse use): then no focus ring is drawn.
static bool cueshidden(HWND control)
{
    return (SendMessageA(control, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) != 0;
}

// Draws an owner drawn push button: primary (accent), secondary (card with border) or a welcome role card, with the hover,
// pressed, disabled and focus states.
void prodrawbutton(const DRAWITEMSTRUCT* item, bool oncard)
{
    HWND button = item->hwndItem;
    RECT box = item->rcItem;
    HDC dc = item->hDC;
    probuffer buffer(dc, box);
    HDC memory = buffer.dc();
    FillRect(memory, &box, oncard ? themecardbrush() : themepagebrush());

    bool disabled = (item->itemState & ODS_DISABLED) != 0;
    bool pressed = (item->itemState & ODS_SELECTED) != 0;
    bool focus = (item->itemState & ODS_FOCUS) != 0 && !cueshidden(button);
    bool hot = GetPropA(button, hotproperty) != NULL;
    bool primary = GetPropA(button, primaryproperty) != NULL;
    bool rolecard = GetPropA(button, cardproperty) != NULL;
    char text[256];
    text[0] = '\0';
    GetWindowTextA(button, text, (int)sizeof(text));

    if (rolecard)
    {
        // a role card is the role name (18 semibold) and one muted line on a card; hover changes the fill and the border only
        COLORREF fill = pressed ? themecolorof(COLOR_PRESSED) : (hot ? themecolorof(COLOR_HOVER) : themecolorof(COLOR_CARD));
        COLORREF border = hot ? themecolorof(COLOR_MUTED) : themecolorof(COLOR_BORDER);
        proround(memory, box, px(8), fill, border);
        RECT name = {box.left + px(cardpadding), box.top + px(cardpadding), box.right - px(cardpadding), box.top + px(cardpadding + textline + gaptiny)};
        protext(memory, text, name, themecolorof(COLOR_TEXT), themefont(FONT_CARDTITLE), DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        RECT line = {box.left + px(cardpadding), name.bottom + px(gaptiny), box.right - px(cardpadding), box.bottom - px(gapfield)};
        protext(memory, rolelineof(text), line, themecolorof(COLOR_MUTED), themefont(FONT_BODY), DT_LEFT | DT_WORDBREAK);

        if (focus)
        {
            proring(memory, box, px(8));
        }

        return;
    }

    COLORREF fill = themecolorof(COLOR_CARD);
    COLORREF border = themecolorof(COLOR_BORDER);
    COLORREF ink = themecolorof(COLOR_TEXT);

    if (disabled)
    {
        fill = themecolorof(COLOR_DISABLEDFILL);
        border = CLR_INVALID;
        ink = themecolorof(COLOR_DISABLED);
    }
    else if (primary)
    {
        // the hover colour comes in over 120 ms (at once when motion is off)
        COLORREF hover = promix(themecolorof(COLOR_ACCENT), themecolorof(COLOR_ACCENTHOVER), promotionhoverstep(button));
        fill = pressed ? themecolorof(COLOR_ACCENTPRESSED) : (hot ? hover : themecolorof(COLOR_ACCENT));
        border = CLR_INVALID;
        ink = themecolorof(COLOR_ONACCENT);
    }
    else if (pressed || hot)
    {
        COLORREF hover = promix(themecolorof(COLOR_CARD), themecolorof(COLOR_HOVER), promotionhoverstep(button));
        fill = pressed ? themecolorof(COLOR_PRESSED) : hover;
    }

    proround(memory, box, px(4), fill, border);
    protext(memory, text, box, ink, (HFONT)SendMessageA(button, WM_GETFONT, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (focus)
    {
        proring(memory, box, px(4));
    }
}

// Draws a standard check box through its custom draw notification: a rounded 20 px box (filled with the text colour and a check
// mark when ticked), the text after it, hover and focus states. The button keeps its state, keys and accessible name.
static LRESULT drawcheck(NMCUSTOMDRAW* draw, bool oncard)
{
    if (draw->dwDrawStage != CDDS_PREPAINT)
    {
        return CDRF_DODEFAULT;
    }

    HWND button = draw->hdr.hwndFrom;
    RECT area;
    GetClientRect(button, &area);
    HDC dc = draw->hdc;
    FillRect(dc, &area, oncard ? themecardbrush() : themepagebrush());

    bool ticked = SendMessageA(button, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool hot = (draw->uItemState & CDIS_HOT) != 0;
    bool disabled = (draw->uItemState & CDIS_DISABLED) != 0;
    bool focus = (draw->uItemState & CDIS_FOCUS) != 0 && !cueshidden(button);
    int size = px(20);
    RECT box = {area.left + px(2), area.top + (area.bottom - area.top - size) / 2, area.left + px(2) + size, 0};
    box.bottom = box.top + size;

    if (ticked)
    {
        // a ticked box is filled with the text colour, not the accent (at most two accent things per screen)
        COLORREF fill = disabled ? themecolorof(COLOR_DISABLED) : (hot ? promix(themecolorof(COLOR_TEXT), themecolorof(COLOR_MUTED), 40) : themecolorof(COLOR_TEXT));
        proround(dc, box, px(4), fill, CLR_INVALID);

        // the tick is two short lines (no icon font outside the sidebar)
        HPEN pen = CreatePen(PS_SOLID, px(2) < 2 ? 2 : px(2), themecolorof(COLOR_ONACCENT));
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, box.left + size / 4, box.top + size / 2, NULL);
        LineTo(dc, box.left + size * 5 / 12, box.top + size * 2 / 3);
        LineTo(dc, box.left + size * 3 / 4, box.top + size / 3);
        SelectObject(dc, old);
        DeleteObject(pen);
    }
    else
    {
        proround(dc, box, px(4), hot ? themecolorof(COLOR_HOVER) : themecolorof(COLOR_CARD), themecolorof(COLOR_MUTED));
    }

    char text[256];
    text[0] = '\0';
    GetWindowTextA(button, text, (int)sizeof(text));
    RECT label = {box.right + px(10), area.top, area.right, area.bottom};
    protext(dc, text, label, disabled ? themecolorof(COLOR_DISABLED) : themecolorof(COLOR_TEXT), (HFONT)SendMessageA(button, WM_GETFONT, 0, 0),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (focus)
    {
        RECT ring = box;
        InflateRect(&ring, px(3), px(3));
        proring(dc, ring, px(6));
    }

    return CDRF_SKIPDEFAULT;
}

// Passes the messages of the controls on to the page and paints the page.
static LRESULT CALLBACK pageproc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    guipage* page = (guipage*)GetWindowLongPtrA(window, GWLP_USERDATA);

    if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN || msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX)
    {
        LRESULT colour = 0;

        if (themecolouron(msg, wparam, lparam, colour, page != NULL && page->oncard((HWND)lparam)))
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

        if (page != NULL)
        {
            probuffer buffer(dc, area);
            page->paintpage(buffer.dc());
        }

        EndPaint(window, &paint);
        return 0;
    }

    if (msg == WM_COMMAND)
    {
        // a text box got or lost the keyboard focus: its field border changes colour
        if (HIWORD(wparam) == EN_SETFOCUS || HIWORD(wparam) == EN_KILLFOCUS)
        {
            InvalidateRect(window, NULL, FALSE);
        }

        if (page != NULL)
        {
            page->command(LOWORD(wparam), HIWORD(wparam));
        }

        return 0;
    }

    if (msg == WM_MEASUREITEM)
    {
        MEASUREITEMSTRUCT* measure = (MEASUREITEMSTRUCT*)lparam;

        if (measure != NULL && measure->CtlType == ODT_LISTBOX)
        {
            measure->itemHeight = (UINT)themelistboxheight();
            return TRUE;
        }
    }

    if (msg == WM_DRAWITEM)
    {
        const DRAWITEMSTRUCT* item = (const DRAWITEMSTRUCT*)lparam;

        if (item != NULL && item->CtlType == ODT_LISTBOX)
        {
            themedrawlistbox(item);
            return TRUE;
        }

        if (item != NULL && item->CtlType == ODT_BUTTON)
        {
            prodrawbutton(item, page != NULL && page->oncard(item->hwndItem));
            return TRUE;
        }
    }

    if (msg == WM_NOTIFY)
    {
        const NMHDR* header = (const NMHDR*)lparam;

        if (header != NULL && header->code == (UINT)NM_CUSTOMDRAW)
        {
            char name[32];
            name[0] = '\0';
            GetClassNameA(header->hwndFrom, name, (int)sizeof(name));

            if (strcmp(name, "SysHeader32") == 0)
            {
                return themedrawheader((NMCUSTOMDRAW*)lparam);
            }

            LRESULT result = 0;

            if (strcmp(name, "SysListView32") == 0 && guilistcustomdraw(lparam, result))
            {
                return result;
            }

            if (strcmp(name, "Button") == 0)
            {
                return drawcheck((NMCUSTOMDRAW*)lparam, page != NULL && page->oncard(header->hwndFrom));
            }
        }

        if (page != NULL)
        {
            page->notify(header);
        }

        return 0;
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

// The class has no background brush (the page paints everything) and no CS_HREDRAW or CS_VREDRAW.
void guiregisterpageclass(HINSTANCE instance)
{
    WNDCLASSA description;
    ZeroMemory(&description, sizeof(description));
    description.lpfnWndProc = pageproc;
    description.hInstance = instance;
    description.hCursor = LoadCursor(NULL, IDC_ARROW);
    description.hbrBackground = NULL;
    description.lpszClassName = pageclass;
    RegisterClassA(&description);
}

// Nothing is made yet.
guipage::guipage(int id, const string& name, const string& title)
{
    window_ = NULL;
    title_ = NULL;
    desc_ = NULL;
    id_ = id;
    name_ = name;
    title_text_ = title;
}

// The controls go away with the window.
guipage::~guipage()
{
    if (window_ != NULL)
    {
        DestroyWindow(window_);
    }
}

// The page window is hidden until the shell shows it.
void guipage::build(HWND host)
{
    window_ = CreateWindowExA(WS_EX_CONTROLPARENT, pageclass, "", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 10, 10,
                              host, NULL, GetModuleHandleA(NULL), NULL);
    SetWindowLongPtrA(window_, GWLP_USERDATA, (LONG_PTR)this);
    createcontrols();
    themeapplyfonts(window_);
}

// A page that has nothing special to do simply refreshes.
void guipage::onshow()
{
    refresh();
}

// By default a page handles no command.
bool guipage::command(int id, int code)
{
    (void)id;
    (void)code;
    return false;
}

// By default a page handles no notification.
bool guipage::notify(const NMHDR* header)
{
    (void)header;
    return false;
}

// By default a page has no primary button.
int guipage::defaultid() const
{
    return 0;
}

// The calm rule: one list.
int guipage::maxlists() const
{
    return 1;
}

// The calm rule: the primary button and two others.
int guipage::maxbuttons() const
{
    return 3;
}

// By default the page keeps its own sidebar item selected.
int guipage::navid() const
{
    return id_;
}

// By default a page has no list.
int guipage::rows() const
{
    return 0;
}

// The page tone, the cards, the page's own drawing, then the field borders and the banner boxes around the
// controls that need them (found from the controls themselves, so a hidden control draws nothing).
void guipage::paintpage(HDC dc)
{
    RECT area;
    GetClientRect(window_, &area);
    FillRect(dc, &area, themepagebrush());

    for (size_t pos = 0; pos < cards_.size(); pos++)
    {
        proround(dc, cards_[pos], px(8), themecolorof(COLOR_CARD), themecolorof(COLOR_BORDER));
    }

    paint(dc);
    HWND child = GetWindow(window_, GW_CHILD);
    HWND focus = GetFocus();

    while (child != NULL)
    {
        if (IsWindowVisible(child))
        {
            RECT place;
            GetWindowRect(child, &place);
            MapWindowPoints(NULL, window_, (POINT*)&place, 2);

            if (GetPropA(child, fieldproperty) != NULL)
            {
                int grow = (px(controlheight) - (place.bottom - place.top)) / 2;
                RECT box = {place.left - px(fieldinset), place.top - grow, place.right + px(fieldinset), place.bottom + grow};
                proround(dc, box, px(4), themecolorof(COLOR_CARD), themecolorof(COLOR_BORDER));

                if (child == focus)
                {
                    proline(dc, box, px(4), themecolorof(COLOR_TEXT), px(2) < 2 ? 2 : px(2));
                }
            }

            if (themegetrole(child) == ROLE_BANNER && GetWindowTextLengthA(child) > 0)
            {
                // a banner is the warning tint with a thin border and the text (no glyph, no coloured bar)
                RECT box = {place.left - px(gapfield), place.top - px(bannerextra) / 2, place.right + px(gapfield), place.bottom + px(bannerextra) / 2};
                proround(dc, box, px(4), themecolorof(COLOR_WARNINGFILL), promix(themecolorof(COLOR_WARNINGFILL), themecolorof(COLOR_WARNING), 40));
            }
        }

        child = GetWindow(child, GW_HWNDNEXT);
    }
}

// A control is on a card when its rectangle lies inside one.
bool guipage::oncard(HWND control) const
{
    RECT place;
    GetWindowRect(control, &place);
    MapWindowPoints(NULL, window_, (POINT*)&place, 2);

    for (size_t pos = 0; pos < cards_.size(); pos++)
    {
        const RECT& card = cards_[pos];

        if (place.left >= card.left && place.top >= card.top && place.right <= card.right && place.bottom <= card.bottom)
        {
            return true;
        }
    }

    return false;
}

// By default a page draws nothing of its own.
void guipage::paint(HDC dc)
{
    (void)dc;
}

// The window of the page.
HWND guipage::window() const
{
    return window_;
}

// The number of the page.
int guipage::id() const
{
    return id_;
}

// The short name of the page.
const string& guipage::name() const
{
    return name_;
}

// The title text.
const string& guipage::title() const
{
    return title_text_;
}

// The title (28 semibold) and the muted description line of the page.
HWND guipage::addtitle()
{
    title_ = makelabel(window_, title_text_, ROLE_TITLE);
    desc_ = makelabel(window_, descriptionof(id_), ROLE_DESC);
    return title_;
}

// The title at the top margin (40 high), the line under it, then a gap before the content.
int guipage::layouttitle(guilayout& layout, int left, int width)
{
    // the 28 px title needs a little more than its 36 px line (its font has some leading), so its row is 40 high
    int top = px(gaptop) - px(gapsmall);
    layout.place(title_, left, top, width, px(cardheader));
    top = top + px(cardheader);

    if (desc_ != NULL)
    {
        layout.place(desc_, left, top, width, px(textline));
        top = top + px(textline);
    }

    return top + px(pagemargin) - px(gapsmall);
}

// The shapes of the last layout are forgotten.
void guipage::clearshapes()
{
    cards_.clear();
}

// One card.
void guipage::addcard(int left, int top, int width, int height)
{
    RECT card = {left, top, left + width, top + height};
    cards_.push_back(card);
}

// A left aligned (or centred) static. Texts that are too long for the control wrap to the next line.
HWND makelabel(HWND parent, const string& text, int role, int id, bool centred)
{
    // SS_NOPREFIX: an ampersand in a name is shown as it is and never makes a keyboard shortcut
    DWORD style = WS_CHILD | WS_VISIBLE | SS_NOPREFIX | (centred ? SS_CENTER : SS_LEFT);
    HWND control = CreateWindowExA(0, "STATIC", text.c_str(), style, 0, 0, 10, 10, parent, (HMENU)(INT_PTR)id,
                                   GetModuleHandleA(NULL), NULL);
    themesetrole(control, role);
    return control;
}

// Tracks the mouse over an owner drawn button, so it can draw its hover state.
static LRESULT CALLBACK hoverproc(HWND button, UINT msg, WPARAM wparam, LPARAM lparam)
{
    WNDPROC old = (WNDPROC)GetPropA(button, oldprocproperty);

    if (msg == WM_MOUSEMOVE && GetPropA(button, hotproperty) == NULL)
    {
        SetPropA(button, hotproperty, (HANDLE)1);
        TRACKMOUSEEVENT track;
        ZeroMemory(&track, sizeof(track));
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = button;
        TrackMouseEvent(&track);
        promotionhover(button);
        InvalidateRect(button, NULL, FALSE);
    }
    else if (msg == WM_MOUSELEAVE)
    {
        RemovePropA(button, hotproperty);
        InvalidateRect(button, NULL, FALSE);
    }
    else if (msg == WM_NCDESTROY)
    {
        RemovePropA(button, hotproperty);
        RemovePropA(button, oldprocproperty);
        SetWindowLongPtrA(button, GWLP_WNDPROC, (LONG_PTR)old);
    }

    return CallWindowProcA(old, button, msg, wparam, lparam);
}

// Owner drawn buttons get the hover tracking.
static void trackhover(HWND button)
{
    SetPropA(button, oldprocproperty, (HANDLE)GetWindowLongPtrA(button, GWLP_WNDPROC));
    SetWindowLongPtrA(button, GWLP_WNDPROC, (LONG_PTR)hoverproc);
}

// An owner drawn push button that can be reached with Tab.
HWND makebutton(HWND parent, const string& text, int id, bool primary)
{
    HWND button = CreateWindowExA(0, "BUTTON", text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10, parent,
                                  (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);

    if (primary)
    {
        SetPropA(button, primaryproperty, (HANDLE)1);
    }

    trackhover(button);
    return button;
}

// A role card is an owner drawn button marked as a card (the role name and one muted line, no icon).
HWND makerolecard(HWND parent, const string& text, int id)
{
    HWND button = makebutton(parent, text, id, false);
    SetPropA(button, cardproperty, (HANDLE)1);
    return button;
}

// A one-line text box without its own edge (the page draws the field border).
HWND makeedit(HWND parent, int id, bool numberonly, int maxchars)
{
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | (numberonly ? ES_NUMBER : 0);
    HWND control = CreateWindowExA(0, "EDIT", "", style, 0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
    SendMessageA(control, EM_LIMITTEXT, (WPARAM)maxchars, 0);
    SetPropA(control, fieldproperty, (HANDLE)1);
    return control;
}

// A drop-down list. The window is made tall, because the height of a combo box includes its list.
HWND makecombo(HWND parent, int id)
{
    return CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 10, px(200),
                           parent, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
}

// An automatic check box (standard, drawn by the page through custom draw).
HWND makecheck(HWND parent, const string& text, int id)
{
    return CreateWindowExA(0, "BUTTON", text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 10, 10, parent,
                           (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
}

// A window property marks a text as data, so no subclassing is needed.
void markdata(HWND control)
{
    SetPropA(control, "cbdata", (HANDLE)1);
}

// A control without the property shows program text.
bool isdata(HWND control)
{
    return GetPropA(control, "cbdata") != NULL;
}

// Measures the text with the font of the control, wrapped at the width.
int measureheight(HWND control, int width)
{
    char text[1024];
    text[0] = '\0';
    GetWindowTextA(control, text, (int)sizeof(text));

    HDC dc = GetDC(control);
    HGDIOBJ old = SelectObject(dc, (HFONT)SendMessageA(control, WM_GETFONT, 0, 0));
    RECT area = {0, 0, width > 0 ? width : 1, 0};
    DrawTextA(dc, text, -1, &area, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    ReleaseDC(control, dc);
    return area.bottom - area.top;
}

// Measures the text with the font of the control.
int textwidth(HWND control)
{
    char text[1024];
    text[0] = '\0';
    GetWindowTextA(control, text, (int)sizeof(text));

    HDC dc = GetDC(control);
    HGDIOBJ old = SelectObject(dc, (HFONT)SendMessageA(control, WM_GETFONT, 0, 0));
    SIZE size;
    GetTextExtentPoint32A(dc, text, (int)lstrlenA(text), &size);
    SelectObject(dc, old);
    ReleaseDC(control, dc);
    return size.cx;
}

// The caption with room at both sides.
int buttonwidth(HWND button)
{
    int width = textwidth(button) + px(40);
    return width < px(120) ? px(120) : width;
}

// Equal margins on both sides.
void contentbox(int pagewidth, int& left, int& width)
{
    left = px(pagemargin);
    width = pagewidth - 2 * left;

    if (width < 0)
    {
        width = 0;
    }
}

// Goes from the right edge to the left, so the last button made (the primary one) ends exactly at right.
int placebuttons(guilayout& layout, const vector<HWND>& buttons, int right, int bottom)
{
    int height = px(controlheight);
    int edge = right;

    for (int pos = (int)buttons.size() - 1; pos >= 0; pos--)
    {
        int wide = buttonwidth(buttons[pos]);
        edge = edge - wide;
        layout.place(buttons[pos], edge, bottom - height, wide, height);
        edge = edge - px(gapsmall);
    }

    return bottom - height;
}

// Uses the content box or a centred column.
void formcolumn(int pagewidth, int& left, int& width)
{
    if (formfullwidth)
    {
        contentbox(pagewidth, left, width);
        return;
    }

    centredcolumn(pagewidth, px(formwidth), px(pagemargin), left, width);
}

// The label a tiny gap above the field; a text box sits inside the field box (the page draws its border), a drop-down list
// fills it; the helper line follows under the field; the next field starts after a field gap.
int placefield(guilayout& layout, HWND label, HWND field, HWND help, int left, int top, int width, int fieldwidth,
               int fieldheight)
{
    layout.place(label, left, top, width, px(textline));
    top = top + px(textline + gaptiny);

    if (GetPropA(field, fieldproperty) != NULL)
    {
        int inner = px(textline) - px(gaptiny);
        layout.place(field, left + px(fieldinset), top + (px(controlheight) - inner) / 2, fieldwidth - 2 * px(fieldinset), inner);
    }
    else
    {
        layout.place(field, left, top, fieldwidth, fieldheight);
    }

    top = top + px(controlheight);

    if (help != NULL)
    {
        layout.place(help, left, top + px(gaptiny), width, px(captionline));
        top = top + px(gaptiny + captionline);
    }

    return top + px(gapbox + gaptiny);
}

// The text sits 36 pixels in from the left of its box (glyph and bar) and 10 from the top; the box is 20 higher than the text.
int placebanner(guilayout& layout, HWND banner, int left, int top, int width)
{
    int inner = width - px(bannerinset);
    int height = measureheight(banner, inner) + px(gaptiny);
    layout.place(banner, left + px(bannerinset - gapfield), top + px(bannerextra) / 2, inner, height);
    return height + px(bannerextra);
}

// The text of a control with the spaces at both ends cut off.
string readtext(HWND control)
{
    char buffer[1024];
    buffer[0] = '\0';
    GetWindowTextA(control, buffer, (int)sizeof(buffer));

    string text = buffer;
    size_t first = 0;

    while (first < text.length() && (text[first] == ' ' || text[first] == '\t'))
    {
        first++;
    }

    size_t last = text.length();

    while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t'))
    {
        last--;
    }

    return text.substr(first, last - first);
}

// Accepts one to nine digits, so the number always fits in an int.
bool readnumber(HWND control, int& value)
{
    string text = readtext(control);

    if (text.empty() || text.length() > 9)
    {
        return false;
    }

    int number = 0;

    for (size_t pos = 0; pos < text.length(); pos++)
    {
        if (text[pos] < '0' || text[pos] > '9')
        {
            return false;
        }

        number = number * 10 + (text[pos] - '0');
    }

    value = number;
    return true;
}
