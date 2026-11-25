// ProMotion.cpp: the page fade and the hover change, on one timer that only runs while needed.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "ProMotion.h"
#include "GuiLayout.h"
#include "GuiSelfTest.h"
#include "Theme.h"

#include <stdlib.h>
#include <string>
#include <vector>

using namespace std;

// SystemParametersInfo action that tells whether client area animations are on (Microsoft Learn, RESEARCH.md row 7: 0x1042, the
// BOOL pointed to by pvParam receives TRUE when they are on). The toolchain headers do not define it.
static const UINT clientareaanimation = 0x1042;

// The durations in milliseconds.
static const DWORD fadetime = 120;
static const DWORD hovertime = 120;

// One moving thing: the window and its start time.
struct motionitem
{
    HWND window;
    DWORD start;
};

static vector<motionitem> hovers;   // buttons whose hover colour changes
static HWND fade = NULL;            // the fade overlay while it runs
static DWORD fadestart = 0;         // the start of the fade

// The class name of the fade overlay.
static const char* const fadeclass = "CommunityBridgeProFade";

// The system setting, read each time (the person may change it while the program runs); no motion in a test mode.
bool promotionon()
{
    if (testmode())
    {
        return false;
    }

    BOOL enabled = TRUE;

    if (!SystemParametersInfoA(clientareaanimation, 0, &enabled, 0))
    {
        return true;
    }

    return enabled != FALSE;
}

// Starts the timer once something moves.
static void starttimer(HWND main)
{
    if (main != NULL)
    {
        SetTimer(main, motiontimer, 15, NULL);
    }
}

// The fade overlay is a layered popup in the page tone that clicks pass through; it starts opaque and becomes transparent.
void promotionfade(HWND main, HWND host)
{
    if (!promotionon() || !IsWindowVisible(main) || IsIconic(main))
    {
        return;
    }

    static bool registered = false;

    if (!registered)
    {
        WNDCLASSA description;
        ZeroMemory(&description, sizeof(description));
        description.lpfnWndProc = DefWindowProcA;
        description.hInstance = GetModuleHandleA(NULL);
        description.hbrBackground = themepagebrush();
        description.lpszClassName = fadeclass;
        RegisterClassA(&description);
        registered = true;
    }

    RECT place;
    GetWindowRect(host, &place);

    if (fade == NULL)
    {
        fade = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, fadeclass, "", WS_POPUP, place.left,
                               place.top, place.right - place.left, place.bottom - place.top, main, NULL, GetModuleHandleA(NULL), NULL);
    }
    else
    {
        SetWindowPos(fade, NULL, place.left, place.top, place.right - place.left, place.bottom - place.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    SetLayeredWindowAttributes(fade, 0, 255, LWA_ALPHA);
    ShowWindow(fade, SW_SHOWNOACTIVATE);
    fadestart = GetTickCount();
    starttimer(main);
}

// A button that the mouse entered starts its hover change.
void promotionhover(HWND button)
{
    if (!promotionon())
    {
        return;
    }

    motionitem item;
    item.window = button;
    item.start = GetTickCount();
    hovers.push_back(item);
    starttimer(GetAncestor(button, GA_ROOT));
}

// 0 at the start, 100 at the end or when the button is not changing.
int promotionhoverstep(HWND button)
{
    for (size_t pos = 0; pos < hovers.size(); pos++)
    {
        if (hovers[pos].window == button)
        {
            DWORD passed = GetTickCount() - hovers[pos].start;
            return passed >= hovertime ? 100 : (int)(passed * 100 / hovertime);
        }
    }

    return 100;
}

// One step of everything that moves; the timer stops when nothing moves any more.
bool promotiontick(HWND main, UINT_PTR timer)
{
    if (timer != motiontimer)
    {
        return false;
    }

    DWORD now = GetTickCount();

    for (size_t pos = 0; pos < hovers.size();)
    {
        InvalidateRect(hovers[pos].window, NULL, FALSE);

        if (now - hovers[pos].start >= hovertime)
        {
            hovers.erase(hovers.begin() + pos);
        }
        else
        {
            pos++;
        }
    }

    if (fade != NULL)
    {
        DWORD passed = now - fadestart;

        if (passed >= fadetime)
        {
            DestroyWindow(fade);
            fade = NULL;
        }
        else
        {
            SetLayeredWindowAttributes(fade, 0, (BYTE)(255 - passed * 255 / fadetime), LWA_ALPHA);
        }
    }

    if (hovers.empty() && fade == NULL)
    {
        KillTimer(main, motiontimer);
    }

    return true;
}
