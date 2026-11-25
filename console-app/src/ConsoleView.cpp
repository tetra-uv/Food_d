#include "ConsoleView.h"

#include <cctype>
#include <iomanip>
#include <sstream>

using namespace std;

CoordinatorChoice::CoordinatorChoice()
    : valid_(false), close_(false), candidateNumber_(0), quantity_(0)
{
}

istream& operator>>(istream& in, CoordinatorChoice& choice)
{
    choice.valid_ = false;
    choice.close_ = false;
    choice.candidateNumber_ = 0;
    choice.quantity_ = 0;

    string line;
    if (!getline(in, line))
    {
        return in;
    }

    size_t start = 0;
    while (start < line.length() && (line[start] == ' ' || line[start] == '\t' || line[start] == '\r' || line[start] == '\n'))
    {
        start++;
    }
    if (start >= line.length())
    {
        choice.valid_ = false;
        return in;
    }
    size_t end = line.length() - 1;
    while (end > start && (line[end] == ' ' || line[end] == '\t' || line[end] == '\r' || line[end] == '\n'))
    {
        end--;
    }
    string trimmed = line.substr(start, end - start + 1);

    if (trimmed == "c" || trimmed == "C")
    {
        choice.valid_ = true;
        choice.close_ = true;
        return in;
    }

    stringstream ss(trimmed);
    int cand = 0;
    int qty = 0;
    if (ss >> cand >> qty)
    {
        string extra;
        if (!(ss >> extra) && cand >= 1 && qty >= 1)
        {
            choice.valid_ = true;
            choice.close_ = false;
            choice.candidateNumber_ = cand;
            choice.quantity_ = qty;
            return in;
        }
    }

    choice.valid_ = false;
    return in;
}

void ConsoleView::showCandidates(const vector<Candidate>& candidates)
{
    cout << endl;
    cout << "Ranked candidates" << endl;

    cout << left << setw(8) << "Rank" << setw(12) << "Recipient" << setw(12) << "Accept"
         << setw(12) << "Travel" << setw(12) << "Unused" << endl;

    cout << "--------------------------------------------------------" << endl;

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
    {
        const Candidate& candidate = candidates[i];

        cout << left << setw(8) << i + 1 << setw(12) << candidate.recipientId()
             << setw(12) << candidate.potentialAcceptance() << setw(12)
             << candidate.travel() << setw(12) << candidate.unusedCapacity() << endl;
    }
}

void ConsoleView::showRejected(const vector<Rejection>& rejected)
{
    if (rejected.empty())
    {
        return;
    }

    cout << endl;
    cout << "Rejected recipients" << endl;

    cout << left << setw(12) << "Recipient" << setw(24) << "Reason" << setw(12)
         << "Travel" << endl;

    cout << "--------------------------------------------------------" << endl;

    for (int i = 0; i < static_cast<int>(rejected.size()); ++i)
    {
        const Rejection& rejection = rejected[i];

        string reasonText;

        switch (rejection.reason())
        {
        case NOT_ACCEPTING:
            reasonText = "NOT_ACCEPTING";
            break;
        case FOOD_INCOMPATIBLE:
            reasonText = "FOOD_INCOMPATIBLE";
            break;
        case NO_CAPACITY:
            reasonText = "NO_CAPACITY";
            break;
        case NO_PICKUP:
            reasonText = "NO_PICKUP";
            break;
        case UNREACHABLE:
            reasonText = "UNREACHABLE";
            break;
        case TOO_FAR:
            reasonText = "TOO_FAR (" + to_string(rejection.travel().minutes()) + " min)";
            break;
        default:
            reasonText = "OK";
            break;
        }

        cout << "  " << left << setw(10) << rejection.recipientId() << setw(24) << reasonText
             << setw(12) << rejection.travel() << endl;
    }
    cout << endl;
}

void ConsoleView::showNoCandidates()
{
    cout << endl;
    cout << "No feasible recipient is available (none). Any leftover is recorded as wasted." << endl;
}

void ConsoleView::showAllocationError(AllocError error)
{
    switch (error)
    {
    case ALLOC_OK:
        cout << "Allocation successful." << endl;
        break;
    case DONATION_NOT_OPEN:
        cout << "Error: donation is not open for allocation." << endl;
        break;
    case BAD_QUANTITY:
        cout << "Error: quantity must be at least 1." << endl;
        break;
    case EXCEEDS_REMAINING:
        cout << "Error: quantity exceeds the donation's remaining amount." << endl;
        break;
    case EXCEEDS_CAPACITY:
        cout << "Error: quantity exceeds recipient capacity." << endl;
        break;
    case NOT_FEASIBLE:
        cout << "Error: recipient is no longer feasible." << endl;
        break;
    default:
        cout << "Error: unknown allocation error." << endl;
        break;
    }
}

void ConsoleView::showDonationClosed()
{
    cout << "The donation is closed. The remaining food is recorded as wasted." << endl;
}

void ConsoleView::showAllocationSuccess(const EntityId& recipientId, int quantity)
{
    cout << "Allocated " << quantity << " to " << recipientId << "." << endl;
}

static string formatRoute(const vector<int>& path)
{
    stringstream text;

    for (int i = 0; i < static_cast<int>(path.size()); ++i)
    {
        if (i > 0)
        {
            text << " -> ";
        }

        text << path[i];
    }

    return text.str();
}

static string joinTypes(const vector<FoodType>& types)
{
    stringstream text;

    for (int i = 0; i < static_cast<int>(types.size()); ++i)
    {
        if (i > 0)
        {
            text << ";";
        }

        text << foodToStr(types[i]);
    }

    return text.str();
}

void ConsoleView::showCandidates(const vector<Candidate>& candidates,
                                 const vector<string>& routes)
{
    cout << endl;
    cout << "Ranked candidates" << endl;

    cout << left << setw(8) << "Rank" << setw(12) << "Recipient" << setw(12) << "Accept"
         << setw(12) << "Travel" << setw(12) << "Unused" << setw(20) << "Route" << endl;

    cout << "----------------------------------------------------------------------------"
            "----"
         << endl;

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i)
    {
        const Candidate& candidate = candidates[i];
        string route = (i < static_cast<int>(routes.size())) ? routes[i] : "";

        cout << left << setw(8) << i + 1 << setw(12) << candidate.recipientId()
             << setw(12) << candidate.potentialAcceptance() << setw(12)
             << candidate.travel() << setw(12) << candidate.unusedCapacity() << setw(20)
             << route << endl;
    }
}

void ConsoleView::showWarnings(const list<string>& warnings)
{
    for (list<string>::const_iterator it = warnings.begin(); it != warnings.end(); ++it)
    {
        cout << "Warning: " << *it << endl;
    }
}

void ConsoleView::showMenu()
{
    cout << endl;
    cout << "1. Process the most urgent donation" << endl;
    cout << "2. Add a donation" << endl;
    cout << "3. Add a recipient" << endl;
    cout << "4. Show donations" << endl;
    cout << "5. Show recipients" << endl;
    cout << "6. Show transactions" << endl;
    cout << "7. Show road network" << endl;
    cout << "8. Show travel times from a donor" << endl;
    cout << "9. Show summary" << endl;
    cout << "0. Exit" << endl;
}

int ConsoleView::readMenu()
{
    const int maxChoice = 9;
    while (true)
    {
        cout << "Choice: ";

        string line;

        if (!getline(cin, line))
        {
            return -1;
        }

        stringstream ss(line);
        int choice;

        if (!(ss >> choice))
        {
            cout << "Please type a number from 0 to " << maxChoice << "." << endl;
            continue;
        }

        string extra;

        if (ss >> extra)
        {
            cout << "Please type a number from 0 to " << maxChoice << "." << endl;
            continue;
        }

        if (choice < 0 || choice > maxChoice)
        {
            cout << "Please type a number from 0 to " << maxChoice << "." << endl;
            continue;
        }

        return choice;
    }
}

bool ConsoleView::processDonation(dataset& data, datamanager& files,
                                  const dijkstratraveltime& provider, int index)
{
    Donation& donation = data.donations[index];

    int donorPos = 0;

    if (!data.donorindex.get(donation.donorId().toString(), donorPos))
    {
        cout << "The donor for this donation is not in the files." << endl;
        return true;
    }

    int donorNode = data.donors[donorPos].nodeId();

    vector<Duration> travelTimes = provider.traveltimesfrom(donorNode);

    cout << endl;
    cout << "Processing " << donation.id() << endl;
    cout << "Donor: " << data.donors[donorPos].name() << " (" << donation.donorId()
        << ")   "
        << "Food: " << foodToStr(donation.foodType()) << "   "
        << "Quantity: " << donation.remainingQuantity() << "   "
        << "Usable: " << donation.usableMinutes() << " min" << endl;

    while (donation.remainingQuantity() > 0)
    {
        RecommendationResult result = recommend(donation, data.recipients, travelTimes);

        if (result.candidates().empty())
        {
            showNoCandidates();
            showRejected(result.rejected());
            donation.close();
            files.commitdonations(data);
            showDonationClosed();
            return true;
        }

        vector<string> routes;

        for (int i = 0; i < static_cast<int>(result.candidates().size()); ++i)
        {
            int recipientPos = 0;
            string route = "";

            if (data.recipientindex.get(result.candidates()[i].recipientId().toString(),
                                        recipientPos))
            {
                int targetNode = data.recipients[recipientPos].nodeId();
                route = formatRoute(provider.routeto(donorNode, targetNode));
            }

            routes.push_back(route);
        }

        showCandidates(result.candidates(), routes);
        showRejected(result.rejected());

        CoordinatorChoice choice;

        while (true)
        {
            cout << "Pick a candidate number and a quantity, or c to close: ";

            if (!(cin >> choice))
            {
                cout << "The input ended." << endl;
                return false;
            }

            if (choice.isclose())
            {
                donation.close();
                files.commitdonations(data);
                showDonationClosed();
                return true;
            }

            if (!choice.valid())
            {
                cout << "Please type two whole numbers." << endl;
                continue;
            }

            if (choice.candidateNumber() < 1 ||
                choice.candidateNumber() > static_cast<int>(result.candidates().size()))
            {
                cout << "There is no candidate with that number." << endl;
                continue;
            }

            const Candidate& picked = result.candidates()[choice.candidateNumber() - 1];

            if (choice.quantity() < 1 || choice.quantity() > picked.potentialAcceptance())
            {
                cout << "The quantity must be between 1 and " << picked.potentialAcceptance() << "." << endl;
                continue;
            }

            int recipientPos = 0;

            if (!data.recipientindex.get(picked.recipientId().toString(), recipientPos))
            {
                cout << "That recipient is no longer in the files." << endl;
                continue;
            }

            Recipient& recipient = data.recipients[recipientPos];
            EntityId transactionId = transactionidissuer::issuenext();

            AllocError outcome = allocate(donation, recipient, choice.quantity(),
                                          picked.travel(), transactionId);

            if (outcome != ALLOC_OK)
            {
                showAllocationError(outcome);
                continue;
            }

            Transaction entry(transactionId, donation.id(), recipient.id(), choice.quantity(),
                              picked.travel().minutes());
            data.transactions.push_back(entry);
            files.commitallocation(data, entry);
            showAllocationSuccess(recipient.id(), choice.quantity());
            break;
        }
    }

    return true;
}

// Prints the prompt and reads one line without the spaces at both ends. Returns false when
// the input has ended.
static bool askLine(const string& prompt, string& answer)
{
    cout << prompt;

    string line;

    if (!getline(cin, line))
    {
        return false;
    }

    size_t first = 0;

    while (first < line.length() &&
           (line[first] == ' ' || line[first] == '\t' || line[first] == '\r' || line[first] == '\n'))
    {
        first++;
    }

    size_t last = line.length();

    while (last > first &&
           (line[last - 1] == ' ' || line[last - 1] == '\t' || line[last - 1] == '\r' ||
            line[last - 1] == '\n'))
    {
        last--;
    }

    answer = line.substr(first, last - first);
    return true;
}

// Checks whether an answer is the word that cancels a question flow.
static bool isQuit(const string& answer)
{
    return answer == "q" || answer == "Q";
}

// Returns the text in capital letters.
static string toCapitals(const string& text)
{
    string result = text;

    for (size_t i = 0; i < result.length(); ++i)
    {
        result[i] = static_cast<char>(toupper(static_cast<unsigned char>(result[i])));
    }

    return result;
}

// Reads a whole number that is at least minimum. A wrong answer gets the message and the
// question again. Returns false when the coordinator types q or the input ends.
static bool askWholeNumber(const string& prompt, int minimum, const string& message, int& value)
{
    while (true)
    {
        string answer;

        if (!askLine(prompt, answer) || isQuit(answer))
        {
            return false;
        }

        stringstream ss(answer);
        int number = 0;
        string extra;

        if ((ss >> number) && !(ss >> extra) && number >= minimum)
        {
            value = number;
            return true;
        }

        cout << message << endl;
    }
}

// These helpers are written further down in this file, next to the recipient questions.
static bool askName(const string& prompt, string& name);
static bool askType(const string& title, const vector<string>& known, string& type);
static bool askLocation(const dataset& data, int& node);

// What the coordinator answered at the donor question.
enum DonorAnswer
{
    DONOR_PICKED, // an existing donor was chosen
    DONOR_NEW,    // a new donor must be created
    DONOR_CANCEL  // q was typed or the input ended
};

// Asks which donor offers the food. The answer is a list number, a donor id, n for a new donor
// or q to cancel. When there are no donors only n and q are offered.
static DonorAnswer askDonor(const dataset& data, EntityId& donorId)
{
    const int count = static_cast<int>(data.donors.size());
    string prompt = "Choose a donor: type n for a new donor, or q to cancel: ";

    if (count > 0)
    {
        prompt = "Choose a donor: type the list number (1-" + to_string(count) +
                 "), a donor id such as DN001, n for a new donor, or q to cancel: ";
    }

    while (true)
    {
        string answer;

        if (!askLine(prompt, answer) || isQuit(answer))
        {
            return DONOR_CANCEL;
        }

        if (answer == "n" || answer == "N")
        {
            return DONOR_NEW;
        }

        if (count == 0)
        {
            cout << "Please type n for a new donor, or q to cancel." << endl;
            continue;
        }

        stringstream ss(answer);
        int number = 0;
        string extra;

        if ((ss >> number) && !(ss >> extra))
        {
            if (number >= 1 && number <= count)
            {
                donorId = data.donors[number - 1].id();
                return DONOR_PICKED;
            }

            cout << "There is no donor with that number. Please type a number from 1 to "
                 << count << "." << endl;
            continue;
        }

        try
        {
            EntityId typed = EntityId::parse(toCapitals(answer), "DN");

            if (answer.length() < 5)
            {
                cout << "That is not a donor id. A donor id looks like DN001. Please try again."
                     << endl;
                continue;
            }

            int donorPos = 0;

            if (!data.donorindex.get(typed.toString(), donorPos))
            {
                cout << "There is no donor with that id." << endl;
                continue;
            }

            donorId = typed;
            return DONOR_PICKED;
        }
        catch (const FormatException&)
        {
            cout << "That is not a donor id. A donor id looks like DN001. Please try again."
                 << endl;
        }
    }
}

// Asks the questions for a new donor: name, type and map location. Returns false when the
// coordinator types q or the input ends.
static bool askNewDonor(const dataset& data, string& name, string& type, int& node)
{
    cout << endl;
    cout << "New donor" << endl;

    if (!askName("Donor name: ", name))
    {
        return false;
    }

    vector<string> knownTypes;

    for (size_t i = 0; i < data.donors.size(); ++i)
    {
        bool seen = false;

        for (size_t k = 0; k < knownTypes.size(); ++k)
        {
            if (knownTypes[k] == data.donors[i].type())
            {
                seen = true;
            }
        }

        if (!seen)
        {
            knownTypes.push_back(data.donors[i].type());
        }
    }

    if (!askType("Donor type", knownTypes, type))
    {
        return false;
    }

    return askLocation(data, node);
}

// Checks whether some recipient is in the same connected part of the map as the location.
static bool recipientInSamePart(const dataset& data, int node)
{
    vector<int> label;
    labelcomponents(data.network, label);

    for (size_t i = 0; i < data.recipients.size(); ++i)
    {
        if (label[data.recipients[i].nodeId()] == label[node])
        {
            return true;
        }
    }

    return false;
}

// Asks for the food type. The answer is 1 to 4 or the name in any letter case. Returns false
// when the coordinator types q or the input ends.
static bool askFoodType(FoodType& food)
{
    while (true)
    {
        string answer;

        if (!askLine("Food type (1 COOKED, 2 PACKAGED, 3 PRODUCE, 4 BAKERY): ", answer) ||
            isQuit(answer))
        {
            return false;
        }

        string capitals = toCapitals(answer);

        if (capitals == "1" || capitals == "COOKED")
        {
            food = COOKED;
            return true;
        }

        if (capitals == "2" || capitals == "PACKAGED")
        {
            food = PACKAGED;
            return true;
        }

        if (capitals == "3" || capitals == "PRODUCE")
        {
            food = PRODUCE;
            return true;
        }

        if (capitals == "4" || capitals == "BAKERY")
        {
            food = BAKERY;
            return true;
        }

        cout << "That is not one of the four food types. Please try again." << endl;
    }
}

int ConsoleView::addDonation(dataset& data, datamanager& files)
{
    cout << endl;
    cout << "Add a donation (type q at any question to cancel)" << endl;

    if (data.donors.empty())
    {
        cout << endl << "There are no donors yet." << endl;
    }
    else
    {
        cout << endl;
        cout << "Donors:" << endl;

        const int numberWidth = static_cast<int>(to_string(data.donors.size()).length());

        for (int i = 0; i < static_cast<int>(data.donors.size()); ++i)
        {
            cout << "  " << right << setw(numberWidth) << i + 1 << ". " << data.donors[i].id()
                 << "  " << data.donors[i].name() << endl;
        }
    }

    cout << endl;

    EntityId donorId;
    DonorAnswer who = askDonor(data, donorId);

    if (who == DONOR_CANCEL)
    {
        return -1;
    }

    // the details of a new donor are asked now, but the donor is saved only at the end
    string newDonorName;
    string newDonorType;
    int newDonorNode = 0;

    if (who == DONOR_NEW)
    {
        if (!askNewDonor(data, newDonorName, newDonorType, newDonorNode))
        {
            return -1;
        }

        cout << endl;
    }

    FoodType foodType = COOKED;

    if (!askFoodType(foodType))
    {
        return -1;
    }

    int originalQuantity = 0;

    if (!askWholeNumber("Quantity (whole number, 1 or more): ", 1,
                        "Please type a whole number of at least 1.", originalQuantity))
    {
        return -1;
    }

    int usableMinutes = 0;

    if (!askWholeNumber("Minutes the food stays usable (whole number, 1 or more): ", 1,
                        "Please type a whole number of at least 1.", usableMinutes))
    {
        return -1;
    }

    // ids are issued only now, so cancelling never uses up a number
    if (who == DONOR_NEW)
    {
        EntityId maxDonorId("DN", 0);
        for (size_t i = 0; i < data.donors.size(); ++i)
        {
            if (maxDonorId < data.donors[i].id())
            {
                maxDonorId = data.donors[i].id();
            }
        }
        donoridissuer::seed(maxDonorId);
        donorId = donoridissuer::issuenext();
    }

    EntityId maxDonationId("DON", 0);
    for (size_t i = 0; i < data.donations.size(); ++i)
    {
        if (maxDonationId < data.donations[i].id())
        {
            maxDonationId = data.donations[i].id();
        }
    }
    donationidissuer::seed(maxDonationId);
    EntityId newId = donationidissuer::issuenext();
    Donation newDonation(newId, donorId, foodType, originalQuantity, originalQuantity, usableMinutes, PENDING);

    // the donor is saved first: a donation whose donor is missing from donors.csv stops the
    // program at the next start
    if (who == DONOR_NEW)
    {
        Donor newDonor(donorId, newDonorName, newDonorType, newDonorNode);
        files.appenddonor(data, newDonor);
        data.donorindex.put(donorId.toString(), static_cast<int>(data.donors.size()));
        data.donors.push_back(newDonor);
    }

    files.appenddonation(data, newDonation);

    int newIndex = static_cast<int>(data.donations.size());
    data.donations.push_back(newDonation);
    data.donationindex.put(newId.toString(), newIndex);

    int donorPos = 0;
    data.donorindex.get(donorId.toString(), donorPos);

    cout << endl;

    if (who == DONOR_NEW)
    {
        cout << "Donor " << donorId << " and donation " << newId << " created and saved."
             << endl;
    }
    else
    {
        cout << "Donation " << newId << " created and saved." << endl;
    }

    cout << "  Donor: " << donorId << " " << data.donors[donorPos].name() << endl;
    cout << "  Food: " << foodToStr(foodType) << "   Quantity: " << originalQuantity
         << "   Usable: " << usableMinutes << " min" << endl;

    if (who == DONOR_NEW && !recipientInSamePart(data, newDonorNode))
    {
        cout << "Note: no recipient is connected to this location, so donations from this donor "
             << "will have no feasible recipient." << endl;
    }

    return newIndex;
}

// Reads the name of a donor or recipient: 1 to 60 characters and no comma. Returns false when
// the coordinator types q or the input ends.
static bool askName(const string& prompt, string& name)
{
    while (true)
    {
        string answer;

        if (!askLine(prompt, answer) || isQuit(answer))
        {
            return false;
        }

        if (!answer.empty() && answer.length() <= 60 && answer.find(',') == string::npos)
        {
            name = answer;
            return true;
        }

        cout << "The name must be 1 to 60 characters and cannot contain a comma." << endl;
    }
}

// Checks a new type: 1 to 30 letters, digits, spaces or underscores.
static bool validTypeText(const string& text)
{
    if (text.empty() || text.length() > 30)
    {
        return false;
    }

    for (size_t i = 0; i < text.length(); ++i)
    {
        unsigned char letter = static_cast<unsigned char>(text[i]);

        if (!isalnum(letter) && letter != ' ' && letter != '_')
        {
            return false;
        }
    }

    return true;
}

// Reads the type of a donor or recipient. The answer is a number from the list of types that
// are already in use, or a new type. A new type is stored in capitals with the spaces turned
// into underscores. Returns false when the coordinator types q or the input ends.
static bool askType(const string& title, const vector<string>& known, string& type)
{
    cout << title;

    if (known.empty())
    {
        cout << " (type a new type):" << endl;
    }
    else
    {
        cout << " (type a number, or type a new type):" << endl;
    }

    for (size_t i = 0; i < known.size(); ++i)
    {
        cout << "  " << i + 1 << ". " << known[i] << endl;
    }

    const int count = static_cast<int>(known.size());

    while (true)
    {
        string answer;

        if (!askLine("Type: ", answer) || isQuit(answer))
        {
            return false;
        }

        stringstream ss(answer);
        int number = 0;
        string extra;

        if (count > 0 && (ss >> number) && !(ss >> extra))
        {
            if (number >= 1 && number <= count)
            {
                type = known[number - 1];
                return true;
            }

            cout << "Please type a number from 1 to " << count << ", or a new type." << endl;
            continue;
        }

        if (validTypeText(answer))
        {
            type = toCapitals(answer);

            for (size_t i = 0; i < type.length(); ++i)
            {
                if (type[i] == ' ')
                {
                    type[i] = '_';
                }
            }

            return true;
        }

        cout << "Use letters, digits, spaces or underscores only (up to 30 characters)." << endl;
    }
}

// Reads the food types a recipient accepts: numbers 1 to 4 separated by spaces. A number typed
// twice counts once. Returns false when the coordinator types q or the input ends.
static bool askFoodTypes(vector<FoodType>& types)
{
    cout << "Food types it accepts (numbers separated by spaces, for example 1 3):" << endl;
    cout << "  1. COOKED  2. PACKAGED  3. PRODUCE  4. BAKERY" << endl;

    while (true)
    {
        string answer;

        if (!askLine("Accepts: ", answer) || isQuit(answer))
        {
            return false;
        }

        stringstream ss(answer);
        string word;
        bool chosen[4] = {false, false, false, false};
        bool allGood = true;
        int count = 0;

        while (ss >> word)
        {
            if (word.length() == 1 && word[0] >= '1' && word[0] <= '4')
            {
                chosen[word[0] - '1'] = true;
                count++;
            }
            else
            {
                allGood = false;
            }
        }

        if (allGood && count > 0)
        {
            types.clear();

            for (int i = 0; i < 4; ++i)
            {
                if (chosen[i])
                {
                    types.push_back(static_cast<FoodType>(i));
                }
            }

            return true;
        }

        cout << "Type one or more numbers from 1 to 4, separated by spaces, for example 1 3."
             << endl;
    }
}

// Reads a y or n answer. Returns false when the coordinator types q or the input ends.
static bool askYesNo(const string& prompt, bool& value)
{
    while (true)
    {
        string answer;

        if (!askLine(prompt, answer) || isQuit(answer))
        {
            return false;
        }

        string capitals = toCapitals(answer);

        if (capitals == "Y" || capitals == "YES")
        {
            value = true;
            return true;
        }

        if (capitals == "N" || capitals == "NO")
        {
            value = false;
            return true;
        }

        cout << "Please type y or n." << endl;
    }
}

// Shows the numbered list of map locations and reads the chosen number. Returns false when the
// map has no locations, the coordinator types q, or the input ends.
static bool askLocation(const dataset& data, int& node)
{
    const int count = static_cast<int>(data.nodelabels.size());

    if (count == 0)
    {
        cout << "The map has no locations, so a location cannot be chosen." << endl;
        return false;
    }

    cout << "Locations on the map:" << endl;

    const int numberWidth = static_cast<int>(to_string(count - 1).length());

    for (int i = 0; i < count; ++i)
    {
        cout << "  " << right << setw(numberWidth) << i << "  " << data.nodelabels[i] << endl;
    }

    string prompt = "Location number (0-" + to_string(count - 1) + "): ";

    while (true)
    {
        string answer;

        if (!askLine(prompt, answer) || isQuit(answer))
        {
            return false;
        }

        stringstream ss(answer);
        int number = 0;
        string extra;

        if ((ss >> number) && !(ss >> extra) && number >= 0 && number < count)
        {
            node = number;
            return true;
        }

        cout << "Please type a location number from 0 to " << count - 1 << "." << endl;
    }
}

// Checks whether some donor is in the same connected part of the map as the location.
static bool donorInSamePart(const dataset& data, int node)
{
    vector<int> label;
    labelcomponents(data.network, label);

    for (size_t i = 0; i < data.donors.size(); ++i)
    {
        if (label[data.donors[i].nodeId()] == label[node])
        {
            return true;
        }
    }

    return false;
}

// Asks every question first and saves only at the end, so typing q at any question cancels
// the whole flow and nothing is saved or used up.
void ConsoleView::addRecipient(dataset& data, datamanager& files)
{
    cout << endl;
    cout << "Add a recipient (type q at any question to cancel)" << endl;
    cout << endl;

    string name;

    if (!askName("Recipient name: ", name))
    {
        return;
    }

    vector<string> knownTypes;

    for (size_t i = 0; i < data.recipients.size(); ++i)
    {
        bool seen = false;

        for (size_t k = 0; k < knownTypes.size(); ++k)
        {
            if (knownTypes[k] == data.recipients[i].type())
            {
                seen = true;
            }
        }

        if (!seen)
        {
            knownTypes.push_back(data.recipients[i].type());
        }
    }

    string type;

    if (!askType("Recipient type", knownTypes, type))
    {
        return;
    }

    vector<FoodType> accepted;

    if (!askFoodTypes(accepted))
    {
        return;
    }

    int capacity = 0;

    if (!askWholeNumber("Available capacity (whole number, 0 or more): ", 0,
                        "Please type a whole number of 0 or more.", capacity))
    {
        return;
    }

    bool accepting = false;

    if (!askYesNo("Is it accepting donations now? (y/n): ", accepting))
    {
        return;
    }

    bool pickup = false;

    if (!askYesNo("Can it pick up food itself? (y/n): ", pickup))
    {
        return;
    }

    int node = 0;

    if (!askLocation(data, node))
    {
        return;
    }

    // the id is issued only now, so cancelling never uses up a number
    EntityId highestRecipient("R", 0);
    for (size_t i = 0; i < data.recipients.size(); ++i)
    {
        if (highestRecipient < data.recipients[i].id())
        {
            highestRecipient = data.recipients[i].id();
        }
    }
    recipientidissuer::seed(highestRecipient);
    EntityId newId = recipientidissuer::issuenext();
    Recipient recipient(newId, name, type, capacity, accepted, accepting, pickup, node);

    files.appendrecipient(data, recipient);

    int newIndex = static_cast<int>(data.recipients.size());
    data.recipients.push_back(recipient);
    data.recipientindex.put(newId.toString(), newIndex);

    cout << endl;
    cout << "Recipient " << newId << " created and saved." << endl;

    if (!donorInSamePart(data, node))
    {
        cout << "Note: " << newId << " is at a location that is not connected to any donor, "
             << "so it cannot be recommended until a donor is placed in the same part of the map."
             << endl;
    }
}

void ConsoleView::showDonations(const dataset& data)
{
    cout << endl;
    cout << left << setw(10) << "ID" << setw(10) << "Donor" << setw(12) << "Food"
         << setw(12) << "Original" << setw(12) << "Remaining" << setw(12) << "Usable"
         << setw(10) << "Status" << endl;
    cout << "----------------------------------------------------------------------------"
            "----"
         << endl;

    for (int i = 0; i < static_cast<int>(data.donations.size()); ++i)
    {
        const Donation& donation = data.donations[i];

        cout << left << setw(10) << donation.id() << setw(10) << donation.donorId()
             << setw(12) << foodToStr(donation.foodType()) << setw(12)
             << donation.originalQuantity() << setw(12) << donation.remainingQuantity()
             << setw(12) << donation.usableMinutes() << setw(10)
             << statusToStr(donation.status()) << endl;
    }
}

// Prints a table. Every column is as wide as its longest cell (the header counts too) plus
// 2 spaces, and the dashed line is as long as all the columns together.
static void printTable(const vector<string>& header, const vector<vector<string> >& rows)
{
    vector<size_t> widths;

    for (size_t col = 0; col < header.size(); ++col)
    {
        size_t longest = header[col].length();

        for (size_t row = 0; row < rows.size(); ++row)
        {
            if (rows[row][col].length() > longest)
            {
                longest = rows[row][col].length();
            }
        }

        widths.push_back(longest + 2);
    }

    size_t total = 0;

    for (size_t col = 0; col < widths.size(); ++col)
    {
        cout << left << setw(static_cast<int>(widths[col])) << header[col];
        total += widths[col];
    }

    cout << endl;
    cout << string(total, '-') << endl;

    for (size_t row = 0; row < rows.size(); ++row)
    {
        for (size_t col = 0; col < widths.size(); ++col)
        {
            cout << left << setw(static_cast<int>(widths[col])) << rows[row][col];
        }

        cout << endl;
    }
}

void ConsoleView::showRecipients(const dataset& data)
{
    cout << endl;

    vector<string> header;
    header.push_back("ID");
    header.push_back("Name");
    header.push_back("Capacity");
    header.push_back("Accepted types");
    header.push_back("Accepts");
    header.push_back("Pickup");

    vector<vector<string> > rows;

    for (int i = 0; i < static_cast<int>(data.recipients.size()); ++i)
    {
        const Recipient& recipient = data.recipients[i];

        vector<string> cells;
        cells.push_back(recipient.id().toString());
        cells.push_back(recipient.name());
        cells.push_back(to_string(recipient.availableCapacity()));
        cells.push_back(joinTypes(recipient.acceptedTypes()));
        cells.push_back(recipient.accepting() ? "yes" : "no");
        cells.push_back(recipient.canPickup() ? "yes" : "no");
        rows.push_back(cells);
    }

    printTable(header, rows);
}

void ConsoleView::showTransactions(const dataset& data)
{
    if (data.transactions.empty())
    {
        cout << endl << "No transactions yet." << endl;
        return;
    }

    cout << endl;

    vector<string> header;
    header.push_back("ID");
    header.push_back("Donor");
    header.push_back("Food");
    header.push_back("Recipient");
    header.push_back("Quantity");
    header.push_back("Travel");

    vector<vector<string> > rows;

    for (int i = 0; i < static_cast<int>(data.transactions.size()); ++i)
    {
        const Transaction& entry = data.transactions[i];

        string donorName = entry.donationId().toString();
        string foodText = "UNKNOWN";
        string recipientName = entry.recipientId().toString();

        int donationPos = 0;

        if (data.donationindex.get(entry.donationId().toString(), donationPos))
        {
            const Donation& donation = data.donations[donationPos];
            foodText = foodToStr(donation.foodType());

            int donorPos = 0;

            if (data.donorindex.get(donation.donorId().toString(), donorPos))
            {
                donorName = data.donors[donorPos].name();
            }
            else
            {
                donorName = donation.donorId().toString();
            }
        }

        int recipientPos = 0;

        if (data.recipientindex.get(entry.recipientId().toString(), recipientPos))
        {
            recipientName = data.recipients[recipientPos].name();
        }

        vector<string> cells;
        cells.push_back(entry.id().toString());
        cells.push_back(donorName);
        cells.push_back(foodText);
        cells.push_back(recipientName);
        cells.push_back(to_string(entry.quantity()));
        cells.push_back(to_string(entry.travelMinutes()));
        rows.push_back(cells);
    }

    printTable(header, rows);
}

void ConsoleView::showNetwork(const dataset& data)
{
    cout << endl;
    cout << "Locations:" << endl;
    for (int i = 0; i < static_cast<int>(data.nodelabels.size()); ++i)
    {
        cout << i << ": " << data.nodelabels[i] << endl;
    }

    cout << endl;
    cout << "Roads:" << endl;
    for (int u = 0; u < data.network.nodecount(); ++u)
    {
        for (const edgenode* edge = data.network.neighbors(u); edge != NULL; edge = edge->next)
        {
            if (u < edge->to)
            {
                cout << u << " to " << edge->to << ": " << edge->minutes << " min" << endl;
            }
        }
    }
}

void ConsoleView::showTravelTimesFromDonor(const dataset& data, const dijkstratraveltime& provider)
{
    if (data.donors.empty())
    {
        cout << endl << "No donors available." << endl;
        return;
    }

    cout << endl;
    cout << "Select a donor:" << endl;
    for (int i = 0; i < static_cast<int>(data.donors.size()); ++i)
    {
        cout << i + 1 << ". " << data.donors[i].name() << " (" << data.donors[i].id() << ")" << endl;
    }

    int donorChoice = -1;
    while (true)
    {
        cout << "Donor (1-" << data.donors.size() << "): ";
        string line;
        if (!getline(cin, line))
        {
            return;
        }

        stringstream ss(line);
        int num;
        string extra;
        if (!(ss >> num) || (ss >> extra) || num < 1 || num > static_cast<int>(data.donors.size()))
        {
            cout << "Please enter a number from 1 to " << data.donors.size() << "." << endl;
            continue;
        }

        donorChoice = num - 1;
        break;
    }

    const Donor& donor = data.donors[donorChoice];
    vector<Duration> times = provider.traveltimesfrom(donor.nodeId());

    cout << endl;
    cout << "Travel times from " << donor.name() << " (Location " << donor.nodeId() << "):" << endl;
    for (int i = 0; i < static_cast<int>(data.nodelabels.size()); ++i)
    {
        cout << i << ": " << data.nodelabels[i] << " - ";
        if (i < static_cast<int>(times.size()) && !times[i].isInfinite())
        {
            cout << times[i].minutes() << " min" << endl;
        }
        else
        {
            cout << "unreachable" << endl;
        }
    }
}

void ConsoleView::showSummary(const dataset& data)
{
    int totalOriginal = 0;
    int totalWasted = 0;
    int totalPending = 0;

    for (size_t i = 0; i < data.donations.size(); ++i)
    {
        const Donation& donation = data.donations[i];
        totalOriginal += donation.originalQuantity();

        if (donation.status() == CLOSED)
        {
            totalWasted += donation.remainingQuantity();
        }
        else if (donation.status() == PENDING || donation.status() == PARTIAL)
        {
            totalPending += donation.remainingQuantity();
        }
    }

    int totalAllocated = 0;
    int cookedAllocated = 0;
    int packagedAllocated = 0;
    int produceAllocated = 0;
    int bakeryAllocated = 0;

    for (size_t i = 0; i < data.transactions.size(); ++i)
    {
        const Transaction& txn = data.transactions[i];
        int qty = txn.quantity();
        totalAllocated += qty;

        int donationPos = 0;
        if (data.donationindex.get(txn.donationId().toString(), donationPos))
        {
            FoodType type = data.donations[donationPos].foodType();
            switch (type)
            {
                case COOKED:
                    cookedAllocated += qty;
                    break;
                case PACKAGED:
                    packagedAllocated += qty;
                    break;
                case PRODUCE:
                    produceAllocated += qty;
                    break;
                case BAKERY:
                    bakeryAllocated += qty;
                    break;
            }
        }
    }

    cout << endl;
    cout << "CommunityBridge Summary Report" << endl;
    cout << "----------------------------------------" << endl;
    cout << "Total original quantity:          " << totalOriginal << endl;
    cout << "Total quantity allocated:         " << totalAllocated << endl;
    cout << "Total quantity wasted:            " << totalWasted << endl;
    cout << "Total remaining open:             " << totalPending << endl;
    cout << "Internal verification sum:        " << (totalAllocated + totalWasted + totalPending)
         << " (must equal " << totalOriginal << ")" << endl;
    cout << endl;
    cout << "Allocated quantity by food type:" << endl;
    cout << "  COOKED:   " << cookedAllocated << endl;
    cout << "  PACKAGED: " << packagedAllocated << endl;
    cout << "  PRODUCE:  " << produceAllocated << endl;
    cout << "  BAKERY:   " << bakeryAllocated << endl;
    cout << endl;
    cout << "About" << endl;
    cout << "-----" << endl;
    cout << "CommunityBridge: location-aware travel-time feasibility recommendation system."
         << endl;
    cout << "All travel times between locations are SIMULATED minutes, not real road data."
         << endl;
    cout << "Donations are ranked by acceptance amount, travel time, and leftover "
            "capacity."
         << endl;
}
