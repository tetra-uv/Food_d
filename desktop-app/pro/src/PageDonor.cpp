// PageDonor.cpp: the pages of a donor. Every page reads the data through the backend (Dashboard, Queries, Tables)
// and every action is one Workflow call; the pages hold no business logic.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "AppState.h"
#include "Dashboard.h"
#include "DonationEdit.h"
#include "Exceptions.h"
#include "Flags.h"
#include "GuiMessage.h"
#include "GuiShell.h"
#include "PageDonorEdit.h"
#include "PageTable.h"
#include "Pages.h"
#include "Queries.h"
#include "Tables.h"
#include "TextTools.h"
#include "Theme.h"
#include "Workflow.h"
#include "resource.h"

#include <string>
#include <vector>

using namespace std;

// The id of the donor who is signed in.
static EntityId donorid()
{
    return EntityId::parse(shellpersonid(), "DN");
}

// True when a donor is signed in (the pages are only shown then, but a test may refresh them earlier).
static bool donorsignedin()
{
    return shellwho() == WHO_DONOR && !shellpersonid().empty();
}

// The most notes the donor Home shows; the rest is counted in one more line.
static const int maxdonornotes = 6;

// Home: four numbers and, when some of the donor's open donations look like typing errors, one note for each of them.
class donorhome : public pagehome
{
public:
    // The base class gets the page number and the short name (a base-class call needs the initializer list).
    donorhome() : pagehome(PAGE_D_HOME, "home")
    {
        captions_.push_back("My donations");
        captions_.push_back("Still open");
        captions_.push_back("Offers waiting");
        captions_.push_back("Food given out");
        notescaption_ = NULL;
        morenote_ = NULL;

        for (int pos = 0; pos < maxdonornotes; pos++)
        {
            notelabels_[pos] = NULL;
        }
    }

    // The lines come from the base page; the caption and the note lines are added after them.
    void createcontrols()
    {
        pagehome::createcontrols();
        notescaption_ = makelabel(window_, "Notes about your donations", ROLE_MUTED);

        for (int pos = 0; pos < maxdonornotes; pos++)
        {
            notelabels_[pos] = makelabel(window_, "", ROLE_BANNER);
        }

        morenote_ = makelabel(window_, "", ROLE_TEXT);
    }

    // The numbers and the notes. Layout is done again because the number of notes can change.
    void refresh()
    {
        pagehome::refresh();
        notes_.clear();

        if (donorsignedin())
        {
            notes_ = flagnotesfordonor(theapp->data, donorid());
        }

        for (int pos = 0; pos < maxdonornotes; pos++)
        {
            SetWindowTextA(notelabels_[pos], pos < (int)notes_.size() ? notes_[pos].c_str() : "");
        }

        if (lastwidth_ > 0)
        {
            onsize(lastwidth_, lastheight_);
        }
    }

protected:
    // The notes sit under the numbers. A note wraps and is never cut off: when the next one does not fit in the free room,
    // the rest is counted in the line "and N more".
    void layoutextra(guilayout& layout, int left, int top, int width, int limit)
    {
        int total = (int)notes_.size();
        ShowWindow(notescaption_, total > 0 ? SW_SHOWNA : SW_HIDE);
        ShowWindow(morenote_, SW_HIDE);

        if (total > 0)
        {
            layout.place(notescaption_, left, top, width, px(captionline));
            top = top + px(captionline + gaptiny);
        }

        int shown = 0;

        for (int pos = 0; pos < maxdonornotes; pos++)
        {
            int height = measureheight(notelabels_[pos], width - px(bannerinset)) + px(gaptiny) + px(bannerextra);
            int reserve = (total - (pos + 1) > 0) ? px(textline + gaptiny) : 0;

            if (pos >= total || (shown > 0 && top + height + reserve > limit))
            {
                ShowWindow(notelabels_[pos], SW_HIDE);
                continue;
            }

            ShowWindow(notelabels_[pos], SW_SHOWNA);
            placebanner(layout, notelabels_[pos], left, top, width);
            top = top + height + px(gapsmall);
            shown++;
        }

        if (total > shown)
        {
            SetWindowTextA(morenote_, ("and " + numtext(total - shown) + " more").c_str());
            ShowWindow(morenote_, SW_SHOWNA);
            layout.place(morenote_, left, top, width, px(textline));
        }
    }

    // The numbers of the Dashboard.
    void values(vector<string>& out)
    {
        donorstats stats;

        if (donorsignedin())
        {
            stats = donorstatsof(theapp->data, theapp->offers, donorid());
        }

        out.push_back(numtext(stats.donations));
        out.push_back(numtext(stats.opendonations));
        out.push_back(numtext(stats.pendingoffers));
        out.push_back(numtext(stats.givenout));
    }

private:
    HWND notescaption_;                 // "Notes about your donations"
    HWND notelabels_[maxdonornotes];    // one wrapped note for each of the first flagged donations
    HWND morenote_;                     // "and N more"
    vector<string> notes_;              // every note, in plain words
};

// My donations: the donations of the donor, with Delete donation and Edit donation for the chosen one (a double click on a
// row is the same as Edit donation). Add donation is a page of the sidebar.
class donordonations : public pagetable
{
public:
    // The base class gets the page number, the names and the empty message (a base-class call needs the initializer list).
    donordonations() : pagetable(PAGE_D_DONATIONS, "donations", "My donations", "No donations yet")
    {
    }

    // A double click on a row edits that donation; every other notification goes to the table page.
    bool notify(const NMHDR* header)
    {
        if (header->idFrom == (UINT_PTR)IDC_LIST_MAIN && header->code == (UINT)NM_DBLCLK)
        {
            clicked(IDC_D_EDIT_SEL);
            return true;
        }

        return pagetable::notify(header);
    }

protected:
    // Delete donation, then the primary Edit donation.
    void addbuttons()
    {
        addbutton("Delete donation", IDC_D_DELETE_SEL, false);
        addbutton("Edit donation", IDC_D_EDIT_SEL, true);
    }

    // Both buttons need a chosen row.
    void updatebuttons()
    {
        BOOL chosen = chosenposition() >= 0 ? TRUE : FALSE;
        EnableWindow(buttons_[0], chosen);
        EnableWindow(buttons_[1], chosen);
    }

    // Both first ask candeditdonation and show its reason when the donation cannot be changed.
    void clicked(int id)
    {
        int pos = chosenposition();

        if (pos < 0 || (id != IDC_D_EDIT_SEL && id != IDC_D_DELETE_SEL))
        {
            return;
        }

        string reason = "";

        if (!candeditdonation(theapp->data, theapp->offers, pos, reason))
        {
            guishowerror(window_, reason);
            return;
        }

        if (id == IDC_D_EDIT_SEL)
        {
            openeditdonation(pos);
            return;
        }

        if (!guiaskyes(window_, "Delete this donation and remove its waiting offers?"))
        {
            return;
        }

        string removed = theapp->data.donations[pos].id().toString();
        string message = "";
        bool deleted = deletedonation(theapp->data, theapp->offers, *theapp->files, donorid(), pos, message);
        refresh();

        if (deleted)
        {
            shellstatus("Donation " + removed + " deleted");
        }
        else
        {
            guishowerror(window_, message);
        }
    }

    // The position in the data of the chosen row, or -1.
    int chosenposition() const
    {
        int pos = -1;
        string id = list_.selectedid();

        if (id.empty() || !theapp->data.donationindex.get(id, pos))
        {
            return -1;
        }

        return pos;
    }

    // The donations of this donor.
    texttable maketable()
    {
        vector<int> mine;

        if (donorsignedin())
        {
            mine = donationsofdonor(theapp->data, donorid());
        }

        texttable table = donationstable(theapp->data, theapp->offers, mine);
        dropcolumn(table, "Donor");
        return table;
    }
};

// Offers: the offers that were made for the donations of the donor.
class donoroffers : public pagetable
{
public:
    // The base class gets the page number, the names and the empty message (a base-class call needs the initializer list).
    donoroffers() : pagetable(PAGE_D_OFFERS, "offers", "Offers", "No offers yet")
    {
    }

protected:
    // The offers for this donor's donations.
    texttable maketable()
    {
        vector<int> mine;

        if (donorsignedin())
        {
            mine = offersfordonor(theapp->data, theapp->offers, donorid());
        }

        texttable table = offerstable(theapp->data, theapp->offers, mine);
        dropcolumn(table, "Donor");
        return table;
    }
};

// Given out: the food of this donor that reached a recipient.
class donorgiven : public pagetable
{
public:
    // The base class gets the page number, the names and the empty message (a base-class call needs the initializer list).
    donorgiven() : pagetable(PAGE_D_GIVEN, "given", "Given out", "No food has been given out yet")
    {
    }

protected:
    // The transactions of this donor.
    texttable maketable()
    {
        vector<int> mine;

        if (donorsignedin())
        {
            mine = transactionsofdonor(theapp->data, donorid());
        }

        texttable table = transactionstable(theapp->data, mine);
        dropcolumn(table, "Donor");
        return table;
    }
};

// Add donation: food type, quantity and usable minutes. A problem is shown as one red line and the page stays.
class donoradd : public guipage
{
public:
    // The base class gets the page number, the short name and the title (a base-class call needs the initializer list).
    donoradd() : guipage(PAGE_D_ADD, "add", "Add donation")
    {
        foodlabel_ = NULL;
        food_ = NULL;
        quantitylabel_ = NULL;
        quantity_ = NULL;
        quantityhelp_ = NULL;
        usablelabel_ = NULL;
        usable_ = NULL;
        usablehelp_ = NULL;
        error_ = NULL;
        cancel_ = NULL;
        save_ = NULL;
    }

    // The fields in the order of the Tab key: food, quantity, minutes, Cancel, Save.
    void createcontrols()
    {
        addtitle();
        foodlabel_ = makelabel(window_, "Food type", ROLE_LABEL);
        food_ = makecombo(window_, IDC_D_FOOD);
        SendMessageA(food_, CB_ADDSTRING, 0, (LPARAM)"COOKED");
        SendMessageA(food_, CB_ADDSTRING, 0, (LPARAM)"PACKAGED");
        SendMessageA(food_, CB_ADDSTRING, 0, (LPARAM)"PRODUCE");
        SendMessageA(food_, CB_ADDSTRING, 0, (LPARAM)"BAKERY");
        SendMessageA(food_, CB_SETCURSEL, 0, 0);

        quantitylabel_ = makelabel(window_, "Quantity", ROLE_LABEL);
        quantity_ = makeedit(window_, IDC_D_QTY, true, 9);
        quantityhelp_ = makelabel(window_, "how many portions", ROLE_MUTED);
        usablelabel_ = makelabel(window_, "Usable minutes", ROLE_LABEL);
        usable_ = makeedit(window_, IDC_D_USABLE, true, 9);
        usablehelp_ = makelabel(window_, "how long the food stays good", ROLE_MUTED);
        error_ = makelabel(window_, "", ROLE_ERROR, IDC_D_ERROR);
        cancel_ = makebutton(window_, "Cancel", IDC_D_CANCEL, false);
        save_ = makebutton(window_, "Save", IDC_D_SAVE, true);
    }

    // A form has nothing to refresh: it is cleared when it is shown.
    void refresh()
    {
    }

    // Every time the form is shown it starts empty, so an old error or an old entry is never left behind.
    void onshow()
    {
        SendMessageA(food_, CB_SETCURSEL, 0, 0);
        SetWindowTextA(quantity_, "");
        SetWindowTextA(usable_, "");
        SetWindowTextA(error_, "");
    }

    // The form column with labels above the fields, the error line under them and the buttons at the bottom right.
    void onsize(int width, int height)
    {
        int left = 0;
        int columnwidth = 0;
        formcolumn(width, left, columnwidth);
        guilayout layout;
        int top = layouttitle(layout, left, columnwidth);
        clearshapes();
        int pad = px(cardpadding);
        int cardbottom = height - px(pagemargin);
        addcard(left, top, columnwidth, cardbottom - top);
        left = left + pad;
        columnwidth = columnwidth - 2 * pad;
        top = top + pad;
        top = placefield(layout, foodlabel_, food_, NULL, left, top, columnwidth, px(choicewidth), px(droplistheight));
        top = placefield(layout, quantitylabel_, quantity_, quantityhelp_, left, top, columnwidth, px(numberwidth),
                         px(controlheight));
        top = placefield(layout, usablelabel_, usable_, usablehelp_, left, top, columnwidth, px(numberwidth),
                         px(controlheight));
        layout.place(error_, left, top, columnwidth, 2 * px(textline));
        vector<HWND> buttons;
        buttons.push_back(cancel_);
        buttons.push_back(save_);
        placebuttons(layout, buttons, left + columnwidth, cardbottom - pad);
        layout.apply();
        InvalidateRect(window_, NULL, FALSE);
    }

    // Save and Cancel.
    bool command(int id, int code)
    {
        if (code != BN_CLICKED)
        {
            return false;
        }

        if (id == IDC_D_SAVE)
        {
            save();
            return true;
        }

        if (id == IDC_D_CANCEL)
        {
            shellback();
            return true;
        }

        return false;
    }

    // Enter saves.
    int defaultid() const
    {
        return IDC_D_SAVE;
    }

    // The primary button and one other (the calm rule for form pages).
    int maxbuttons() const
    {
        return 2;
    }

private:
    // Shows one plain problem line and puts the cursor in the field that needs attention.
    void problem(const string& text, HWND field)
    {
        SetWindowTextA(error_, text.c_str());

        if (field != NULL)
        {
            SetFocus(field);
        }
    }

    // Checks the numbers, makes the donation with createdonation and goes to My donations.
    void save()
    {
        int quantity = 0;
        int usable = 0;

        if (!readnumber(quantity_, quantity))
        {
            problem("Please type the quantity as a whole number 1 or more", quantity_);
            return;
        }

        if (!readnumber(usable_, usable))
        {
            problem("Please type the usable minutes as a whole number 1 or more", usable_);
            return;
        }

        if (!donorsignedin())
        {
            problem("Please sign in as a donor first", NULL);
            return;
        }

        int food = (int)SendMessageA(food_, CB_GETCURSEL, 0, 0);
        string message = "";
        int pos = -1;

        try
        {
            pos = createdonation(theapp->data, *theapp->files, donorid(), (FoodType)food, quantity, usable, message);
        }
        catch (const AppException& failure)
        {
            message = "The donation could not be added because " + plaintext(failure.what());
        }

        if (pos < 0)
        {
            problem(message, NULL);
            return;
        }

        string added = theapp->data.donations[pos].id().toString();
        shellshow(PAGE_D_DONATIONS, false);
        shellstatus("Donation " + added + " added");
    }

    HWND foodlabel_;       // "Food type"
    HWND food_;            // the food type drop-down
    HWND quantitylabel_;   // "Quantity"
    HWND quantity_;        // the quantity box
    HWND quantityhelp_;    // the grey helper line next to it
    HWND usablelabel_;     // "Usable minutes"
    HWND usable_;          // the minutes box
    HWND usablehelp_;      // the grey helper line next to it
    HWND error_;           // the one red problem line
    HWND cancel_;          // Cancel
    HWND save_;            // Save (the primary button)
};

// Makes the donor pages.
guipage* newpagedonorhome()
{
    return new donorhome();
}

guipage* newpagedonordonations()
{
    return new donordonations();
}

guipage* newpagedonoroffers()
{
    return new donoroffers();
}

guipage* newpagedonorgiven()
{
    return new donorgiven();
}

guipage* newpagedonoradd()
{
    return new donoradd();
}
