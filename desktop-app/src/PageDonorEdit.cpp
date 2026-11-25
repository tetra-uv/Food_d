// PageDonorEdit.cpp: the hidden Edit donation page of a donor. It is reached only from My donations, keeps My donations
// active in the sidebar and changes the donation with one DonationEdit call.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "AppState.h"
#include "DonationEdit.h"
#include "GuiShell.h"
#include "PageDonorEdit.h"
#include "Theme.h"
#include "resource.h"

#include <string>

using namespace std;

// Edit donation: food type, quantity and usable minutes filled with the current values, one red problem line, Cancel and
// Save changes.
class donoredit : public guipage
{
public:
    // The base class gets the page number, the short name and the title (a base-class call needs the initializer list).
    donoredit() : guipage(PAGE_D_EDIT, "editdonation", "Edit donation")
    {
        foodlabel_ = NULL;
        food_ = NULL;
        quantitylabel_ = NULL;
        quantity_ = NULL;
        usablelabel_ = NULL;
        usable_ = NULL;
        error_ = NULL;
        cancel_ = NULL;
        save_ = NULL;
        position_ = -1;
    }

    // Chooses the donation (its position in the data) before the page is shown.
    void setdonation(int position)
    {
        position_ = position;
    }

    // The fields in the order of the Tab key: food, quantity, minutes, Cancel, Save changes.
    void createcontrols()
    {
        addtitle();
        foodlabel_ = makelabel(window_, "Food type", ROLE_LABEL);
        food_ = makecombo(window_, IDC_E_FOOD);
        SendMessageA(food_, CB_ADDSTRING, 0, (LPARAM)"COOKED");
        SendMessageA(food_, CB_ADDSTRING, 0, (LPARAM)"PACKAGED");
        SendMessageA(food_, CB_ADDSTRING, 0, (LPARAM)"PRODUCE");
        SendMessageA(food_, CB_ADDSTRING, 0, (LPARAM)"BAKERY");
        quantitylabel_ = makelabel(window_, "Quantity", ROLE_LABEL);
        quantity_ = makeedit(window_, IDC_E_QTY, true, 9);
        usablelabel_ = makelabel(window_, "Usable minutes", ROLE_LABEL);
        usable_ = makeedit(window_, IDC_E_USABLE, true, 9);
        error_ = makelabel(window_, "", ROLE_ERROR, IDC_E_ERROR);
        cancel_ = makebutton(window_, "Cancel", IDC_E_CANCEL, false);
        save_ = makebutton(window_, "Save changes", IDC_E_SAVE, true);
    }

    // The values are loaded when the page is shown.
    void refresh()
    {
    }

    // Every time the page is shown it holds the current values of the chosen donation and no old error.
    void onshow()
    {
        SetWindowTextA(error_, "");

        if (position_ < 0 || position_ >= (int)theapp->data.donations.size())
        {
            return;
        }

        const Donation& donation = theapp->data.donations[position_];
        SendMessageA(food_, CB_SETCURSEL, (WPARAM)donation.foodType(), 0);
        SetWindowTextA(quantity_, numtext(donation.originalQuantity()).c_str());
        SetWindowTextA(usable_, numtext(donation.usableMinutes()).c_str());
    }

    // The same column as Add donation: labels above the fields, the error line under them, the buttons at the bottom right.
    void onsize(int width, int height)
    {
        int left = 0;
        int columnwidth = 0;
        formcolumn(width, left, columnwidth);
        guilayout layout;
        int top = layouttitle(layout, left, columnwidth);
        top = placefield(layout, foodlabel_, food_, NULL, left, top, columnwidth, px(choicewidth), px(droplistheight));
        top = placefield(layout, quantitylabel_, quantity_, NULL, left, top, columnwidth, px(numberwidth), px(controlheight));
        top = placefield(layout, usablelabel_, usable_, NULL, left, top, columnwidth, px(numberwidth), px(controlheight));
        layout.place(error_, left, top, columnwidth, 2 * px(textline));
        vector<HWND> buttons;
        buttons.push_back(cancel_);
        buttons.push_back(save_);
        placebuttons(layout, buttons, left + columnwidth, height - px(pagemargin));
        layout.apply();
    }

    // Save changes and Cancel.
    bool command(int id, int code)
    {
        if (code != BN_CLICKED)
        {
            return false;
        }

        if (id == IDC_E_SAVE)
        {
            save();
            return true;
        }

        if (id == IDC_E_CANCEL)
        {
            shellshow(PAGE_D_DONATIONS, false);
            return true;
        }

        return false;
    }

    // Enter saves.
    int defaultid() const
    {
        return IDC_E_SAVE;
    }

    // The primary button and one other (the calm rule for form pages).
    int maxbuttons() const
    {
        return 2;
    }

    // My donations stays chosen in the sidebar.
    int navid() const
    {
        return PAGE_D_DONATIONS;
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

    // Checks the numbers, calls editdonation and goes back to My donations.
    void save()
    {
        int quantity = 0;
        int usable = 0;

        if (!readnumber(quantity_, quantity) || quantity < 1)
        {
            problem("Please type the quantity as a whole number 1 or more", quantity_);
            return;
        }

        if (!readnumber(usable_, usable) || usable < 1)
        {
            problem("Please type the usable minutes as a whole number 1 or more", usable_);
            return;
        }

        if (shellwho() != WHO_DONOR || position_ < 0 || position_ >= (int)theapp->data.donations.size())
        {
            problem("Please choose a donation on My donations first", NULL);
            return;
        }

        string id = theapp->data.donations[position_].id().toString();
        int food = (int)SendMessageA(food_, CB_GETCURSEL, 0, 0);
        string message = "";
        EntityId donor = EntityId::parse(shellpersonid(), "DN");

        if (!editdonation(theapp->data, theapp->offers, *theapp->files, donor, position_, (FoodType)food, quantity, usable, message))
        {
            problem(message, NULL);
            return;
        }

        shellshow(PAGE_D_DONATIONS, false);
        shellstatus("Donation " + id + " updated");
    }

    HWND foodlabel_;       // "Food type"
    HWND food_;            // the food type drop-down
    HWND quantitylabel_;   // "Quantity"
    HWND quantity_;        // the quantity box
    HWND usablelabel_;     // "Usable minutes"
    HWND usable_;          // the minutes box
    HWND error_;           // the one red problem line
    HWND cancel_;          // Cancel
    HWND save_;            // Save changes (the primary button)
    int position_;         // the position of the donation in the data (-1 before one is chosen)
};

// Makes the page.
guipage* newpagedonoredit()
{
    return new donoredit();
}

// Chooses the donation and shows the page.
void openeditdonation(int donationpos)
{
    donoredit* page = static_cast<donoredit*>(shellpage(PAGE_D_EDIT));

    if (page == NULL)
    {
        return;
    }

    page->setdonation(donationpos);
    shellshow(PAGE_D_EDIT, true);
}
