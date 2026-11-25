// Flags.cpp: the flag texts and the notes about records that look like typing errors.

#include "Flags.h"

#include <sstream>

using namespace std;

// Turns a whole number into text (backend files keep their own copy).
static string numtext(int value)
{
    stringstream text;
    text << value;
    return text.str();
}

// A donation is open while it is waiting or partly given; only open donations make notes.
static bool isopen(const Donation& donation)
{
    return donation.status() == PENDING || donation.status() == PARTIAL;
}

// Tells which of the two donation problems are there.
static void donationproblems(const Donation& donation, bool& big, bool& slow)
{
    big = donation.originalQuantity() > BIGQUANTITY;
    slow = donation.usableMinutes() > LONGMINUTES;
}

// The problem part of a note, for example "a very large quantity of 50000".
static string problemtext(const Donation& donation)
{
    bool big = false;
    bool slow = false;
    donationproblems(donation, big, slow);
    string quantity = "a very large quantity of " + numtext(donation.originalQuantity());
    string minutes = "a very long usable time of " + numtext(donation.usableMinutes()) + " minutes";

    if (big && slow)
    {
        return quantity + " and " + minutes;
    }

    return big ? quantity : minutes;
}

// The name of a donor, or its id when it is not known.
static string donorname(const dataset& data, const EntityId& donor)
{
    int pos = 0;

    if (data.donorindex.get(donor.toString(), pos))
    {
        return data.donors[pos].name();
    }

    return donor.toString();
}

// The note about a flagged recipient.
static string recipientnote(const Recipient& recipient)
{
    return recipient.name() + " has a very large room of " + numtext(recipient.availableCapacity()) + " so please check it";
}

// Joins the two problems with "and" when both are there.
string donationflagtext(const Donation& donation)
{
    bool big = false;
    bool slow = false;
    donationproblems(donation, big, slow);

    if (big && slow)
    {
        return "Very large quantity and very long usable time";
    }

    if (big)
    {
        return "Very large quantity";
    }

    return slow ? "Very long usable time" : "";
}

// Only the room can be flagged.
string recipientflagtext(const Recipient& recipient)
{
    return recipient.availableCapacity() > BIGCAPACITY ? "Very large room" : "";
}

// Open flagged donations first (with the donor name), then flagged recipients.
vector<string> flagnotes(const dataset& data)
{
    vector<string> notes;

    for (size_t pos = 0; pos < data.donations.size(); pos++)
    {
        const Donation& donation = data.donations[pos];

        if (isopen(donation) && !donationflagtext(donation).empty())
        {
            notes.push_back("Donation " + donation.id().toString() + " from " + donorname(data, donation.donorId()) + " has " +
                            problemtext(donation) + " so please check it");
        }
    }

    for (size_t pos = 0; pos < data.recipients.size(); pos++)
    {
        if (!recipientflagtext(data.recipients[pos]).empty())
        {
            notes.push_back(recipientnote(data.recipients[pos]));
        }
    }

    return notes;
}

// The donor's own open flagged donations.
vector<string> flagnotesfordonor(const dataset& data, const EntityId& donor)
{
    vector<string> notes;

    for (size_t pos = 0; pos < data.donations.size(); pos++)
    {
        const Donation& donation = data.donations[pos];

        if (donation.donorId() == donor && isopen(donation) && !donationflagtext(donation).empty())
        {
            notes.push_back("Donation " + donation.id().toString() + " has " + problemtext(donation) + " so please check it");
        }
    }

    return notes;
}

// The recipient's own room.
vector<string> flagnotesforrecipient(const dataset& data, const EntityId& recipient)
{
    vector<string> notes;
    int pos = 0;

    if (data.recipientindex.get(recipient.toString(), pos) && !recipientflagtext(data.recipients[pos]).empty())
    {
        notes.push_back(recipientnote(data.recipients[pos]));
    }

    return notes;
}
