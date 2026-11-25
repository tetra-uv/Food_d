// PageRecipient.cpp: the pages of a recipient. Every page reads the data through the backend and every action is one
// Workflow call; the pages hold no business logic. Every action has one home: answering offers is on Incoming offers,
// changing the details is on My details.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "AppState.h"
#include "Dashboard.h"
#include "Exceptions.h"
#include "Flags.h"
#include "GuiMessage.h"
#include "GuiShell.h"
#include "PageTable.h"
#include "Pages.h"
#include "Queries.h"
#include "RecipientUpdate.h"
#include "Tables.h"
#include "TextTools.h"
#include "Theme.h"
#include "Workflow.h"
#include "resource.h"

#include <string>
#include <vector>

using namespace std;

// The id of the recipient who is signed in.
static EntityId recipientid()
{
    return EntityId::parse(shellpersonid(), "R");
}

// True when a recipient is signed in (the pages are only shown then, but a test may refresh them earlier).
static bool recipientsignedin()
{
    return shellwho() == WHO_RECIPIENT && !shellpersonid().empty();
}

// The position of the signed-in recipient in the recipient list, or -1.
static int recipientposition()
{
    int pos = -1;

    if (!recipientsignedin() || !theapp->data.recipientindex.get(shellpersonid(), pos))
    {
        pos = -1;
    }

    return pos;
}

// Home: two numbers and nothing else (the profile lives on My details).
class recipienthome : public pagehome
{
public:
    // The base class gets the page number and the short name (a base-class call needs the initializer list).
    recipienthome() : pagehome(PAGE_R_HOME, "home")
    {
        captions_.push_back("Offers waiting");
        captions_.push_back("Food received");
        note_ = NULL;
    }

    // The stat row and one note line for a flagged room.
    void createcontrols()
    {
        pagehome::createcontrols();
        note_ = makelabel(window_, "", ROLE_BANNER);
    }

    // The numbers and the note of this recipient; the page is laid out again because the note may come or go.
    void refresh()
    {
        pagehome::refresh();
        vector<string> notes;

        if (recipientsignedin())
        {
            notes = flagnotesforrecipient(theapp->data, recipientid());
        }

        SetWindowTextA(note_, notes.empty() ? "" : notes[0].c_str());

        if (lastwidth_ > 0)
        {
            onsize(lastwidth_, lastheight_);
        }
    }

protected:
    // The note under the stat row, wrapped and never cut off; hidden when the room is fine.
    void layoutextra(guilayout& layout, int left, int top, int width, int limit)
    {
        (void)limit;

        if (GetWindowTextLengthA(note_) == 0)
        {
            ShowWindow(note_, SW_HIDE);
            return;
        }

        ShowWindow(note_, SW_SHOWNA);
        placebanner(layout, note_, left, top, width);
    }

    // The numbers of the Dashboard.
    void values(vector<string>& out)
    {
        recipientstats stats;

        if (recipientsignedin())
        {
            stats = recipientstatsof(theapp->data, theapp->offers, recipientid());
        }

        out.push_back(numtext(stats.incoming));
        out.push_back(numtext(stats.received));
    }

private:
    HWND note_;   // the note about a flagged room (empty when the room is fine)
};

// Incoming offers: the offers that wait for an answer. Decline and Accept act on the selected one.
class recipientincoming : public pagetable
{
public:
    // The base class gets the page number, the names and the empty message (a base-class call needs the initializer list).
    recipientincoming() : pagetable(PAGE_R_INCOMING, "incoming", "Incoming offers", "No offers are waiting for you")
    {
    }

protected:
    // The waiting offers of this recipient (the Recipient column would repeat the same name in every row).
    texttable maketable()
    {
        vector<int> mine;

        if (recipientsignedin())
        {
            mine = offersforrecipient(theapp->offers, recipientid(), true);
        }

        texttable table = offerstable(theapp->data, theapp->offers, mine);
        dropcolumn(table, "Recipient");
        return table;
    }

    // Decline offer, then the primary Accept offer.
    void addbuttons()
    {
        addbutton("Decline offer", IDC_R_DECLINE, false);
        addbutton("Accept offer", IDC_R_ACCEPT, true);
    }

    // Both buttons need a selected offer.
    void updatebuttons()
    {
        bool chosen = selectedposition() >= 0;
        EnableWindow(buttons_[0], chosen ? TRUE : FALSE);
        EnableWindow(buttons_[1], chosen ? TRUE : FALSE);
    }

    // Accept and Decline ask first.
    void clicked(int id)
    {
        int pos = selectedposition();

        if (pos < 0)
        {
            return;
        }

        if (id == IDC_R_ACCEPT)
        {
            accept(pos);
        }
        else if (id == IDC_R_DECLINE)
        {
            decline(pos);
        }
    }

private:
    // The position of the selected offer in the offer store, or -1.
    int selectedposition() const
    {
        int pos = -1;
        string id = list_.selectedid();

        if (!id.empty() && !theapp->offers.find(EntityId::parse(id, "OFF"), pos))
        {
            pos = -1;
        }

        return pos;
    }

    // Asks "Accept <qty> of <food> from <donor>?" and then lets the Workflow accept the offer.
    void accept(int pos)
    {
        // everything the question needs is copied first, because accepting can replace the offers in memory
        EntityId offerid = theapp->offers.at(pos).id();
        string quantity = numtext(theapp->offers.at(pos).quantity());
        string food = "food";
        string donor = "the donor";
        int donationpos = 0;

        if (theapp->data.donationindex.get(theapp->offers.at(pos).donationId().toString(), donationpos))
        {
            const Donation& donation = theapp->data.donations[donationpos];
            food = foodToStr(donation.foodType());
            int donorpos = 0;

            if (theapp->data.donorindex.get(donation.donorId().toString(), donorpos))
            {
                donor = theapp->data.donors[donorpos].name();
            }
        }

        if (!guiaskyes(window_, "Accept " + quantity + " of " + food + " from " + donor + "?"))
        {
            return;
        }

        string message = "";
        bool done = false;

        try
        {
            done = acceptoffer(theapp->data, theapp->offers, *theapp->files, offerid, message);
        }
        catch (const AppException& failure)
        {
            message = "The offer could not be accepted because " + plaintext(failure.what());
        }

        refresh();

        if (done)
        {
            shellstatus(message);
        }
        else
        {
            guishowerror(window_, message);
        }
    }

    // Asks "Decline this offer?" and then lets the Workflow decline it.
    void decline(int pos)
    {
        EntityId offerid = theapp->offers.at(pos).id();

        if (!guiaskyes(window_, "Decline this offer?"))
        {
            return;
        }

        string message = "";
        bool done = false;

        try
        {
            done = declineoffer(theapp->offers, *theapp->files, offerid, message);
        }
        catch (const AppException& failure)
        {
            message = "The answer could not be saved because " + plaintext(failure.what());
        }

        refresh();

        if (done)
        {
            shellstatus("Offer " + offerid.toString() + " declined");
        }
        else
        {
            guishowerror(window_, message);
        }
    }
};

// Past offers: the offers that were answered (accepted, declined or withdrawn).
class recipientpast : public pagetable
{
public:
    // The base class gets the page number, the names and the empty message (a base-class call needs the initializer list).
    recipientpast() : pagetable(PAGE_R_PAST, "past", "Past offers", "No past offers yet")
    {
    }

protected:
    // The finished offers of this recipient.
    texttable maketable()
    {
        vector<int> mine;

        if (recipientsignedin())
        {
            mine = pastoffersforrecipient(theapp->offers, recipientid());
        }

        texttable table = offerstable(theapp->data, theapp->offers, mine);
        dropcolumn(table, "Recipient");
        return table;
    }
};

// Received food: the transactions of this recipient.
class recipientreceived : public pagetable
{
public:
    // The base class gets the page number, the names and the empty message (a base-class call needs the initializer list).
    recipientreceived() : pagetable(PAGE_R_RECEIVED, "received", "Received food", "No food received yet")
    {
    }

protected:
    // The transactions of this recipient.
    texttable maketable()
    {
        vector<int> mine;

        if (recipientsignedin())
        {
            mine = transactionsofrecipient(theapp->data, recipientid());
        }

        texttable table = transactionstable(theapp->data, mine);
        dropcolumn(table, "Recipient");
        return table;
    }
};

// The four food types of My details, in the order they are shown and saved, and the control id of the first box.
static const int foodkinds = 4;
static const FoodType foodorder[foodkinds] = {COOKED, PACKAGED, PRODUCE, BAKERY};
static const int foodboxid = 2320;

// My details: shows and changes the capacity, the two yes or no settings and the food types. The only button is Save.
// The saved values are shown every time the page is shown, so leaving the page drops unsaved changes.
class recipientdetails : public guipage
{
public:
    // The base class gets the page number, the short name and the title (a base-class call needs the initializer list).
    recipientdetails() : guipage(PAGE_R_EDIT, "details", "My details")
    {
        capacitylabel_ = NULL;
        capacity_ = NULL;
        accepting_ = NULL;
        pickup_ = NULL;
        foodcaption_ = NULL;

        for (int kind = 0; kind < foodkinds; kind++)
        {
            foods_[kind] = NULL;
        }

        error_ = NULL;
        save_ = NULL;
        warning_ = NULL;
        capacityhelp_ = NULL;
        width_ = 0;
        height_ = 0;
    }

    // The fields in the order of the Tab key: capacity, the two check boxes, the four food type boxes, Save.
    void createcontrols()
    {
        addtitle();
        warning_ = makelabel(window_, "", ROLE_BANNER);
        capacitylabel_ = makelabel(window_, "Capacity", ROLE_LABEL);
        capacity_ = makeedit(window_, IDC_R_CAPACITY, true, 9);
        capacityhelp_ = makelabel(window_, "how many portions you can still take", ROLE_MUTED);
        accepting_ = makecheck(window_, "Accepting donations", IDC_R_ACCEPTING);
        pickup_ = makecheck(window_, "Can collect food", IDC_R_PICKUP);
        foodcaption_ = makelabel(window_, "Food types", ROLE_MUTED);

        for (int kind = 0; kind < foodkinds; kind++)
        {
            foods_[kind] = makecheck(window_, foodToStr(foodorder[kind]), foodboxid + kind);
        }

        error_ = makelabel(window_, "", ROLE_ERROR, IDC_R_ERROR);
        save_ = makebutton(window_, "Save", IDC_R_SAVE, true);
    }

    // The form has nothing else to refresh; the values are loaded when the page is shown.
    void refresh()
    {
    }

    // Every time the page is shown it holds the details that are saved now.
    void onshow()
    {
        SetWindowTextA(error_, "");
        loadvalues();
    }

    // The form column: the capacity with its label above, the two check boxes, the food types, the error line and Save.
    void onsize(int width, int height)
    {
        width_ = width;
        height_ = height;
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
        if (GetWindowTextLengthA(warning_) > 0)
        {
            ShowWindow(warning_, SW_SHOWNA);
            top = top + placebanner(layout, warning_, left, top, columnwidth) + px(gapfield);
        }
        else
        {
            ShowWindow(warning_, SW_HIDE);
        }
        top = placefield(layout, capacitylabel_, capacity_, capacityhelp_, left, top, columnwidth, px(numberwidth),
                         px(controlheight));
        layout.place(accepting_, left, top, columnwidth, px(controlheight));
        top = top + px(controlheight + gapsmall);
        layout.place(pickup_, left, top, columnwidth, px(controlheight));
        top = top + px(controlheight + pagemargin);
        layout.place(foodcaption_, left, top, columnwidth, px(captionline));
        top = top + px(captionline + gaptiny);

        // the four food type boxes in one row when the widest fits a quarter of the column, else in two rows of two
        int widest = 0;

        for (int kind = 0; kind < foodkinds; kind++)
        {
            int need = textwidth(foods_[kind]) + px(controlheight);
            widest = need > widest ? need : widest;
        }

        int perrow = (widest * foodkinds <= columnwidth) ? foodkinds : foodkinds / 2;
        int cell = columnwidth / perrow;

        for (int kind = 0; kind < foodkinds; kind++)
        {
            layout.place(foods_[kind], left + (kind % perrow) * cell, top + (kind / perrow) * px(controlheight + gapsmall), cell,
                         px(controlheight));
        }

        top = top + (foodkinds / perrow) * px(controlheight + gapsmall) + px(gapfield);
        layout.place(error_, left, top, columnwidth, 2 * px(textline));
        vector<HWND> buttons(1, save_);
        placebuttons(layout, buttons, left + columnwidth, cardbottom - pad);
        layout.apply();
        InvalidateRect(window_, NULL, FALSE);
    }

    // Save.
    bool command(int id, int code)
    {
        if (code == BN_CLICKED && id == IDC_R_SAVE)
        {
            save();
            return true;
        }

        return false;
    }

    // Enter saves.
    int defaultid() const
    {
        return IDC_R_SAVE;
    }

    // Only the Save button.
    int maxbuttons() const
    {
        return 1;
    }

private:
    // Shows the values that are saved: capacity, the two settings and a tick for every food type the recipient takes.
    void loadvalues()
    {
        int pos = recipientposition();
        vector<string> notes;

        if (recipientsignedin())
        {
            notes = flagnotesforrecipient(theapp->data, recipientid());
        }

        SetWindowTextA(warning_, notes.empty() ? "" : notes[0].c_str());

        if (width_ > 0)
        {
            onsize(width_, height_);
        }

        if (pos < 0)
        {
            SetWindowTextA(capacity_, "");

            for (int kind = 0; kind < foodkinds; kind++)
            {
                SendMessageA(foods_[kind], BM_SETCHECK, BST_UNCHECKED, 0);
            }

            return;
        }

        const Recipient& recipient = theapp->data.recipients[pos];
        SetWindowTextA(capacity_, numtext(recipient.availableCapacity()).c_str());
        SendMessageA(accepting_, BM_SETCHECK, recipient.accepting() ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageA(pickup_, BM_SETCHECK, recipient.canPickup() ? BST_CHECKED : BST_UNCHECKED, 0);

        for (int kind = 0; kind < foodkinds; kind++)
        {
            SendMessageA(foods_[kind], BM_SETCHECK, recipient.accepts(foodorder[kind]) ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    // Checks the capacity, calls updaterecipientdetails with the ticked food types and says so in the status line. The page stays.
    void save()
    {
        int capacity = 0;

        if (!readnumber(capacity_, capacity))
        {
            SetWindowTextA(error_, "Please type the capacity as a whole number 0 or more");
            SetFocus(capacity_);
            return;
        }

        bool accepting = SendMessageA(accepting_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        bool pickup = SendMessageA(pickup_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        vector<FoodType> types;

        for (int kind = 0; kind < foodkinds; kind++)
        {
            if (SendMessageA(foods_[kind], BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                types.push_back(foodorder[kind]);
            }
        }

        string message = "";
        bool saved = false;

        try
        {
            saved = updaterecipientdetails(theapp->data, *theapp->files, recipientid(), capacity, accepting, pickup, types,
                                           message);
        }
        catch (const AppException& failure)
        {
            message = "Your details could not be saved because " + plaintext(failure.what());
        }

        if (!saved)
        {
            SetWindowTextA(error_, message.c_str());
            return;
        }

        SetWindowTextA(error_, "");
        loadvalues();
        shellstatus("Details saved");
    }

    HWND capacitylabel_;   // "Capacity"
    HWND capacity_;        // the capacity box (digits only)
    HWND capacityhelp_;    // the muted helper line under the capacity
    HWND accepting_;       // check box "Accepting donations"
    HWND pickup_;          // check box "Can collect food"
    HWND foodcaption_;     // "Food types"
    HWND foods_[4];        // one check box per food type, in the order of foodorder
    HWND error_;           // the one red problem line
    HWND save_;            // Save (the only button)
    HWND warning_;         // one warning line above the form when the room is flagged
    int width_;            // the size of the page at the last layout (0 before the first)
    int height_;
};

// Makes the recipient pages.
guipage* newpagerecipienthome()
{
    return new recipienthome();
}

guipage* newpagerecipientincoming()
{
    return new recipientincoming();
}

guipage* newpagerecipientpast()
{
    return new recipientpast();
}

guipage* newpagerecipientreceived()
{
    return new recipientreceived();
}

guipage* newpagerecipientedit()
{
    return new recipientdetails();
}
