// test_recipientfoods: a recipient changes the food types it takes, in memory, in recipients.csv and in the matching.
// Runs only in fresh folders below the build folder, never on real data.

#include "Datamanager.h"
#include "Exceptions.h"
#include "OfferStore.h"
#include "RecipientUpdate.h"
#include "Tables.h"
#include "TextTools.h"
#include "Workflow.h"
#include "testkit.h"
#include "traveltime.h"

#include <list>
#include <string>
#include <vector>

using namespace std;

// Returns every recipient row except the one with this id, as one text.
string otherrows(const dataset& data, const string& skip)
{
    string text = "";

    for (size_t pos = 0; pos < data.recipients.size(); pos++)
    {
        if (data.recipients[pos].id().toString() != skip)
        {
            text = text + data.recipients[pos].toCsvRow() + "\n";
        }
    }

    return text;
}

// The csv row of one recipient in memory, or an empty text.
string rowof(const dataset& data, const string& id)
{
    int pos = 0;
    return data.recipientindex.get(id, pos) ? data.recipients[pos].toCsvRow() : "";
}

// Loads a folder into a new data set, as a restart would.
dataset reload(const string& folder)
{
    datamanager files(folder);
    dataset data;
    list<string> warnings;
    files.loadall(data, warnings);
    return data;
}

// A list of food types.
vector<FoodType> typesof(FoodType first, FoodType second, FoodType third, FoodType fourth, int count)
{
    FoodType all[4] = {first, second, third, fourth};
    return vector<FoodType>(all, all + count);
}

// Tells whether a recipient is a candidate of the match, or rejected with the food reason.
void wherein(const matchview& match, const string& id, bool& candidate, bool& foodrejected)
{
    candidate = false;
    foodrejected = false;

    for (size_t at = 0; at < match.candidates.size(); at++)
    {
        candidate = candidate || match.candidates[at].recipientId().toString() == id;
    }

    for (size_t at = 0; at < match.rejected.size(); at++)
    {
        foodrejected = foodrejected || (match.rejected[at].recipientId().toString() == id && match.rejected[at].reason() == FOOD_INCOMPATIBLE);
    }
}

// Changing, refusing and saving the food types.
void testchange()
{
    string folder = testfolder("recipientfoods_change");
    writefixture(folder);
    datamanager files(folder);
    dataset data = reload(folder);
    string message = "";
    string others = otherrows(data, "R001");

    // only BAKERY
    check(updaterecipientdetails(data, files, EntityId("R", 1), 150, true, true, typesof(BAKERY, BAKERY, BAKERY, BAKERY, 1), message), "R001 takes only BAKERY");
    dataset fresh = reload(folder);
    check(rowof(fresh, "R001") == "R001,Hope Shelter,SHELTER,150,BAKERY,1,1,5", "recipients.csv holds only BAKERY: " + rowof(fresh, "R001"));
    check(otherrows(fresh, "R001") == others, "every other recipient row is unchanged");

    // all four, given out of order and with a duplicate, are stored once each in the fixed order
    check(updaterecipientdetails(data, files, EntityId("R", 1), 150, true, true, typesof(BAKERY, COOKED, PRODUCE, PACKAGED, 4), message), "R001 takes all four");
    vector<FoodType> twice = typesof(PRODUCE, COOKED, PRODUCE, COOKED, 4);
    check(updaterecipientdetails(data, files, EntityId("R", 1), 150, true, true, twice, message), "duplicates are accepted");
    fresh = reload(folder);
    check(rowof(fresh, "R001") == "R001,Hope Shelter,SHELTER,150,COOKED;PRODUCE,1,1,5", "duplicates are stored once in the fixed order: " + rowof(fresh, "R001"));
    check(updaterecipientdetails(data, files, EntityId("R", 1), 150, true, true, typesof(BAKERY, PRODUCE, PACKAGED, COOKED, 4), message), "all four again");
    fresh = reload(folder);
    check(rowof(fresh, "R001") == "R001,Hope Shelter,SHELTER,150,COOKED;PACKAGED;PRODUCE;BAKERY,1,1,5", "all four in the order COOKED PACKAGED PRODUCE BAKERY: " + rowof(fresh, "R001"));

    // refusals change nothing in memory or in the file
    string before = rowof(data, "R001");
    string filebefore = readfile(folder + "recipients.csv");
    check(!updaterecipientdetails(data, files, EntityId("R", 1), 150, true, true, vector<FoodType>(), message) && message == "Choose at least one food type", "an empty list is refused with the message");
    check(!haspunct(message), "the new message has no punctuation");
    check(rowof(data, "R001") == before && readfile(folder + "recipients.csv") == filebefore, "the empty list changed nothing");
    check(!updaterecipientdetails(data, files, EntityId("R", 99), 10, true, true, typesof(COOKED, COOKED, COOKED, COOKED, 1), message) && message == "That recipient could not be found", "an unknown recipient is refused");
    check(!updaterecipientdetails(data, files, EntityId("R", 1), -1, true, true, typesof(COOKED, COOKED, COOKED, COOKED, 1), message) && message == "The capacity cannot be below 0", "a negative capacity is refused");
    check(rowof(data, "R001") == before && readfile(folder + "recipients.csv") == filebefore, "the refusals changed nothing");

    // a read-only recipients.csv: the call fails and the old object is back in memory (the file is always made writable again)
    setreadonly(folder + "recipients.csv", true);
    bool saved = updaterecipientdetails(data, files, EntityId("R", 1), 150, true, true, typesof(COOKED, COOKED, COOKED, COOKED, 1), message);
    setreadonly(folder + "recipients.csv", false);
    remove((folder + "recipients.csv.tmp").c_str());
    check(!saved && !haspunct(message), "a read-only file makes the call fail with a plain message");
    check(rowof(data, "R001") == before, "the old recipient is back in memory after the failed save");
}

// Removing COOKED takes a recipient out of a COOKED match; putting it back makes it a candidate again.
void testmatching()
{
    string folder = testfolder("recipientfoods_match");
    writefixture(folder);
    datamanager files(folder);
    dataset data = reload(folder);
    offerstore store;
    dijkstratraveltime provider(data.network);
    string message = "";
    int pos = 0;
    data.donationindex.get("DON001", pos);
    bool candidate = false;
    bool foodrejected = false;

    wherein(buildmatch(data, store, provider, pos), "R003", candidate, foodrejected);
    check(candidate && !foodrejected, "R003 is a candidate for the COOKED donation DON001");

    check(updaterecipientdetails(data, files, EntityId("R", 3), 120, true, true, typesof(PACKAGED, PACKAGED, PACKAGED, PACKAGED, 1), message), "R003 stops taking COOKED");
    wherein(buildmatch(data, store, provider, pos), "R003", candidate, foodrejected);
    check(!candidate && foodrejected, "R003 is rejected because it does not take this kind of food");
    check(reasontext(FOOD_INCOMPATIBLE) == "Does not take this kind of food", "the reason text");

    check(updaterecipientdetails(data, files, EntityId("R", 3), 120, true, true, typesof(COOKED, COOKED, COOKED, COOKED, 1), message), "R003 takes COOKED again");
    wherein(buildmatch(data, store, provider, pos), "R003", candidate, foodrejected);
    check(candidate && !foodrejected, "R003 is a candidate again");
}

// Runs every group of checks.
int main()
{
    testchange();
    testmatching();
    return finish();
}
