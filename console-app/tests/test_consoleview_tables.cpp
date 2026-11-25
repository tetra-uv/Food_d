#include "ConsoleView.h"
#include "Datamanager.h"
#include "EntityId.h"
#include "Models.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

int failcount = 0;

// RAII redirection of cout, so the original buffer is always restored.
struct CoutRedirect {
    streambuf* oldOut;
    CoutRedirect(ostream& outStream)
        : oldOut(cout.rdbuf(outStream.rdbuf())) {}
    ~CoutRedirect() {
        cout.rdbuf(oldOut);
    }
};

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

// Splits screen text into lines and drops the empty ones.
vector<string> screenlines(const string& text)
{
    vector<string> lines;
    istringstream in(text);
    string line;

    while (getline(in, line))
    {
        if (!line.empty() && line[line.length() - 1] == '\r')
        {
            line = line.substr(0, line.length() - 1);
        }

        if (!line.empty())
        {
            lines.push_back(line);
        }
    }

    return lines;
}

// Finds the start index of each label in the header line.
vector<size_t> columnstarts(const string& header, const vector<string>& labels)
{
    vector<size_t> starts;
    size_t from = 0;

    for (size_t pos = 0; pos < labels.size(); pos++)
    {
        size_t spot = header.find(labels[pos], from);
        starts.push_back(spot);
        from = spot + labels[pos].length();
    }

    return starts;
}

// The generic gap check. lines[0] is the header, lines[1] the dashed line, the rest is data.
// Every column start above 0 must have two spaces before it and a visible character at it,
// and the dashed line must be as long as the header line.
bool gapsok(const vector<string>& lines, const vector<string>& labels, string& problem)
{
    if (lines.size() < 3)
    {
        problem = "the table has no data lines";
        return false;
    }

    vector<size_t> starts = columnstarts(lines[0], labels);

    for (size_t pos = 0; pos < starts.size(); pos++)
    {
        if (starts[pos] == string::npos)
        {
            problem = "label not found: " + labels[pos];
            return false;
        }
    }

    if (lines[1].length() != lines[0].length())
    {
        problem = "the dashed line is not as long as the header line";
        return false;
    }

    for (size_t row = 2; row < lines.size(); row++)
    {
        for (size_t pos = 1; pos < starts.size(); pos++)
        {
            size_t at = starts[pos];

            if (lines[row].length() <= at || lines[row][at] == ' ' ||
                lines[row][at - 1] != ' ' || lines[row][at - 2] != ' ')
            {
                problem = "columns touch or move in line: " + lines[row];
                return false;
            }
        }
    }

    return true;
}

// Builds the in-memory dataset with long names and every kind of recipient.
dataset makedata()
{
    dataset data;
    vector<FoodType> some;
    some.push_back(COOKED);
    some.push_back(PACKAGED);

    vector<FoodType> all;
    all.push_back(COOKED);
    all.push_back(PACKAGED);
    all.push_back(PRODUCE);
    all.push_back(BAKERY);

    // A name of exactly 60 characters
    string sixty = "Sixty Character Community Relief And Food Support Centre 123";

    data.donors.push_back(Donor(EntityId("DN", 1),
                                "Hotel Rajdhani Kitchen And Banquet Services Preet Vihar",
                                "HOTEL", 0));
    data.donors.push_back(Donor(EntityId("DN", 2), "Sun", "CATERER", 0));
    data.donorindex.put("DN001", 0);
    data.donorindex.put("DN002", 1);

    data.recipients.push_back(Recipient(EntityId("R", 1), sixty, "SHELTER", 150, some, true,
                                        true, 0));
    data.recipients.push_back(Recipient(EntityId("R", 2), "Barakhamba Relief Society",
                                        "FOOD_BANK", 10000, all, true, true, 0));
    data.recipients.push_back(Recipient(EntityId("R", 3), "Closed Kitchen", "KITCHEN", 0,
                                        some, false, false, 0));
    data.recipientindex.put("R001", 0);
    data.recipientindex.put("R002", 1);
    data.recipientindex.put("R003", 2);

    data.donations.push_back(Donation(EntityId("DON", 1), EntityId("DN", 1), COOKED, 80, 0, 30,
                                      COMPLETED));
    data.donations.push_back(Donation(EntityId("DON", 2), EntityId("DN", 2), PACKAGED, 50, 0,
                                      40, COMPLETED));
    data.donationindex.put("DON001", 0);
    data.donationindex.put("DON002", 1);

    data.transactions.push_back(Transaction(EntityId("T", 1), EntityId("DON", 1),
                                            EntityId("R", 1), 80, 9));
    data.transactions.push_back(Transaction(EntityId("T", 2), EntityId("DON", 2),
                                            EntityId("R", 2), 50, 125));
    data.transactions.push_back(Transaction(EntityId("T", 3), EntityId("DON", 1),
                                            EntityId("R", 3), 5, 7));

    return data;
}

// Runs the checks for one table. show is the screen text, labels the header labels.
void checktable(const string& name, const string& show, const vector<string>& labels,
                const string& mustcontain)
{
    vector<string> lines = screenlines(show);
    string problem = "";

    bool gapsfine = gapsok(lines, labels, problem);

    check(gapsfine, name + ": no columns touch" + (gapsfine ? "" : " (" + problem + ")"));
    check(show.find(mustcontain) != string::npos,
          name + ": the long text is printed in full");
}

int main()
{
    cout << "--- test_consoleview_tables ---\n";

    dataset data = makedata();

    vector<string> recipientlabels;
    recipientlabels.push_back("ID");
    recipientlabels.push_back("Name");
    recipientlabels.push_back("Capacity");
    recipientlabels.push_back("Accepted types");
    recipientlabels.push_back("Accepts");
    recipientlabels.push_back("Pickup");

    ostringstream recipientout;
    {
        CoutRedirect guard(recipientout);
        ConsoleView::showRecipients(data);
    }

    checktable("T1 recipients", recipientout.str(), recipientlabels,
               "Sixty Character Community Relief And Food Support Centre 123");
    check(recipientout.str().find("COOKED;PACKAGED;PRODUCE;BAKERY") != string::npos &&
              recipientout.str().find("10000") != string::npos,
          "T2 recipients: four food types and capacity 10000 are shown");

    vector<string> rlines = screenlines(recipientout.str());
    check(rlines.size() == 5 && rlines[4].find("no") != string::npos,
          "T3 recipients: the closed recipient has its own line with no and no");

    vector<string> transactionlabels;
    transactionlabels.push_back("ID");
    transactionlabels.push_back("Donor");
    transactionlabels.push_back("Food");
    transactionlabels.push_back("Recipient");
    transactionlabels.push_back("Quantity");
    transactionlabels.push_back("Travel");

    ostringstream transactionout;
    {
        CoutRedirect guard(transactionout);
        ConsoleView::showTransactions(data);
    }

    checktable("T4 transactions", transactionout.str(), transactionlabels,
               "Hotel Rajdhani Kitchen And Banquet Services Preet Vihar");
    check(transactionout.str().find("Sixty Character Community Relief And Food Support Centre 123") !=
              string::npos,
          "T5 transactions: the 60 character recipient name is printed in full");

    // Shorter names must not leave a wide table behind: the width follows the longest cell.
    dataset small;
    small.donors.push_back(Donor(EntityId("DN", 1), "Sun", "CATERER", 0));
    small.donorindex.put("DN001", 0);
    small.recipients.push_back(Recipient(EntityId("R", 1), "Hope", "SHELTER", 5,
                                         vector<FoodType>(1, COOKED), true, true, 0));
    small.recipientindex.put("R001", 0);

    ostringstream smallout;
    {
        CoutRedirect guard(smallout);
        ConsoleView::showRecipients(small);
    }

    vector<string> slines = screenlines(smallout.str());
    check(slines.size() == 3 && slines[0].length() < rlines[0].length(),
          "T6 recipients: a table with short names is narrower than one with long names");

    cout << "Total FAIL: " << failcount << "\n";

    if (failcount > 0)
    {
        return 1;
    }

    return 0;
}
