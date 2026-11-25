// PageTable.cpp: the table page and the home page, the two page shapes that other pages build on.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "PageTable.h"
#include "ProMotion.h"
#include "Theme.h"

using namespace std;

// Takes one column out of a table (the column with this header). Used where every row would repeat the person who is signed in.
void dropcolumn(texttable& table, const string& header)
{
    size_t column = 0;

    while (column < table.headers.size() && table.headers[column] != header)
    {
        column++;
    }

    if (column >= table.headers.size() || column == 0)
    {
        return;
    }

    table.headers.erase(table.headers.begin() + column);

    for (size_t row = 0; row < table.rows.size(); row++)
    {
        if (column < table.rows[row].size())
        {
            table.rows[row].erase(table.rows[row].begin() + column);
        }
    }
}

// The base class gets the page number, the short name and the title (a base-class call needs the initializer list).
pagetable::pagetable(int id, const string& name, const string& title, const string& emptymessage)
    : guipage(id, name, title)
{
    primaryid_ = 0;
    count_ = NULL;
    emptymessage_ = emptymessage;
    width_ = 0;
    height_ = 0;
}

// The title first, then the list, then the buttons, which is also the order of the Tab key.
void pagetable::createcontrols()
{
    addtitle();
    count_ = makelabel(window_, "", ROLE_HINT);

    // the toolbar buttons sit above the list, so they come first in the Tab order (top to bottom)
    addbuttons();
    list_.create(window_, IDC_LIST_MAIN, emptymessage_);
}

// The count of the toolbar: the number and the word for the rows of the page (one row drops the s). Listed in the report.
static string countof(int id, int rows)
{
    string noun = "rows";

    if (id == PAGE_C_DONATIONS || id == PAGE_D_DONATIONS)
    {
        noun = "donations";
    }
    else if (id == PAGE_C_OFFERS || id == PAGE_D_OFFERS || id == PAGE_R_INCOMING || id == PAGE_R_PAST)
    {
        noun = "offers";
    }
    else if (id == PAGE_C_RECIPIENTS)
    {
        noun = "recipients";
    }
    else if (id == PAGE_C_TRANSACTIONS || id == PAGE_D_GIVEN || id == PAGE_R_RECEIVED)
    {
        noun = "transactions";
    }
    else if (id == PAGE_C_SUMMARY)
    {
        noun = "totals";
    }

    if (rows == 1)
    {
        noun = noun.substr(0, noun.length() - 1);
    }

    return numtext(rows) + " " + noun;
}

// Nothing of its own: the base page paints the card and the circle of the empty state.
void pagetable::paint(HDC dc)
{
    (void)dc;
}

// A page has no buttons unless it makes them.
void pagetable::addbuttons()
{
}

// By default a list is as high as the page.
bool pagetable::huglist() const
{
    return false;
}

// By default nothing is under the list.
int pagetable::belowheight(int width)
{
    (void)width;
    return 0;
}

// By default there is nothing to place under the list.
void pagetable::layoutbelow(guilayout& layout, int left, int top, int width)
{
    (void)layout;
    (void)left;
    (void)top;
    (void)width;
}

// Nothing needs a selection unless the page says so.
void pagetable::updatebuttons()
{
}

// Nothing happens unless the page says so.
void pagetable::clicked(int id)
{
    (void)id;
}

// The button is remembered in the order it was made; the primary one is the default button.
HWND pagetable::addbutton(const string& text, int id, bool primary)
{
    HWND button = makebutton(window_, text, id, primary);
    buttons_.push_back(button);

    if (primary)
    {
        primaryid_ = id;
    }

    return button;
}

// Builds the table and puts it in the list. The selection stays when the row is still there.
void pagetable::refresh()
{
    list_.fill(maketable());
    SetWindowTextA(count_, countof(id_, list_.rowcount()).c_str());
    updatebuttons();

    // the columns may need another width now, so the list is laid out again (the page was sized before)
    if (width_ > 0)
    {
        onsize(width_, height_);
    }
}

// The table card fills the content (equal margins) and the free height: a toolbar row with the count on the left and the
// buttons on the right (the primary one rightmost), then the list with whole rows only. A short table with text under it
// (huglist) is only as high as its rows.
void pagetable::onsize(int width, int height)
{
    width_ = width;
    height_ = height;
    clearshapes();
    int left = 0;
    int contentwidth = 0;
    contentbox(width, left, contentwidth);
    guilayout layout;
    int top = layouttitle(layout, left, contentwidth);
    int toolbar = px(toolbarheight);
    int cardbottom = height - px(pagemargin) - belowheight(contentwidth);
    int listtop = top + toolbar;
    int listheight = cardbottom - listtop - px(gapsmall);

    if (listheight < 0)
    {
        listheight = 0;
    }

    listheight = list_.snapheight(listheight);

    if (huglist() && list_.hugheight() < listheight)
    {
        listheight = list_.hugheight();
    }

    int cardheight = huglist() ? toolbar + listheight + px(gapsmall) : cardbottom - top;
    addcard(left, top, contentwidth, cardheight);
    layout.place(count_, left + px(cardpadding), top + (toolbar - px(textline)) / 2, contentwidth / 2, px(textline));

    if (!buttons_.empty())
    {
        placebuttons(layout, buttons_, left + contentwidth - px(gapbox), top + (toolbar + px(controlheight)) / 2);
    }

    list_.place(layout, left + 1, listtop, contentwidth - 2, listheight);

    layoutbelow(layout, left, top + cardheight, contentwidth);
    layout.apply();
    list_.fitcolumns();
    InvalidateRect(window_, NULL, FALSE);
}

// A button press goes to the page.
bool pagetable::command(int id, int code)
{
    if (code == BN_CLICKED)
    {
        clicked(id);
        return true;
    }

    return false;
}

// The selection of the list changed (this also happens while the list is filled).
bool pagetable::notify(const NMHDR* header)
{
    if (header->idFrom == (UINT_PTR)IDC_LIST_MAIN && header->code == LVN_ITEMCHANGED)
    {
        updatebuttons();
        return true;
    }

    return false;
}

// The number of rows.
int pagetable::rows() const
{
    return list_.rowcount();
}

// The primary button, if the page has one.
int pagetable::defaultid() const
{
    return primaryid_;
}

// The base class gets the page number, the short name and the title (a base-class call needs the initializer list).
pagehome::pagehome(int id, const string& name) : guipage(id, name, "Home")
{
    lastwidth_ = 0;
    lastheight_ = 0;
}

// One pair of statics for each caption: the caption on the left, the value on the right.
void pagehome::createcontrols()
{
    addtitle();

    for (size_t pos = 0; pos < captions_.size(); pos++)
    {
        captionwindows_.push_back(makelabel(window_, captions_[pos], ROLE_MUTED));
        HWND value = makelabel(window_, "", ROLE_VALUE);
        markdata(value);
        valuewindows_.push_back(value);
        notewindows_.push_back(makelabel(window_, "", ROLE_MUTED));
    }
}

// By default no tile has a small line.
void pagehome::tilenotes(vector<string>& out)
{
    (void)out;
}

// Writes the values.
void pagehome::refresh()
{
    vector<string> texts;
    values(texts);

    for (size_t pos = 0; pos < valuewindows_.size() && pos < texts.size(); pos++)
    {
        SetWindowTextA(valuewindows_[pos], texts[pos].c_str());
    }

    vector<string> notes;
    tilenotes(notes);

    for (size_t pos = 0; pos < notewindows_.size(); pos++)
    {
        string note = pos < notes.size() ? notes[pos] : "";
        SetWindowTextA(notewindows_[pos], note.c_str());
        ShowWindow(notewindows_[pos], note.empty() ? SW_HIDE : SW_SHOWNA);
    }
}

// The stat tiles in rows of equal columns (as many as fit with at least 200 design pixels each): each tile is a card with the
// caption (12 muted), the value (28 semibold on a 36 line) and the small line where the data has one; the extras of the page follow under the tiles.
void pagehome::onsize(int width, int height)
{
    clearshapes();
    int left = 0;
    int contentwidth = 0;
    contentbox(width, left, contentwidth);
    guilayout layout;
    int top = layouttitle(layout, left, contentwidth);
    int count = (int)captionwindows_.size();
    int gap = px(gapbox);
    int columns = (contentwidth + gap) / (px(tileminwidth) + gap);
    columns = columns < 1 ? 1 : (columns > count ? count : columns);
    columns = columns < 1 ? 1 : columns;
    int tilewidth = (contentwidth - (columns - 1) * gap) / columns;
    int pad = px(cardpadding);
    int tilehigh = px(tileheight);
    int bottom = top;

    for (int pos = 0; pos < count; pos++)
    {
        int x = left + (pos % columns) * (tilewidth + gap);
        int y = top + (pos / columns) * (tilehigh + gap);
        addcard(x, y, tilewidth, tilehigh);
        int textroom = tilewidth - 2 * pad;
        layout.place(captionwindows_[pos], x + pad, y + pad, textroom, px(captionline));
        layout.place(valuewindows_[pos], x + pad, y + pad + px(captionline), textroom, px(valueline));
        layout.place(notewindows_[pos], x + pad, y + pad + px(captionline + valueline), textroom, px(captionline));
        bottom = y + tilehigh;
    }

    layoutextra(layout, left, bottom + px(pagemargin) - px(gapsmall), contentwidth, height - px(pagemargin));
    layout.apply();
    lastwidth_ = width;
    lastheight_ = height;
    InvalidateRect(window_, NULL, FALSE);
}

// A plain home page has nothing below its lines.
void pagehome::layoutextra(guilayout& layout, int left, int top, int width, int limit)
{
    (void)layout;
    (void)left;
    (void)top;
    (void)width;
    (void)limit;
}

// A home page has no button.
int pagehome::maxbuttons() const
{
    return 0;
}
