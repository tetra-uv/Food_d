// test_donationedit: a donor edits or deletes his own donation, in memory, in donations.csv and in offers.csv.
// Runs only in a fresh folder below the build folder, never on real data.

#include "DonationEdit.h"
#include "Datamanager.h"
#include "Exceptions.h"
#include "OfferStore.h"
#include "TextTools.h"
#include "Workflow.h"
#include "testkit.h"

#include <cstdio>
#include <list>
#include <string>
#include <vector>

using namespace std;

// The header row of offers.csv.
const string OFFERHEADER = "offer_id,donation_id,recipient_id,quantity,travel_minutes,status,txn_id\n";

// Writes the standard fixture with five donations, two transactions and five offers.
// DON001 and DON005 are waiting and untouched (DN001); DON002 is partly given; DON003 is closed; DON004 is completed.
void writecase(const string& folder)
{
    writefixture(folder);
    writefile(folder + "donations.csv",
              "donation_id,donor_id,food_type,original_quantity,remaining_quantity,usable_minutes,status\n"
              "DON001,DN001,COOKED,80,80,60,PENDING\n"
              "DON002,DN002,COOKED,80,50,90,PARTIAL\n"
              "DON003,DN001,PACKAGED,40,40,30,CLOSED\n"
              "DON004,DN002,PACKAGED,30,0,30,COMPLETED\n"
              "DON005,DN001,BAKERY,20,20,45,PENDING\n");
    writefile(folder + "transactions.csv", transactionstext() + "T001,DON002,R001,30,5\nT002,DON004,R002,30,5\n");
    writefile(folder + "offers.csv",
              OFFERHEADER + "OFF001,DON001,R001,10,5,PENDING,\n"
                            "OFF002,DON001,R003,5,5,DECLINED,\n"
                            "OFF003,DON005,R001,5,5,PENDING,\n"
                            "OFF004,DON005,R002,3,5,WITHDRAWN,\n"
                            "OFF005,DON002,R001,30,5,ACCEPTED,T001\n");
}

// Checks whether a text holds a part.
bool contains(const string& text, const string& part)
{
    return text.find(part) != string::npos;
}

// Returns every donation row of a data set as one text.
string donationrows(const dataset& data)
{
    string text = "";

    for (size_t pos = 0; pos < data.donations.size(); pos++)
    {
        text = text + data.donations[pos].toCsvRow() + "\n";
    }

    return text;
}

// Returns the position of a donation id in the vector, or -1.
int findpos(const dataset& data, const string& id)
{
    for (size_t pos = 0; pos < data.donations.size(); pos++)
    {
        if (data.donations[pos].id().toString() == id)
        {
            return (int)pos;
        }
    }

    return -1;
}

// Returns the status of the offer with this id, or -1 when it is not there.
int offerstatus(const offerstore& store, const string& id)
{
    int pos = 0;

    if (!store.find(EntityId::parse(id, "OFF"), pos))
    {
        return -1;
    }

    return (int)store.at(pos).status();
}

// Checks that a message is plain language (not empty, no developer words and no punctuation).
bool plain(const string& message)
{
    return !message.empty() && message.find("xception") == string::npos && !haspunct(message);
}

// Loads a folder into a new data set and offer store, as a restart would.
void reload(const string& folder, dataset& data, offerstore& store)
{
    datamanager files(folder);
    list<string> warnings;
    files.loadall(data, warnings);
    store.load(folder, warnings);
}

// candeditdonation: only a waiting donation from which nothing was given out.
void testcanedit()
{
    string folder = testfolder("donationedit_canedit");
    writecase(folder);
    dataset data;
    offerstore store;
    reload(folder, data, store);
    string reason = "";

    check(candeditdonation(data, store, findpos(data, "DON001"), reason) && reason.empty(), "a waiting untouched donation can be edited");
    check(!candeditdonation(data, store, findpos(data, "DON002"), reason) && contains(reason, "given out") && plain(reason), "a partly given donation is refused with the reason");
    check(!candeditdonation(data, store, findpos(data, "DON003"), reason) && plain(reason), "a closed donation is refused");
    check(!candeditdonation(data, store, findpos(data, "DON004"), reason) && contains(reason, "given out"), "a completed donation is refused");
    check(!candeditdonation(data, store, -1, reason) && !candeditdonation(data, store, 99, reason), "a wrong position is refused");
}

// editdonation: refusals, a failed save, then a real edit checked in memory and after a fresh load.
void testedit()
{
    string folder = testfolder("donationedit_edit");
    writecase(folder);
    datamanager files(folder);
    dataset data;
    offerstore store;
    reload(folder, data, store);
    string message = "";
    int one = findpos(data, "DON001");
    string before = donationrows(data);

    check(!editdonation(data, store, files, EntityId("DN", 2), one, PACKAGED, 500, 120, message) && contains(message, "another donor"), "another donor is refused");
    check(!editdonation(data, store, files, EntityId("DN", 1), one, PACKAGED, 0, 120, message) && plain(message), "quantity 0 is refused");
    check(!editdonation(data, store, files, EntityId("DN", 1), one, PACKAGED, 500, 0, message) && plain(message), "usable 0 is refused");
    check(!editdonation(data, store, files, EntityId("DN", 2), findpos(data, "DON002"), COOKED, 10, 10, message) && contains(message, "given out"), "a partly given donation is refused");
    check(!editdonation(data, store, files, EntityId("DN", 1), findpos(data, "DON003"), COOKED, 10, 10, message) && plain(message), "a closed donation is refused");
    check(donationrows(data) == before && offerstatus(store, "OFF001") == OFFER_PENDING, "refusals change nothing");

    // a read-only donations.csv: the call fails and memory and the offers file stay as they were
    setreadonly(folder + "donations.csv", true);
    bool failed = !editdonation(data, store, files, EntityId("DN", 1), one, PACKAGED, 500, 120, message);
    setreadonly(folder + "donations.csv", false);
    remove((folder + "donations.csv.tmp").c_str());
    check(failed && plain(message), "a read-only donations file makes the edit fail with a plain message");
    check(donationrows(data) == before, "the donations in memory are unchanged after the failed edit");
    check(offerstatus(store, "OFF001") == OFFER_PENDING, "the offer in memory is pending again after the failed edit");
    check(contains(readfile(folder + "offers.csv"), "OFF001,DON001,R001,10,5,PENDING"), "offers.csv holds the pending offer again after the failed edit");

    // the real edit
    check(editdonation(data, store, files, EntityId("DN", 1), one, PACKAGED, 500, 120, message) && plain(message), "the owner edits DON001");
    const Donation& edited = data.donations[one];
    check(edited.id() == EntityId("DON", 1) && edited.donorId() == EntityId("DN", 1), "id and donor stay the same");
    check(edited.foodType() == PACKAGED && edited.originalQuantity() == 500 && edited.remainingQuantity() == 500, "food and quantities changed in memory");
    check(edited.usableMinutes() == 120 && edited.status() == PENDING, "usable minutes changed and the donation is still waiting");
    check(offerstatus(store, "OFF001") == OFFER_WITHDRAWN && offerstatus(store, "OFF002") == OFFER_DECLINED, "the waiting offer is withdrawn and the declined one is untouched");

    dataset fresh;
    offerstore freshstore;
    reload(folder, fresh, freshstore);
    int again = findpos(fresh, "DON001");
    check(again >= 0 && fresh.donations[again].originalQuantity() == 500 && fresh.donations[again].usableMinutes() == 120, "a fresh load sees the new quantity and minutes");
    check(offerstatus(freshstore, "OFF001") == OFFER_WITHDRAWN, "a fresh load sees the withdrawn offer");
    check(files.checkinvariants(fresh).empty(), "checkinvariants finds nothing after the edit");
}

// deletedonation: refusals, a failed save, then a real delete checked in memory and after a fresh load, and a new id.
void testdelete()
{
    string folder = testfolder("donationedit_delete");
    writecase(folder);
    datamanager files(folder);
    dataset data;
    offerstore store;
    reload(folder, data, store);
    string message = "";
    int five = findpos(data, "DON005");
    string before = donationrows(data);

    check(!deletedonation(data, store, files, EntityId("DN", 2), five, message) && contains(message, "another donor"), "another donor cannot delete");
    check(!deletedonation(data, store, files, EntityId("DN", 2), findpos(data, "DON002"), message) && contains(message, "given out"), "a partly given donation cannot be deleted");
    check(!deletedonation(data, store, files, EntityId("DN", 2), findpos(data, "DON004"), message) && plain(message), "a completed donation cannot be deleted");
    check(donationrows(data) == before && store.count() == 5, "refusals change nothing");

    // a read-only donations.csv: the call fails and memory, the index and the offers stay as they were
    setreadonly(folder + "donations.csv", true);
    bool failed = !deletedonation(data, store, files, EntityId("DN", 1), five, message);
    setreadonly(folder + "donations.csv", false);
    remove((folder + "donations.csv.tmp").c_str());
    int indexed = -1;
    check(failed && plain(message), "a read-only donations file makes the delete fail with a plain message");
    check(donationrows(data) == before && data.donationindex.get("DON005", indexed) && indexed == five, "donations and index are unchanged after the failed delete");
    check(store.count() == 5 && contains(readfile(folder + "offers.csv"), "OFF003,DON005"), "the offers are back in memory and in offers.csv after the failed delete");

    // the real delete
    check(deletedonation(data, store, files, EntityId("DN", 1), five, message) && plain(message), "the owner deletes DON005");
    check(data.donations.size() == 4 && findpos(data, "DON005") < 0, "DON005 is gone from memory");
    check(!data.donationindex.get("DON005", indexed), "the index no longer knows DON005");
    check(data.donationindex.get("DON004", indexed) && indexed == findpos(data, "DON004"), "the index still finds the other donations");
    check(store.count() == 3 && offerstatus(store, "OFF003") < 0 && offerstatus(store, "OFF004") < 0, "both offer rows of DON005 are removed");

    bool hasown = false;

    for (size_t pos = 0; pos < data.donations.size(); pos++)
    {
        hasown = hasown || (data.donations[pos].donorId() == EntityId("DN", 1) && data.donations[pos].id() == EntityId("DON", 5));
    }

    check(!hasown, "the donor's list no longer holds DON005");

    dataset fresh;
    offerstore freshstore;
    reload(folder, fresh, freshstore);
    check(!contains(readfile(folder + "donations.csv"), "DON005"), "donations.csv no longer holds DON005");
    check(fresh.donations.size() == 4 && files.checkinvariants(fresh).empty(), "a fresh load works and checkinvariants finds nothing");
    check(freshstore.reconcile(fresh).empty(), "no offer points to a missing donation after the delete");

    // deleting the highest id and then creating a donation still gives an id no other donation has
    int made = createdonation(fresh, files, EntityId("DN", 1), COOKED, 10, 30, message);
    bool unique = made >= 0;

    for (size_t pos = 0; pos < fresh.donations.size() && unique; pos++)
    {
        unique = ((int)pos == made) || !(fresh.donations[pos].id() == fresh.donations[made].id());
    }

    check(unique, "the next created donation id is unique");
}

// Runs every group of checks.
int main()
{
    testcanedit();
    testedit();
    testdelete();
    return finish();
}
