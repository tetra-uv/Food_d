// DonationEdit.cpp: change or delete a donor's own donation while none of it was given out yet.

#include "DonationEdit.h"

#include "Exceptions.h"
#include "hashindex.h"

#include <sstream>
#include <vector>

using namespace std;

// Turns a whole number into text (backend files keep their own copy, as Workflow.cpp and Tables.cpp do).
static string numtext(int value)
{
    stringstream text;
    text << value;
    return text.str();
}

// Shared checks of edit and delete: the position, the owner and candeditdonation. Returns false with the message.
static bool ownsandcanedit(const dataset& data, const offerstore& store, const EntityId& donor, int pos, string& message)
{
    if (pos < 0 || pos >= (int)data.donations.size())
    {
        message = "That donation could not be found";
        return false;
    }

    if (!(data.donations[pos].donorId() == donor))
    {
        message = "This donation belongs to another donor";
        return false;
    }

    return candeditdonation(data, store, pos, message);
}

// Waiting, untouched and named by no transaction. A finished donation from which nothing was given gets its own reason.
bool candeditdonation(const dataset& data, const offerstore& store, int pos, string& reason)
{
    (void)store;

    if (pos < 0 || pos >= (int)data.donations.size())
    {
        reason = "That donation could not be found";
        return false;
    }

    const Donation& donation = data.donations[pos];
    bool given = donation.remainingQuantity() != donation.originalQuantity();

    for (size_t at = 0; at < data.transactions.size() && !given; at++)
    {
        if (data.transactions[at].donationId() == donation.id())
        {
            given = true;
        }
    }

    if (given)
    {
        reason = "Some of this food was already given out so it cannot be changed";
        return false;
    }

    if (donation.status() != PENDING)
    {
        reason = "This donation is already finished so it cannot be changed";
        return false;
    }

    reason = "";
    return true;
}

// Withdraws the waiting offers (saved first), then replaces the donation and rewrites donations.csv. A failed save puts
// the donation and the offers back.
bool editdonation(dataset& data, offerstore& store, datamanager& files, const EntityId& donor, int pos, FoodType food,
                  int qty, int usable, string& message)
{
    if (!ownsandcanedit(data, store, donor, pos, message))
    {
        return false;
    }

    if (qty < 1)
    {
        message = "The quantity must be at least 1";
        return false;
    }

    if (usable < 1)
    {
        message = "The usable minutes must be at least 1";
        return false;
    }

    Donation old = data.donations[pos];
    offerstore backup = store;
    int withdrawn = 0;

    try
    {
        withdrawn = store.withdrawfordonation(old.id());

        if (withdrawn > 0)
        {
            store.save(files);
        }

        data.donations[pos] = Donation(old.id(), old.donorId(), food, qty, qty, usable, PENDING);
        files.commitdonations(data);
    }
    catch (const AppException&)
    {
        data.donations[pos] = old;
        store = backup;

        if (withdrawn > 0)
        {
            try
            {
                store.save(files);
            }
            catch (const AppException&)
            {
            }
        }

        message = "The donation could not be changed because the files could not be saved so nothing was changed";
        return false;
    }

    message = "Donation " + old.id().toString() + " updated";

    if (withdrawn > 0)
    {
        message = message + " and " + numtext(withdrawn) + " waiting offers were withdrawn";
    }

    return true;
}

// Removes the offer rows (saved first), then the donation, rebuilds the donation index and rewrites donations.csv.
// A failed save puts the donations, the index and the offers back.
bool deletedonation(dataset& data, offerstore& store, datamanager& files, const EntityId& donor, int pos, string& message)
{
    if (!ownsandcanedit(data, store, donor, pos, message))
    {
        return false;
    }

    EntityId id = data.donations[pos].id();
    vector<Donation> olddonations = data.donations;
    hashindex oldindex = data.donationindex;
    offerstore backup = store;
    int removed = 0;

    try
    {
        removed = store.removefordonation(id);

        if (removed > 0)
        {
            store.save(files);
        }

        data.donations.erase(data.donations.begin() + pos);
        hashindex fresh;

        for (size_t at = 0; at < data.donations.size(); at++)
        {
            fresh.put(data.donations[at].id().toString(), (int)at);
        }

        data.donationindex = fresh;
        files.commitdonations(data);
    }
    catch (const AppException&)
    {
        data.donations = olddonations;
        data.donationindex = oldindex;
        store = backup;

        if (removed > 0)
        {
            try
            {
                store.save(files);
            }
            catch (const AppException&)
            {
            }
        }

        message = "The donation could not be deleted because the files could not be saved so nothing was changed";
        return false;
    }

    message = "Donation " + id.toString() + " deleted";

    if (removed > 0)
    {
        message = message + " and " + numtext(removed) + " offers were removed";
    }

    return true;
}
