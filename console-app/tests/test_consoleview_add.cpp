#include "ConsoleView.h"
#include "Datamanager.h"
#include "EntityId.h"
#include "Exceptions.h"
#include "Models.h"
#include "allocationmanager.h"
#include "csv_fixture.h"
#include <cstdlib>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

int failcount = 0;

// RAII stream redirection guards ensuring original stream buffers are restored.
struct StreamRedirect {
    streambuf* oldIn;
    streambuf* oldOut;
    StreamRedirect(istream& inStream, ostream& outStream)
        : oldIn(cin.rdbuf(inStream.rdbuf())),
          oldOut(cout.rdbuf(outStream.rdbuf())) {}
    ~StreamRedirect() {
        cin.rdbuf(oldIn);
        cout.rdbuf(oldOut);
    }
};

// All files of these tests start with this prefix, inside the tests/fixtures folder.
const string PREFIX = "console-app/tests/fixtures/ca_";

// Checks a condition and prints PASS or FAIL.
void check(bool condition, const string& name)
{
    if (condition)
    {
        cout << "PASS " << name << "\n";
    }
    else
    {
        cout << "FAIL " << name << "\n";
        failcount++;
    }
}

// Checks whether text contains needle.
bool contains(const string& text, const string& needle)
{
    return text.find(needle) != string::npos;
}

// Counts how many times needle appears in text.
int occurrences(const string& text, const string& needle)
{
    int total = 0;
    size_t spot = text.find(needle);

    while (spot != string::npos)
    {
        total++;
        spot = text.find(needle, spot + needle.length());
    }

    return total;
}

// Starts every test with a fresh copy of the standard fixture files. The map gets one extra
// location (number 11) that no road reaches, so no donor and no recipient is connected to it.
void fresh()
{
    removefixture(PREFIX);
    writefixture(PREFIX);
    writefile(PREFIX + "graph_nodes.csv", nodestext() + "11,Island Outpost,12,12\n");
}

// Loads the files again, as a restart would.
dataset reload()
{
    datamanager files(PREFIX);
    dataset data;
    list<string> warnings;
    files.loadall(data, warnings);
    return data;
}

// Runs addDonation with the scripted answers and returns its result.
int rundonation(dataset& data, datamanager& files, const string& script, string& shown)
{
    istringstream in(script);
    ostringstream out;
    int result = 0;

    {
        StreamRedirect guard(in, out);
        result = ConsoleView::addDonation(data, files);
    }

    shown = out.str();
    return result;
}

// Runs addRecipient with the scripted answers.
void runrecipient(dataset& data, datamanager& files, const string& script, string& shown)
{
    istringstream in(script);
    ostringstream out;

    {
        StreamRedirect guard(in, out);
        ConsoleView::addRecipient(data, files);
    }

    shown = out.str();
}

// Reads the three tables an add can change, so two states can be compared.
string tables()
{
    return readfile(PREFIX + "donors.csv") + "|" + readfile(PREFIX + "recipients.csv") + "|" +
           readfile(PREFIX + "donations.csv");
}

// Turns the slashes of a path into back slashes for the Windows attrib command.
string winpath(const string& path)
{
    string changed = path;

    for (size_t pos = 0; pos < changed.length(); pos++)
    {
        if (changed[pos] == '/')
        {
            changed[pos] = '\\';
        }
    }

    return changed;
}

int main()
{
    cout << "--- test_consoleview_add ---\n";

    string shown;

    // A1: cancelling after the donor details saves nothing and uses up no id
    fresh();
    datamanager files(PREFIX);
    dataset data = reload();
    string before = tables();
    int result = rundonation(data, files, "n\nSunrise Caterers\n1\n6\nq\n", shown);

    check(result == -1 && tables() == before && data.donors.size() == 2 &&
              data.donations.size() == 2,
          "A1: q after the new donor details saves nothing and changes nothing in memory");
    check(contains(shown, "New donor") && contains(shown, "Donor name:"),
          "A2: answer n starts the new donor questions");

    // A3: the next real add gets the very next ids and is saved
    result = rundonation(data, files, "n\nSunrise Caterers\n1\n6\n1\n40\n30\n", shown);
    dataset saved = reload();

    check(result == 2 && contains(shown, "Donor DN003 and donation DON003 created and saved."),
          "A3: the next new donor is DN003 and its donation DON003 (no id was burned)");
    check(saved.donors.size() == 3 && saved.donors[2].toCsvRow() == "DN003,Sunrise Caterers,CATERER,6",
          "A4: the new donor is in donors.csv with the chosen type and location");
    check(saved.donations.size() == 3 &&
              saved.donations[2].toCsvRow() == "DON003,DN003,COOKED,40,40,30,PENDING",
          "A5: the donation is in donations.csv and points to the new donor");
    check(data.donors.size() == 3 && data.donations.size() == 3 &&
              data.donations[result].donorId() == EntityId("DN", 3),
          "A6: the dataset in memory has the new donor and the new donation");

    int position = -1;
    check(data.donorindex.get("DN003", position) && position == 2 &&
              data.donationindex.get("DON003", position) && position == 2,
          "A7: both indexes find the new records");
    check(!contains(shown, "Note:"), "A8: a donor on the connected map gets no warning note");

    // B: bad answers are refused with a message and asked again
    fresh();
    data = reload();
    result = rundonation(data, files,
                         "DON001\nDN009\n9\nn\nSun, rise\n"
                         "ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJX\n"
                         "Sunrise Caterers\nbad!type\n9\n1\n99\n-1\n6\n"
                         "soup\n3\nabc\n0\n5\nx\n0\n20\n",
                         shown);

    check(result == 2 && data.donors.size() == 3 && data.donations[2].foodType() == PRODUCE,
          "B1: after many wrong answers the right ones still add the donor and the donation");
    check(contains(shown, "That is not a donor id") && contains(shown, "There is no donor with that id") &&
              contains(shown, "There is no donor with that number"),
          "B2: wrong donor answers get their own messages");
    check(occurrences(shown, "The name must be 1 to 60 characters and cannot contain a comma.") == 2,
          "B3: a name with a comma and a name of 61 characters are refused");
    check(contains(shown, "Use letters, digits, spaces or underscores only") &&
              contains(shown, "Please type a number from 1 to 2, or a new type.") &&
              occurrences(shown, "Please type a location number from 0 to 11.") == 2,
          "B4: wrong type and location answers get their own messages");
    check(contains(shown, "not one of the four food types") &&
              occurrences(shown, "Please type a whole number of at least 1.") == 4,
          "B5: wrong food type and number answers are asked again");

    // C: a new type is stored in capitals with underscores
    fresh();
    data = reload();
    result = rundonation(data, files, "n\nGreen Fork\nnight hotel\n2\n1\n10\n10\n", shown);
    check(result == 2 && reload().donors[2].type() == "NIGHT_HOTEL",
          "C1: a typed new donor type is saved in capitals with underscores");

    // D: the Note when nobody is connected to the new donor's location
    fresh();
    data = reload();
    rundonation(data, files, "n\nIsland Kitchen\n1\n11\n1\n10\n10\n", shown);
    check(contains(shown, "Note: no recipient is connected to this location"),
          "D1: a donor on the island gets the no-recipient note");

    // E: the donor is saved before the donation. With donations.csv read-only the donation
    // save fails, and the donor must already be in donors.csv.
    fresh();
    data = reload();
    string donationsfile = winpath(PREFIX + "donations.csv");
    system(("attrib +R " + donationsfile).c_str());

    bool refused = false;

    try
    {
        rundonation(data, files, "n\nSunrise Caterers\n1\n6\n1\n40\n30\n", shown);
    }
    catch (const DataException&)
    {
        refused = true;
    }

    system(("attrib -R " + donationsfile).c_str());
    saved = reload();

    check(refused, "E1: the donation save was refused (donations.csv was read-only)");
    check(saved.donors.size() == 3 && saved.donations.size() == 2,
          "E2: the new donor was written before the donation (donor saved, donation not)");

    // F: no donors at all, only n and q are offered
    fresh();
    writefile(PREFIX + "donors.csv", "donor_id,name,type,node_id\n");
    writefile(PREFIX + "donations.csv", "donation_id,donor_id,food_type,original_quantity,"
                                        "remaining_quantity,usable_minutes,status\n");
    data = reload();
    result = rundonation(data, files, "5\nq\n", shown);

    check(result == -1 && contains(shown, "There are no donors yet.") &&
              contains(shown, "type n for a new donor, or q to cancel") &&
              !contains(shown, "list number") &&
              contains(shown, "Please type n for a new donor, or q to cancel."),
          "F1: with no donors only n and q are offered");

    // G: an existing donor is still chosen by list number
    fresh();
    data = reload();
    result = rundonation(data, files, "2\n3\n10\n20\n", shown);
    check(result == 2 && data.donations[2].donorId() == EntityId("DN", 2) &&
              contains(shown, "Donation DON003 created and saved.") &&
              !contains(shown, "New donor"),
          "G1: list number 2 gives donor DN002 and a plain donation message");

    // H: adding a recipient
    fresh();
    data = reload();
    before = tables();
    runrecipient(data, files, "Temp Pantry\n1\n3\nq\n", shown);
    check(tables() == before && data.recipients.size() == 7,
          "H1: q in the middle of a recipient saves nothing");

    runrecipient(data, files, "Hope Community Kitchen\n2\n1 2 2\n120\ny\nn\n7\n", shown);
    saved = reload();
    check(contains(shown, "Recipient R008 created and saved.") && saved.recipients.size() == 8 &&
              saved.recipients[7].toCsvRow() ==
                  "R008,Hope Community Kitchen,FOOD_BANK,120,COOKED;PACKAGED,1,0,7",
          "H2: the next recipient is R008 and every answer is saved");
    check(data.recipients.size() == 8 && data.recipientindex.get("R008", position) && position == 7,
          "H3: the dataset in memory and its index have the new recipient");
    check(!contains(shown, "Note:"), "H4: a recipient on the connected map gets no note");

    runrecipient(data, files, "Island Pantry\n1\n1\n10\nn\nn\n11\n", shown);
    check(contains(shown, "Recipient R009 created and saved.") &&
              contains(shown, "Note: R009 is at a location that is not connected to any donor"),
          "H5: a recipient on the island gets the not-connected note");

    runrecipient(data, files,
                 "Bad, Name\nOk Name\n1\n5\n0\na\n1\n-1\nabc\n5\nmaybe\ny\nn\n99\n-1\n3\n", shown);
    check(occurrences(shown, "The name must be 1 to 60 characters") == 1 &&
              occurrences(shown, "Type one or more numbers from 1 to 4") == 3 &&
              occurrences(shown, "Please type a whole number of 0 or more.") == 2 &&
              contains(shown, "Please type y or n.") &&
              occurrences(shown, "Please type a location number from 0 to 11.") == 2,
          "H6: every wrong recipient answer gets its own message");

    removefixture(PREFIX);

    cout << "Total FAIL: " << failcount << "\n";

    if (failcount > 0)
    {
        return 1;
    }

    return 0;
}
