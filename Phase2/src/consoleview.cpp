#include "consoleview.h"
#include "Exceptions.h"
#include <iomanip>
#include <sstream>

using namespace std;

// Reads a whole number made only of digits (at most nine, so it always fits an int).
static bool wholenumber(const string& text, int& value)
{
    if (text.empty() || text.length() > 9)
    {
        return false;
    }

    for (size_t pos = 0; pos < text.length(); pos++)
    {
        if (text[pos] < '0' || text[pos] > '9')
        {
            return false;
        }
    }

    stringstream stream(text);
    stream >> value;
    return true;
}

// Removes spaces, tabs and carriage returns at both ends of a line.
static string cleanup(const string& line)
{
    size_t first = 0;

    while (first < line.length() &&
           (line[first] == ' ' || line[first] == '\t' || line[first] == '\r'))
    {
        first++;
    }

    size_t last = line.length();

    while (last > first &&
           (line[last - 1] == ' ' || line[last - 1] == '\t' || line[last - 1] == '\r'))
    {
        last--;
    }

    return line.substr(first, last - first);
}

// Writes the nodes of a route with arrows between them, for example 0 -> 2 -> 5.
static string routetext(const vector<int>& route)
{
    stringstream text;

    for (size_t pos = 0; pos < route.size(); pos++)
    {
        if (pos > 0)
        {
            text << " -> ";
        }

        text << route[pos];
    }

    return text.str();
}

// Starts as an answer that was not understood.
coordinatorchoice::coordinatorchoice()
{
    valid_ = false;
    close_ = false;
    candidate_ = 0;
    quantity_ = 0;
}

// Checks whether the last line was understood.
bool coordinatorchoice::valid() const
{
    return valid_;
}

// Checks whether the coordinator asked to close the donation.
bool coordinatorchoice::isclose() const
{
    return close_;
}

// Returns the candidate number as typed.
int coordinatorchoice::candidatenumber() const
{
    return candidate_;
}

// Returns the quantity as typed.
int coordinatorchoice::quantity() const
{
    return quantity_;
}

// Accepts the line c, or exactly two whole numbers. Anything else is not valid.
istream& operator>>(istream& in, coordinatorchoice& choice)
{
    choice = coordinatorchoice();
    string line;

    if (!getline(in, line))
    {
        return in;
    }

    stringstream words(line);
    string first;
    string second;
    string extra;
    words >> first;

    if (first == "c" || first == "C")
    {
        if (!(words >> extra))
        {
            choice.close_ = true;
            choice.valid_ = true;
        }

        return in;
    }

    if (words >> second && !(words >> extra))
    {
        if (wholenumber(first, choice.candidate_) && wholenumber(second, choice.quantity_))
        {
            choice.valid_ = true;
        }
    }

    return in;
}

// Remembers the streams.
consoleview::consoleview(istream& in, ostream& out) : in_(in), out_(out)
{
}

// States the purpose, that the coordinator decides, and that the weights are simulated.
void consoleview::showabout()
{
    out_ << "\nAbout CommunityBridge\n";
    out_ << "CommunityBridge helps a coordinator get surplus food to a recipient\n";
    out_ << "organization. The tool only recommends. The coordinator makes every final\n";
    out_ << "decision: nothing is allocated or closed without an answer typed in.\n\n";
    out_ << "Feature: location-aware travel-time feasibility. A weighted graph and\n";
    out_ << "Dijkstra's algorithm check whether the food can arrive within its usable time.\n";
    out_ << "The travel times on the edges are SIMULATED minutes. They are not real road\n";
    out_ << "or traffic data.\n";
}

// Prints the menu.
void consoleview::showmenu()
{
    out_ << "\nCommunityBridge menu\n";
    out_ << "  1  Process the most urgent donation\n";
    out_ << "  2  Enter a new donation\n";
    out_ << "  3  Show donations\n";
    out_ << "  4  Show recipients\n";
    out_ << "  5  Show transaction log\n";
    out_ << "  6  About this tool\n";
    out_ << "  0  Exit\n";
}

// Keeps asking until the answer is a whole number from 0 to 6.
int consoleview::readmenu()
{
    string answer;
    int number = 0;

    while (ask("Choose a number: ", answer))
    {
        if (wholenumber(answer, number) && number <= 6)
        {
            return number;
        }

        out_ << "Please type a number from 0 to 6.\n";
    }

    return -1;
}

// Prints each warning on its own line.
void consoleview::showwarnings(const list<string>& warnings)
{
    if (warnings.empty())
    {
        return;
    }

    out_ << "\nWarnings found while loading:\n";

    for (list<string>::const_iterator spot = warnings.begin(); spot != warnings.end(); spot++)
    {
        out_ << "  " << *spot << "\n";
    }
}

// Prints one row per donation.
void consoleview::showdonations(const dataset& data)
{
    out_ << "\nDonations\n";
    out_ << "  " << left << setw(8) << "Id" << setw(8) << "Donor" << setw(10) << "Food"
         << right << setw(10) << "Original" << setw(11) << "Remaining" << setw(8)
         << "Usable" << "  " << left << "Status\n";

    for (size_t pos = 0; pos < data.donations.size(); pos++)
    {
        const Donation& donation = data.donations[pos];
        out_ << "  " << left << setw(8) << donation.id().toString() << setw(8)
             << donation.donorId().toString() << setw(10) << foodToStr(donation.foodType())
             << right << setw(10) << donation.originalQuantity() << setw(11)
             << donation.remainingQuantity() << setw(8) << donation.usableMinutes() << "  "
             << left << statusToStr(donation.status()) << "\n";
    }
}

// Prints one row per recipient.
void consoleview::showrecipients(const dataset& data)
{
    out_ << "\nRecipients\n";
    out_ << "  " << left << setw(8) << "Id" << setw(26) << "Name" << right << setw(10)
         << "Capacity" << setw(11) << "Accepting" << setw(8) << "Pickup" << setw(6)
         << "Node" << "\n";

    for (size_t pos = 0; pos < data.recipients.size(); pos++)
    {
        const Recipient& recipient = data.recipients[pos];
        string accepting = "no";
        string pickup = "no";

        if (recipient.accepting())
        {
            accepting = "yes";
        }

        if (recipient.canPickup())
        {
            pickup = "yes";
        }

        out_ << "  " << left << setw(8) << recipient.id().toString() << setw(26)
             << recipient.name() << right << setw(10) << recipient.availableCapacity()
             << setw(11) << accepting << setw(8) << pickup << setw(6) << recipient.nodeId()
             << "\n";
    }
}

// Prints one row per transaction.
void consoleview::showtransactions(const dataset& data)
{
    out_ << "\nTransaction log\n";

    if (data.transactions.empty())
    {
        out_ << "  No transactions yet.\n";
        return;
    }

    out_ << "  " << left << setw(8) << "Id" << setw(10) << "Donation" << setw(11)
         << "Recipient" << right << setw(10) << "Quantity" << setw(8) << "Travel" << "\n";

    for (size_t pos = 0; pos < data.transactions.size(); pos++)
    {
        const Transaction& entry = data.transactions[pos];
        out_ << "  " << left << setw(8) << entry.id().toString() << setw(10)
             << entry.donationId().toString() << setw(11) << entry.recipientId().toString()
             << right << setw(10) << entry.quantity() << setw(8) << entry.travelMinutes()
             << "\n";
    }
}

// The workflow of Spec 7.4 for one donation.
bool consoleview::processdonation(dataset& data, datamanager& files,
                                  const dijkstratraveltime& provider, int index)
{
    Donation& donation = data.donations[index];
    int donorpos = 0;

    if (!data.donorindex.get(donation.donorId().toString(), donorpos))
    {
        throw StructureException("The donor of a donation is missing from the index");
    }

    // The travel times are computed once and reused every time the donation is evaluated
    int source = data.donors[donorpos].nodeId();
    vector<Duration> times = provider.traveltimesfrom(source);

    while (donation.status() == PENDING || donation.status() == PARTIAL)
    {
        recommendationresult found = recommend(donation, data.recipients, times);
        showdonation(data, donation, source);
        showrecommendation(data, found, provider, source);

        if (found.candidates.empty())
        {
            closedonation(donation);
            files.commitdonations(data);
            out_ << "\nNo recipient can take this donation. It is closed and the leftover "
                 << donation.remainingQuantity() << " is recorded as wasted.\n";
            return true;
        }

        int count = (int)found.candidates.size();
        coordinatorchoice choice;
        bool decided = false;

        while (!decided)
        {
            if (!readchoice(choice))
            {
                out_ << "\nThe input ended. The donation stays open.\n";
                return false;
            }

            if (choice.isclose())
            {
                decided = true;
            }
            else if (choice.candidatenumber() < 1 || choice.candidatenumber() > count)
            {
                out_ << "There is no candidate with that number.\n";
            }
            else if (choice.quantity() < 1 ||
                     choice.quantity() >
                         found.candidates[choice.candidatenumber() - 1].potentialacceptance())
            {
                out_ << "The quantity must be between 1 and "
                     << found.candidates[choice.candidatenumber() - 1].potentialacceptance()
                     << " for that candidate.\n";
            }
            else
            {
                decided = true;
            }
        }

        if (choice.isclose())
        {
            closedonation(donation);
            files.commitdonations(data);
            out_ << "\nThe donation is closed. The leftover " << donation.remainingQuantity()
                 << " is recorded as wasted.\n";
            return true;
        }

        const candidate& chosen = found.candidates[choice.candidatenumber() - 1];
        int recipientpos = 0;
        data.recipientindex.get(chosen.recipientid().toString(), recipientpos);

        allocationresult outcome =
            allocate(donation, data.recipients[recipientpos], choice.quantity(),
                     chosen.traveltime(), transactionidissuer::issuenext());

        if (outcome.error != ALLOC_OK)
        {
            out_ << allocerrormessage(outcome.error) << ".\n";
        }
        else
        {
            data.transactions.push_back(outcome.transaction);
            data.lasttransaction = outcome.transaction.id();
            files.commitallocation(data, outcome.transaction);

            out_ << "\nAllocated " << choice.quantity() << " to "
                 << chosen.recipientid().toString() << " (transaction "
                 << outcome.transaction.id().toString() << ").\n";
        }
    }

    out_ << "\nThe donation " << donation.id().toString() << " is completed.\n";
    return true;
}

// Reads the donor, food type, quantity and usable minutes, each until it is valid.
int consoleview::adddonation(dataset& data, datamanager& files)
{
    out_ << "\nEnter a new donation (type q at any question to cancel)\n";
    out_ << "Donors:\n";

    for (size_t pos = 0; pos < data.donors.size(); pos++)
    {
        out_ << "  " << data.donors[pos].id().toString() << "  " << data.donors[pos].name()
             << "\n";
    }

    string answer;
    EntityId donor;
    bool known = false;

    while (!known)
    {
        if (!ask("Donor id: ", answer) || answer == "q")
        {
            return -1;
        }

        try
        {
            donor = EntityId::parse(answer, "DN");
            int unused = 0;

            if (data.donorindex.get(donor.toString(), unused))
            {
                known = true;
            }
            else
            {
                out_ << "There is no donor with that id.\n";
            }
        }
        catch (const FormatException&)
        {
            out_ << "A donor id looks like DN001.\n";
        }
    }

    FoodType food = COOKED;
    bool foodok = false;

    while (!foodok)
    {
        if (!ask("Food type (COOKED, PACKAGED, PRODUCE, BAKERY): ", answer) || answer == "q")
        {
            return -1;
        }

        try
        {
            food = strToFood(answer);
            foodok = true;
        }
        catch (const FormatException&)
        {
            out_ << "That is not one of the four food types.\n";
        }
    }

    int quantity = 0;
    bool quantityok = false;

    while (!quantityok)
    {
        if (!ask("Quantity in portions (at least 1): ", answer) || answer == "q")
        {
            return -1;
        }

        if (wholenumber(answer, quantity) && quantity >= 1)
        {
            quantityok = true;
        }
        else
        {
            out_ << "Please type a whole number of at least 1.\n";
        }
    }

    int usable = 0;
    bool usableok = false;

    while (!usableok)
    {
        if (!ask("Usable minutes (at least 1): ", answer) || answer == "q")
        {
            return -1;
        }

        if (wholenumber(answer, usable) && usable >= 1)
        {
            usableok = true;
        }
        else
        {
            out_ << "Please type a whole number of at least 1.\n";
        }
    }

    // The next number is one more than the highest donation number so far
    EntityId next("DON", 0);

    for (size_t pos = 0; pos < data.donations.size(); pos++)
    {
        if (next < data.donations[pos].id())
        {
            next = data.donations[pos].id();
        }
    }

    ++next;

    int index = (int)data.donations.size();
    data.donations.push_back(Donation(next, donor, food, quantity, quantity, usable, PENDING));
    data.donationindex.put(next.toString(), index);
    files.commitdonations(data);

    out_ << "The donation " << next.toString() << " was added.\n";
    return index;
}

// Prints the prompt, then reads one line and trims it.
bool consoleview::ask(const string& prompt, string& answer)
{
    out_ << prompt;
    string line;

    if (!getline(in_, line))
    {
        return false;
    }

    answer = cleanup(line);
    return true;
}

// Repeats the question until the answer is c or two whole numbers.
bool consoleview::readchoice(coordinatorchoice& choice)
{
    out_ << "\nType a candidate number and a quantity (for example 1 40), or c to close: ";

    while (in_ >> choice)
    {
        if (choice.valid())
        {
            return true;
        }

        out_ << "Please type two whole numbers such as 1 40, or c. Try again: ";
    }

    return false;
}

// Prints what is being processed and how much is left.
void consoleview::showdonation(const dataset& data, const Donation& donation, int source)
{
    int donorpos = 0;
    string donorname = "";

    if (data.donorindex.get(donation.donorId().toString(), donorpos))
    {
        donorname = data.donors[donorpos].name();
    }

    out_ << "\nDonation " << donation.id().toString() << " from " << donorname << " (node "
         << source << "): " << foodToStr(donation.foodType()) << ", "
         << donation.remainingQuantity() << " of " << donation.originalQuantity()
         << " left, usable " << donation.usableMinutes() << " minutes\n";
}

// Prints the ranked table with routes and then the rejected recipients with their reason.
void consoleview::showrecommendation(const dataset& data, const recommendationresult& found,
                                     const dijkstratraveltime& provider, int source)
{
    out_ << "Ranked candidates (best first):\n";

    if (found.candidates.empty())
    {
        out_ << "  none\n";
    }
    else
    {
        out_ << "  " << left << setw(3) << "#" << setw(10) << "Recipient" << setw(26)
             << "Name" << right << setw(6) << "Takes" << setw(8) << "Travel" << setw(8)
             << "Unused" << "  " << left << "Route\n";
    }

    for (size_t pos = 0; pos < found.candidates.size(); pos++)
    {
        const candidate& entry = found.candidates[pos];
        int recipientpos = 0;
        data.recipientindex.get(entry.recipientid().toString(), recipientpos);
        const Recipient& recipient = data.recipients[recipientpos];

        out_ << "  " << left << setw(3) << (pos + 1) << setw(10)
             << entry.recipientid().toString() << setw(26) << recipient.name() << right
             << setw(6) << entry.potentialacceptance() << setw(8) << entry.traveltime()
             << setw(8) << entry.unusedcapacity() << "  " << left
             << routetext(provider.routeto(source, recipient.nodeId())) << "\n";
    }

    out_ << "Rejected recipients:\n";

    if (found.rejections.empty())
    {
        out_ << "  none\n";
    }

    for (size_t pos = 0; pos < found.rejections.size(); pos++)
    {
        const rejection& entry = found.rejections[pos];
        int recipientpos = 0;
        data.recipientindex.get(entry.recipientid().toString(), recipientpos);

        out_ << "  " << left << setw(10) << entry.recipientid().toString() << setw(26)
             << data.recipients[recipientpos].name() << reasonToStr(entry.reason());

        if (entry.reason() == TOO_FAR)
        {
            out_ << " (" << entry.traveltime() << " min)";
        }

        out_ << "\n";
    }
}