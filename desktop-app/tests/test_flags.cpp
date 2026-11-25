// test_flags: the flag texts and the notes about records that look like typing errors (built in memory, no files).

#include "Flags.h"
#include "TextTools.h"
#include "testkit.h"

#include <string>
#include <vector>

using namespace std;

// Adds a donor to the data set and its index.
void adddonor(dataset& data, int number, const string& name)
{
    data.donors.push_back(Donor(EntityId("DN", number), name, "HOTEL", 0));
    data.donorindex.put(EntityId("DN", number).toString(), (int)data.donors.size() - 1);
}

// Adds a donation to the data set and its index.
void adddonation(dataset& data, int number, int donor, int qty, int usable, DonationStatus status)
{
    int remaining = (status == PENDING || status == CLOSED) ? qty : 0;
    data.donations.push_back(Donation(EntityId("DON", number), EntityId("DN", donor), COOKED, qty, remaining, usable, status));
    data.donationindex.put(EntityId("DON", number).toString(), (int)data.donations.size() - 1);
}

// Adds a recipient to the data set and its index.
void addrecipient(dataset& data, int number, const string& name, int room)
{
    vector<FoodType> types(1, COOKED);
    data.recipients.push_back(Recipient(EntityId("R", number), name, "SHELTER", room, types, true, true, 0));
    data.recipientindex.put(EntityId("R", number).toString(), (int)data.recipients.size() - 1);
}

// Checks whether a list of notes holds a note with this part.
bool hasnote(const vector<string>& notes, const string& part)
{
    for (size_t pos = 0; pos < notes.size(); pos++)
    {
        if (notes[pos].find(part) != string::npos)
        {
            return true;
        }
    }

    return false;
}

// Checks that no note has punctuation.
bool noneplain(const vector<string>& notes)
{
    for (size_t pos = 0; pos < notes.size(); pos++)
    {
        if (haspunct(notes[pos]))
        {
            return false;
        }
    }

    return true;
}

// Runs every check of this test.
int main()
{
    dataset data;
    adddonor(data, 9, "Lakeside Caterers");
    adddonor(data, 10, "Old Town Bakery");
    adddonation(data, 5, 10, 30, 25, PENDING);               // normal
    adddonation(data, 41, 9, 50000, 60, PENDING);            // very large quantity
    adddonation(data, 42, 10, 500, 999999999, PENDING);      // very long usable time
    adddonation(data, 43, 9, 20000, 20000, PENDING);         // both
    adddonation(data, 44, 9, 90000, 60, CLOSED);             // flagged but closed
    adddonation(data, 45, 10, 10000, 10080, PENDING);        // exactly at both limits
    adddonation(data, 46, 10, 10001, 60, PENDING);           // one above the quantity limit
    adddonation(data, 47, 10, 30, 10081, PENDING);           // one above the minutes limit
    addrecipient(data, 1, "Hope Shelter", 150);
    addrecipient(data, 14, "Barakhamba Relief Society", 10000);
    addrecipient(data, 15, "Edge Shelter", 5000);
    addrecipient(data, 16, "Wide Shelter", 5001);

    // flag texts
    check(donationflagtext(data.donations[1]) == "Very large quantity", "DON041 is flagged for its quantity");
    check(donationflagtext(data.donations[2]) == "Very long usable time", "DON042 is flagged for its usable time");
    check(donationflagtext(data.donations[3]) == "Very large quantity and very long usable time", "both problems are joined with and");
    check(donationflagtext(data.donations[0]).empty(), "DON005 is not flagged");
    check(donationflagtext(data.donations[4]) == "Very large quantity", "a closed donation is still flagged as a row");
    check(donationflagtext(data.donations[5]).empty(), "exactly at both limits is not flagged");
    check(donationflagtext(data.donations[6]) == "Very large quantity", "one above the quantity limit is flagged");
    check(donationflagtext(data.donations[7]) == "Very long usable time", "one above the minutes limit is flagged");
    check(recipientflagtext(data.recipients[1]) == "Very large room", "R014 is flagged");
    check(recipientflagtext(data.recipients[0]).empty(), "a normal recipient is not flagged");
    check(recipientflagtext(data.recipients[2]).empty(), "a room exactly at the limit is not flagged");
    check(recipientflagtext(data.recipients[3]) == "Very large room", "a room one above the limit is flagged");

    // coordinator notes
    vector<string> all = flagnotes(data);
    check(hasnote(all, "Donation DON041 from Lakeside Caterers has a very large quantity of 50000 so please check it"), "the coordinator note for DON041");
    check(hasnote(all, "Donation DON042 from Old Town Bakery has a very long usable time of 999999999 minutes so please check it"), "the coordinator note for DON042");
    check(hasnote(all, "has a very large quantity of 20000 and a very long usable time of 20000 minutes so please check it"), "both problems in one note");
    check(hasnote(all, "Barakhamba Relief Society has a very large room of 10000 so please check it"), "the note for R014");
    check(!hasnote(all, "DON044") && !hasnote(all, "DON005") && !hasnote(all, "DON045"), "no note for a closed, a normal or an at the limit donation");
    check(!hasnote(all, "Hope Shelter") && !hasnote(all, "Edge Shelter"), "no note for normal recipients");
    check(all.size() == 7 && noneplain(all), "seven coordinator notes without punctuation");

    // donor notes
    vector<string> mine = flagnotesfordonor(data, EntityId("DN", 9));
    check(hasnote(mine, "Donation DON041 has a very large quantity of 50000 so please check it"), "the donor note leaves out from and the name");
    check(mine.size() == 2 && !hasnote(mine, "DON042") && !hasnote(mine, "DON044"), "a donor sees only his own open flagged donations");
    check(flagnotesfordonor(data, EntityId("DN", 99)).empty(), "an unknown donor has no notes");
    check(noneplain(mine), "donor notes have no punctuation");

    // recipient notes
    vector<string> room = flagnotesforrecipient(data, EntityId("R", 14));
    check(room.size() == 1 && room[0] == "Barakhamba Relief Society has a very large room of 10000 so please check it", "R014 sees its own note");
    check(flagnotesforrecipient(data, EntityId("R", 1)).empty(), "a normal recipient has no note");
    check(noneplain(room), "the recipient note has no punctuation");
    return finish();
}
