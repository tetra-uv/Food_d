// GuiMessage.cpp (professional program): ProDialog, the small rounded modal window that replaces the message boxes. It shows a
// title line, the text and its buttons (no glyph; a warning has the warning tint behind them) (the primary one on the right, the default for Enter; Escape answers No).
// A test mode keeps logging the text and answering yes, exactly as before.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "GuiMessage.h"
#include "GuiPage.h"
#include "GuiSelfTest.h"
#include "ProDraw.h"
#include "Theme.h"

using namespace std;

// The class name of the dialog window.
static const char* const dialogclass = "CommunityBridgeProDialog";

// What the dialog that is open shows and answers.
struct dialogstate
{
    string title;        // the title line
    string text;         // the message
    bool warning;        // a warning (tinted) instead of a question
    bool done;           // true once a button, Enter or Escape answered
    bool yes;            // the answer
    HWND primary;        // the primary button (Yes or OK)
};

static dialogstate box;

// The height of the footer band that holds the buttons.
static int footerheight()
{
    return px(72);
}

// Paints the dialog: the card surface (the warning tint for a warning), the title, the text and the footer band in the page tone.
static void paintdialog(HWND window, HDC dc)
{
    RECT area;
    GetClientRect(window, &area);
    if (box.warning)
    {
        HBRUSH tint = CreateSolidBrush(themecolorof(COLOR_WARNINGFILL));
        FillRect(dc, &area, tint);
        DeleteObject(tint);
    }
    else
    {
        FillRect(dc, &area, themecardbrush());
    }

    RECT footer = area;
    footer.top = area.bottom - footerheight();
    FillRect(dc, &footer, themepagebrush());
    RECT line = footer;
    line.bottom = line.top + 1;
    HBRUSH border = CreateSolidBrush(themecolorof(COLOR_BORDER));
    FillRect(dc, &line, border);
    DeleteObject(border);
    int pad = px(24);
    int textleft = pad;
    RECT title = {textleft, pad, area.right - pad, pad + px(28)};
    protext(dc, box.title, title, themecolorof(COLOR_TEXT), themefont(FONT_SUBTITLE), DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    RECT text = {textleft, title.bottom + px(gapfield), area.right - pad, footer.top - pad};
    protext(dc, box.text, text, themecolorof(box.warning ? COLOR_WARNING : COLOR_TEXT), themefont(FONT_BODY), DT_LEFT | DT_WORDBREAK);
}

// The dialog procedure: painting, the owner drawn buttons and the answers.
static LRESULT CALLBACK dialogproc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
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
            paintdialog(window, buffer.dc());
        }

        EndPaint(window, &paint);
        return 0;
    }

    if (msg == WM_DRAWITEM)
    {
        prodrawbutton((const DRAWITEMSTRUCT*)lparam, false);
        return TRUE;
    }

    if (msg == DM_GETDEFID)
    {
        return MAKELRESULT(IDOK, DC_HASDEFID);
    }

    if (msg == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL))
    {
        box.yes = LOWORD(wparam) == IDOK;
        box.done = true;
        return 0;
    }

    if (msg == WM_CLOSE)
    {
        box.yes = false;
        box.done = true;
        return 0;
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

// Registers the class once (with the system drop shadow).
static void registerdialog()
{
    static bool done = false;

    if (done)
    {
        return;
    }

    WNDCLASSA description;
    ZeroMemory(&description, sizeof(description));
    description.style = CS_DROPSHADOW;
    description.lpfnWndProc = dialogproc;
    description.hInstance = GetModuleHandleA(NULL);
    description.hCursor = LoadCursor(NULL, IDC_ARROW);
    description.lpszClassName = dialogclass;
    RegisterClassA(&description);
    done = true;
}

// Shows the dialog modally over the main window and returns the answer (true for Yes or OK).
static bool runbox(HWND parent, const string& title, const string& text, bool warning, bool question)
{
    registerdialog();
    HWND owner = parent != NULL ? GetAncestor(parent, GA_ROOT) : NULL;
    box.title = title;
    box.text = text;
    box.warning = warning;
    box.done = false;
    box.yes = false;

    // the height follows the wrapped text
    int width = px(440);
    HDC screen = GetDC(NULL);
    HGDIOBJ old = SelectObject(screen, themefont(FONT_BODY));
    RECT measure = {0, 0, width - px(24) - px(64), 0};
    DrawTextA(screen, text.c_str(), -1, &measure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(screen, old);
    ReleaseDC(NULL, screen);
    int height = px(24) + px(28) + px(gapfield) + (measure.bottom - measure.top) + px(24) + footerheight();

    RECT frame = {0, 0, width, height};
    AdjustWindowRectEx(&frame, WS_POPUP | WS_BORDER, FALSE, WS_EX_DLGMODALFRAME);
    int outerwidth = frame.right - frame.left;
    int outerheight = frame.bottom - frame.top;
    RECT place;

    if (owner != NULL)
    {
        GetWindowRect(owner, &place);
    }
    else
    {
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &place, 0);
    }

    int left = place.left + (place.right - place.left - outerwidth) / 2;
    int top = place.top + (place.bottom - place.top - outerheight) / 2;
    HWND dialog = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, dialogclass, "CommunityBridge", WS_POPUP | WS_BORDER, left, top,
                                  outerwidth, outerheight, owner, NULL, GetModuleHandleA(NULL), NULL);
    prodwmround(dialog);

    // the buttons sit in the footer band: the primary one on the right
    int buttontop = height - footerheight() + (footerheight() - px(controlheight)) / 2;
    int right = width - px(24);
    box.primary = makebutton(dialog, question ? "Yes" : "OK", IDOK, true);
    int primarywidth = buttonwidth(box.primary);

    if (question)
    {
        HWND other = makebutton(dialog, "No", IDCANCEL, false);
        SendMessageA(other, WM_SETFONT, (WPARAM)themefont(FONT_BODY), FALSE);
        int otherwidth = buttonwidth(other);
        SetWindowPos(other, NULL, right - primarywidth - px(gapsmall) - otherwidth, buttontop, otherwidth, px(controlheight), SWP_NOZORDER);
    }

    SendMessageA(box.primary, WM_SETFONT, (WPARAM)themefont(FONT_BODY), FALSE);
    primarywidth = buttonwidth(box.primary);
    SetWindowPos(box.primary, NULL, right - primarywidth, buttontop, primarywidth, px(controlheight), SWP_NOZORDER);

    if (owner != NULL)
    {
        EnableWindow(owner, FALSE);
    }

    ShowWindow(dialog, SW_SHOW);
    SetFocus(box.primary);
    MSG message;

    while (!box.done && GetMessageA(&message, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(dialog, &message))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }

    if (owner != NULL)
    {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }

    DestroyWindow(dialog);
    return box.yes;
}

// A test mode only writes the text to the log.
void guishowerror(HWND parent, const string& text)
{
    if (testmode())
    {
        testlog("message=" + text);
        return;
    }

    runbox(parent, "Something needs your attention", text, true, false);
}

// A test mode logs the question and answers Yes.
bool guiaskyes(HWND parent, const string& text)
{
    if (testmode())
    {
        testlog("question=" + text);
        return true;
    }

    return runbox(parent, "Please confirm", text, false, true);
}
