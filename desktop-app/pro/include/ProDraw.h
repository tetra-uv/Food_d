#ifndef PRODRAW_H
#define PRODRAW_H

// ProDraw.h: the drawing helpers of the professional program. GDI+ draws the anti-aliased shapes (rounded fills and borders,
// circles, rings, chips, shadows); every text is GDI DrawText with ClearType (decision D4). All sizes are in pixels already
// scaled for the dpi; colours come from the Theme tokens.

#include <windows.h>

#include <string>

using namespace std;

// The kinds of a status chip (each has a text colour and a tint from the Theme).
enum chipkind
{
    CHIP_NEUTRAL,
    CHIP_SUCCESS,
    CHIP_WARNING,
    CHIP_ERROR
};

// Starts GDI+ (call once before any window is made).
void prodrawstart();

// Stops GDI+ (call once at the end).
void prodrawstop();

// Fills a rounded rectangle and draws its 1 px border; CLR_INVALID leaves the fill or the border out. radius 0 draws a plain box.
void proround(HDC dc, const RECT& box, int radius, COLORREF fill, COLORREF border);

// Draws a rounded outline of the given width inside the box.
void proline(HDC dc, const RECT& box, int radius, COLORREF colour, int width);

// The focus ring of the design: a 2 px ring in the text colour with a 1 px light gap inside it, drawn inside the box.
void proring(HDC dc, const RECT& box, int radius);

// Fills a circle (or ellipse) that fits the box.
void procircle(HDC dc, const RECT& box, COLORREF fill);

// Tells whether an icon font exists (Segoe Fluent Icons, else Segoe MDL2 Assets). Without one, no glyph is ever drawn.
bool proiconsready();

// Draws one icon glyph centred in the box, design pixels tall at the present dpi. Does nothing when there is no icon font.
void proglyph(HDC dc, unsigned int code, const RECT& box, COLORREF colour, int design);

// Draws a status chip that starts at the left of the cell and is centred in its height: a rounded soft tint and the word (no dot).
// Returns the right edge of the chip.
int prochip(HDC dc, const RECT& cell, const string& word, int kind, HFONT font);

// The chip kind of a status word of the lists (a flag text of the Check column is always an error chip).
int prochipkind(const string& word, bool checkcolumn);

// Draws text with a font and a colour on a transparent background (DrawText flags as given).
void protext(HDC dc, const string& text, RECT box, COLORREF colour, HFONT font, UINT format);

// Asks DWM (Windows 11) for rounded window corners; ignored where Windows does not support it.
void prodwmround(HWND window);

// Asks DWM (Windows 11) to paint the caption of the window in this colour; ignored where Windows does not support it.
void prodwmcaption(HWND window, COLORREF colour);

// Mixes two colours: percent of b over a.
COLORREF promix(COLORREF a, COLORREF b, int percent);

// A memory bitmap that a window paints into and then copies to the screen in one step (no flicker).
class probuffer
{
public:
    // Makes the memory bitmap for the client area of the window that is being painted.
    probuffer(HDC target, const RECT& area);

    // Copies the bitmap to the screen and frees it.
    ~probuffer();

    // The memory device context to draw into.
    HDC dc() const;

private:
    HDC target_;      // the screen device context
    HDC memory_;      // the memory device context
    HBITMAP bitmap_;  // the bitmap
    HGDIOBJ old_;     // the bitmap that was selected before
    RECT area_;       // the area that is copied
};

#endif
