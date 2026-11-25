// ProDraw.cpp: GDI+ shapes and GDI ClearType text for the professional program.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <gdiplus/gdiplus.h>

#include "ProDraw.h"
#include "GuiLayout.h"
#include "Theme.h"

#include <vector>

using namespace std;
using namespace Gdiplus;

static ULONG_PTR gdiplustoken = 0;          // the GDI+ session
static int iconstate = 0;                   // 0 not probed yet, 1 an icon font exists, 2 none
static char iconface[LF_FACESIZE] = "";     // the icon font family that was found

// One icon font of one pixel height (made when first needed).
struct iconfont
{
    int height;
    HFONT font;
};

static vector<iconfont> iconfonts;

// Starts GDI+.
void prodrawstart()
{
    GdiplusStartupInput input;
    GdiplusStartup(&gdiplustoken, &input, NULL);
}

// Stops GDI+ and frees the icon fonts.
void prodrawstop()
{
    for (size_t pos = 0; pos < iconfonts.size(); pos++)
    {
        DeleteObject(iconfonts[pos].font);
    }

    iconfonts.clear();
    GdiplusShutdown(gdiplustoken);
}

// A GDI+ colour from a COLORREF.
static Color colourof(COLORREF value)
{
    return Color(255, GetRValue(value), GetGValue(value), GetBValue(value));
}

// A rounded rectangle path (a plain rectangle when the radius is 0).
static void roundedpath(GraphicsPath& path, float x, float y, float w, float h, float r)
{
    if (r <= 0.5f)
    {
        path.AddRectangle(RectF(x, y, w, h));
        return;
    }

    float d = r * 2.0f;

    if (d > w)
    {
        d = w;
    }

    if (d > h)
    {
        d = h;
    }

    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}

// The fill first, then the border on the pixel grid (half a pixel inside the box).
void proround(HDC dc, const RECT& box, int radius, COLORREF fill, COLORREF border)
{
    if (box.right <= box.left || box.bottom <= box.top)
    {
        return;
    }

    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);

    if (fill != CLR_INVALID)
    {
        GraphicsPath path;
        roundedpath(path, (float)box.left, (float)box.top, (float)(box.right - box.left), (float)(box.bottom - box.top), (float)radius);
        SolidBrush brush(colourof(fill));
        graphics.FillPath(&brush, &path);
    }

    if (border != CLR_INVALID)
    {
        GraphicsPath path;
        roundedpath(path, box.left + 0.5f, box.top + 0.5f, (float)(box.right - box.left) - 1.0f, (float)(box.bottom - box.top) - 1.0f,
                    (float)radius);
        Pen pen(colourof(border), 1.0f);
        graphics.DrawPath(&pen, &path);
    }
}

// An outline of a given width, kept inside the box.
void proline(HDC dc, const RECT& box, int radius, COLORREF colour, int width)
{
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
    float half = width / 2.0f;
    GraphicsPath path;
    roundedpath(path, box.left + half, box.top + half, (float)(box.right - box.left) - width, (float)(box.bottom - box.top) - width,
                (float)radius);
    Pen pen(colourof(colour), (float)width);
    graphics.DrawPath(&pen, &path);
}

// The 2 px ring in the text colour, and a 1 px line in the card colour just inside it.
void proring(HDC dc, const RECT& box, int radius)
{
    int ring = px(2) < 2 ? 2 : px(2);
    proline(dc, box, radius, themecolorof(COLOR_TEXT), ring);
    RECT inner = box;
    InflateRect(&inner, -ring, -ring);
    proline(dc, inner, radius > ring ? radius - ring : 0, themecolorof(COLOR_CARD), 1);
}

// An anti-aliased filled ellipse.
void procircle(HDC dc, const RECT& box, COLORREF fill)
{
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
    SolidBrush brush(colourof(fill));
    graphics.FillEllipse(&brush, (REAL)box.left, (REAL)box.top, (REAL)(box.right - box.left), (REAL)(box.bottom - box.top));
}

// The answer of the font probe: the family exists.
static int CALLBACK familyfound(const LOGFONTA* font, const TEXTMETRICA* metric, DWORD kind, LPARAM flag)
{
    (void)font;
    (void)metric;
    (void)kind;
    *(bool*)flag = true;
    return 0;
}

// True when a font family is installed.
static bool familyexists(const char* family)
{
    LOGFONTA want;
    ZeroMemory(&want, sizeof(want));
    want.lfCharSet = DEFAULT_CHARSET;
    lstrcpynA(want.lfFaceName, family, LF_FACESIZE);
    bool exists = false;
    HDC screen = GetDC(NULL);
    EnumFontFamiliesExA(screen, &want, (FONTENUMPROCA)familyfound, (LPARAM)&exists, 0);
    ReleaseDC(NULL, screen);
    return exists;
}

// Segoe Fluent Icons first, then Segoe MDL2 Assets, then none. A test may force the fallback with CB_PRO_NOICONS=1.
bool proiconsready()
{
    if (iconstate == 0)
    {
        char forced[8];
        forced[0] = '\0';
        GetEnvironmentVariableA("CB_PRO_NOICONS", forced, (DWORD)sizeof(forced));

        if (forced[0] == '1')
        {
            iconstate = 2;
        }
        else if (familyexists("Segoe Fluent Icons"))
        {
            lstrcpynA(iconface, "Segoe Fluent Icons", LF_FACESIZE);
            iconstate = 1;
        }
        else if (familyexists("Segoe MDL2 Assets"))
        {
            lstrcpynA(iconface, "Segoe MDL2 Assets", LF_FACESIZE);
            iconstate = 1;
        }
        else
        {
            iconstate = 2;
        }
    }

    return iconstate == 1;
}

// The icon font of a pixel height (one font per height, kept for the life of the program).
static HFONT iconfontof(int height)
{
    for (size_t pos = 0; pos < iconfonts.size(); pos++)
    {
        if (iconfonts[pos].height == height)
        {
            return iconfonts[pos].font;
        }
    }

    iconfont made;
    made.height = height;
    made.font = CreateFontA(-height, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, iconface);
    iconfonts.push_back(made);
    return made.font;
}

// The glyph is one wide character drawn with DrawTextW (the program itself uses the ANSI functions).
void proglyph(HDC dc, unsigned int code, const RECT& box, COLORREF colour, int design)
{
    if (!proiconsready())
    {
        return;
    }

    wchar_t glyph[2] = {(wchar_t)code, 0};
    RECT area = box;
    HGDIOBJ old = SelectObject(dc, iconfontof(px(design)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colour);
    DrawTextW(dc, glyph, 1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
}

// The text and tint pair of a chip kind.
static void chipcolours(int kind, COLORREF& text, COLORREF& fill)
{
    if (kind == CHIP_SUCCESS)
    {
        text = themecolorof(COLOR_SUCCESS);
        fill = themecolorof(COLOR_SUCCESSFILL);
    }
    else if (kind == CHIP_WARNING)
    {
        text = themecolorof(COLOR_WARNING);
        fill = themecolorof(COLOR_WARNINGFILL);
    }
    else if (kind == CHIP_ERROR)
    {
        text = themecolorof(COLOR_ERROR);
        fill = themecolorof(COLOR_ERRORFILL);
    }
    else
    {
        text = themecolorof(COLOR_MUTED);
        fill = themecolorof(COLOR_NEUTRALFILL);
    }
}

// The chip is as wide as its word plus the dot and some room, never wider than the cell (the word then ends with dots).
int prochip(HDC dc, const RECT& cell, const string& word, int kind, HFONT font)
{
    if (word.empty())
    {
        return cell.left;
    }

    COLORREF text = 0;
    COLORREF fill = 0;
    chipcolours(kind, text, fill);
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size;
    GetTextExtentPoint32A(dc, word.c_str(), (int)word.length(), &size);
    SelectObject(dc, old);

    int height = px(24);
    int top = cell.top + ((cell.bottom - cell.top) - height) / 2;
    int width = px(10) + size.cx + px(10);
    RECT chip = {cell.left, top, cell.left + width, top + height};

    if (chip.right > cell.right)
    {
        chip.right = cell.right;
    }

    proround(dc, chip, px(4), fill, CLR_INVALID);
    RECT label = {chip.left + px(10), chip.top, chip.right - px(6), chip.bottom};
    protext(dc, word, label, text, font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    return chip.right;
}

// Words that mean done are green, words that mean open are amber, flags are red, the rest is neutral.
int prochipkind(const string& word, bool checkcolumn)
{
    if (checkcolumn)
    {
        return CHIP_ERROR;
    }

    if (word == "All given" || word == "Accepted")
    {
        return CHIP_SUCCESS;
    }

    if (word == "Waiting" || word == "Partly given")
    {
        return CHIP_WARNING;
    }

    return CHIP_NEUTRAL;
}

// Transparent text in one colour.
void protext(HDC dc, const string& text, RECT box, COLORREF colour, HFONT font, UINT format)
{
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colour);
    DrawTextA(dc, text.c_str(), -1, &box, format | DT_NOPREFIX);
    SelectObject(dc, old);
}

// DWM window attributes, values checked on Microsoft Learn (RESEARCH.md rows 5, 6 and 11): DWMWA_WINDOW_CORNER_PREFERENCE 33
// with DWMWCP_ROUND 2, DWMWA_CAPTION_COLOR 35 (Windows 11 build 22000). The toolchain has no dwmapi, so the function is loaded.
static const DWORD dwmcorner = 33;
static const DWORD dwmround = 2;
static const DWORD dwmcaption = 35;

// DwmSetWindowAttribute, loaded by name.
typedef HRESULT(WINAPI* dwmsetattribute)(HWND, DWORD, LPCVOID, DWORD);

// Calls DwmSetWindowAttribute when dwmapi.dll has it; every failure is ignored (Windows 10 knows neither attribute).
static void dwmset(HWND window, DWORD attribute, const void* value, DWORD size)
{
    static HMODULE library = LoadLibraryA("dwmapi.dll");

    if (library == NULL)
    {
        return;
    }

    dwmsetattribute function = (dwmsetattribute)GetProcAddress(library, "DwmSetWindowAttribute");

    if (function != NULL)
    {
        function(window, attribute, value, size);
    }
}

// Rounded corners.
void prodwmround(HWND window)
{
    DWORD value = dwmround;
    dwmset(window, dwmcorner, &value, sizeof(value));
}

// The caption colour.
void prodwmcaption(HWND window, COLORREF colour)
{
    COLORREF value = colour;
    dwmset(window, dwmcaption, &value, sizeof(value));
}

// percent of b over a, channel by channel.
COLORREF promix(COLORREF a, COLORREF b, int percent)
{
    int red = GetRValue(a) + (GetRValue(b) - GetRValue(a)) * percent / 100;
    int green = GetGValue(a) + (GetGValue(b) - GetGValue(a)) * percent / 100;
    int blue = GetBValue(a) + (GetBValue(b) - GetBValue(a)) * percent / 100;
    return RGB(red, green, blue);
}

// The memory bitmap has the size of the area.
probuffer::probuffer(HDC target, const RECT& area)
{
    target_ = target;
    area_ = area;
    memory_ = CreateCompatibleDC(target);
    int width = area.right - area.left > 0 ? area.right - area.left : 1;
    int height = area.bottom - area.top > 0 ? area.bottom - area.top : 1;
    bitmap_ = CreateCompatibleBitmap(target, width, height);
    old_ = SelectObject(memory_, bitmap_);
    SetViewportOrgEx(memory_, -area.left, -area.top, NULL);
}

// One copy to the screen.
probuffer::~probuffer()
{
    SetViewportOrgEx(memory_, 0, 0, NULL);
    BitBlt(target_, area_.left, area_.top, area_.right - area_.left, area_.bottom - area_.top, memory_, 0, 0, SRCCOPY);
    SelectObject(memory_, old_);
    DeleteObject(bitmap_);
    DeleteDC(memory_);
}

// The memory device context.
HDC probuffer::dc() const
{
    return memory_;
}
