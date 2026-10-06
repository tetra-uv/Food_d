#include "datamanager.h"
#include "Exceptions.h"
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace std;

// The header row that every table must start with.
static const string DONORS_HEADER = "donor_id,name,type,node_id";
static const string RECIPIENTS_HEADER =
    "recipient_id,name,type,available_capacity,accepted_types,accepting,can_pickup,node_id";
static const string DONATIONS_HEADER =
    "donation_id,donor_id,food_type,original_quantity,remaining_quantity,usable_minutes,"
    "status";
static const string TRANSACTIONS_HEADER =
    "transaction_id,donation_id,recipient_id,quantity,travel_minutes";
static const string NODES_HEADER = "node_id,label,x_km,y_km";
static const string EDGES_HEADER = "u,v,minutes";

// One data row of a table with its line number in the file and its fields.
struct csvrow
{
    int number;
    vector<string> fields;
};

// One road read from graph_edges.csv before the graph is built.
struct edgerow
{
    int from;
    int to;
    int minutes;
};

// Checks for a character that is removed around values.
static bool isspacechar(char letter)
{
    return letter == ' ' || letter == '\t' || letter == '\r' || letter == '\n';
}

// Returns the text without spaces, tabs and carriage returns at both ends.
static string trim(const string& text)
{
    size_t first = 0;

    while (first < text.length() && isspacechar(text[first]))
    {
        first++;
    }

    size_t last = text.length();

    while (last > first && isspacechar(text[last - 1]))
    {
        last--;
    }

    return text.substr(first, last - first);
}

// Splits a line at the separator and trims every part.
static void splitfields(const string& line, char separator, vector<string>& parts)
{
    string current = "";

    for (size_t pos = 0; pos < line.length(); pos++)
    {
        if (line[pos] == separator)
        {
            parts.push_back(trim(current));
            current = "";
        }
        else
        {
            current = current + line[pos];
        }
    }

    parts.push_back(trim(current));
}

// Builds the start of an error message such as "donors.csv line 3: ".
static string where(const string& name, int line)
{
    stringstream text;
    text << name << " line " << line << ": ";
    return text.str();
}

// Checks whether a file can be opened for reading.
static bool fileexists(const string& path)
{
    ifstream file(path.c_str());
    return file.is_open();
}

// Reads a whole number. Throws FormatException if the text is not one.
static int parseint(const string& text, const string& what)
{
    size_t start = 0;

    if (!text.empty() && text[0] == '-')
    {
        start = 1;
    }

    if (start >= text.length())
    {
        throw FormatException("Missing whole number for " + what);
    }

    for (size_t pos = start; pos < text.length(); pos++)
    {
        if (text[pos] < '0' || text[pos] > '9')
        {
            throw FormatException("Not a whole number for " + what + ": " + text);
        }
    }

    int value = 0;
    stringstream stream(text);
    stream >> value;

    if (stream.fail())
    {
        throw FormatException("Number is too large for " + what + ": " + text);
    }

    return value;
}

// Reads a decimal number. Throws FormatException if the text is not one.
static double parsedouble(const string& text, const string& what)
{
    double value = 0;
    stringstream stream(text);
    stream >> value;

    if (stream.fail() || !stream.eof())
    {
        throw FormatException("Not a number for " + what + ": " + text);
    }

    return value;
}

// Reads 0 or 1. Throws FormatException for anything else.
static bool parsebool(const string& text, const string& what)
{
    if (text == "0")
    {
        return false;
    }

    if (text == "1")
    {
        return true;
    }

    throw FormatException("Expected 0 or 1 for " + what + ": " + text);
}

// Reads an id with the given prefix and at least three digits (rule A15).
static EntityId parseid(const string& text, const string& prefix)
{
    EntityId identity = EntityId::parse(text, prefix);

    if (text.length() - prefix.length() < 3)
    {
        throw FormatException("An id needs at least three digits: " + text);
    }

    return identity;
}

// Reads a file, checks the header row and splits every data row into fields.
static void readtable(const string& path, const string& name, const string& header,
                      int columns, vector<csvrow>& rows)
{
    ifstream file(path.c_str());

    if (!file.is_open())
    {
        throw DataException("Cannot open file: " + path);
    }

    string line;
    int number = 0;

    while (getline(file, line))
    {
        number++;

        // A UTF-8 byte order mark can sit in front of the first line
        if (number == 1 && line.length() >= 3 && (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
        {
            line = line.substr(3);
        }

        string clean = trim(line);

        if (number == 1)
        {
            if (clean != header)
            {
                throw DataException(where(name, 1) + "unexpected header, expected: " +
                                    header);
            }
        }
        else if (!clean.empty())
        {
            csvrow row;
            row.number = number;
            splitfields(clean, ',', row.fields);

            if ((int)row.fields.size() != columns)
            {
                stringstream message;
                message << where(name, number) << "expected " << columns
                        << " fields but found " << row.fields.size();
                throw DataException(message.str());
            }

            rows.push_back(row);
        }
    }

    if (number == 0)
    {
        throw DataException(name + ": the file is empty, the header row is missing");
    }
}

// Checks rule I4 and V6: the status must match the quantities.
static bool statusmatches(DonationStatus status, int original, int remaining)
{
    if (status == PENDING)
    {
        return remaining == original;
    }

    if (status == PARTIAL)
    {
        return remaining > 0 && remaining < original;
    }

    if (status == COMPLETED)
    {
        return remaining == 0;
    }

    return true;
}

// Reads graph_nodes.csv. Node ids must be 0, 1, 2, ... in order (rule V7).
static void loadnodes(const string& path, const string& name, dataset& target)
{
    vector<csvrow> rows;
    readtable(path, name, NODES_HEADER, 4, rows);

    for (size_t pos = 0; pos < rows.size(); pos++)
    {
        const csvrow& row = rows[pos];

        try
        {
            int node = parseint(row.fields[0], "node_id");

            if (node != (int)pos)
            {
                throw DataException(where(name, row.number) +
                                    "node ids must be 0, 1, 2, ... in order (V7)");
            }

            target.nodelabels.push_back(row.fields[1]);
            target.nodexs.push_back(parsedouble(row.fields[2], "x_km"));
            target.nodeys.push_back(parsedouble(row.fields[3], "y_km"));
        }
        catch (const FormatException& problem)
        {
            throw DataException(where(name, row.number) + problem.what());
        }
    }

    target.network = graph((int)rows.size());
}

// Reads graph_edges.csv and builds the graph. A duplicate edge keeps the smaller minutes
// and adds a warning (rule V7).
static void loadedges(const string& path, const string& name, dataset& target,
                      list<string>& warnings)
{
    vector<csvrow> rows;
    readtable(path, name, EDGES_HEADER, 3, rows);

    int count = target.network.nodecount();
    vector<edgerow> edges;
    hashindex seen;

    for (size_t pos = 0; pos < rows.size(); pos++)
    {
        const csvrow& row = rows[pos];

        try
        {
            edgerow edge;
            edge.from = parseint(row.fields[0], "u");
            edge.to = parseint(row.fields[1], "v");
            edge.minutes = parseint(row.fields[2], "minutes");

            if (edge.from < 0 || edge.from >= count || edge.to < 0 || edge.to >= count)
            {
                throw DataException(where(name, row.number) +
                                    "an edge ends at a node that does not exist (V7)");
            }

            if (edge.from == edge.to)
            {
                throw DataException(where(name, row.number) +
                                    "an edge from a node to itself is not allowed (V7)");
            }

            if (edge.minutes < 1)
            {
                throw DataException(where(name, row.number) +
                                    "an edge needs at least 1 minute (V7)");
            }

            int small = edge.from;
            int large = edge.to;

            if (large < small)
            {
                small = edge.to;
                large = edge.from;
            }

            stringstream key;
            key << small << "," << large;
            int earlier = 0;

            if (seen.get(key.str(), earlier))
            {
                stringstream message;
                message << where(name, row.number) << "duplicate edge " << small << "-"
                        << large << ", the smaller minutes are kept";
                warnings.push_back(message.str());

                if (edge.minutes < edges[earlier].minutes)
                {
                    edges[earlier].minutes = edge.minutes;
                }
            }
            else
            {
                seen.put(key.str(), (int)edges.size());
                edges.push_back(edge);
            }
        }
        catch (const FormatException& problem)
        {
            throw DataException(where(name, row.number) + problem.what());
        }
    }

    graph built(count);

    for (size_t pos = 0; pos < edges.size(); pos++)
    {
        built.addedge(edges[pos].from, edges[pos].to, edges[pos].minutes);
    }

    target.network = built;
}

// Reads donors.csv.
static void loaddonors(const string& path, const string& name, dataset& target)
{
    vector<csvrow> rows;
    readtable(path, name, DONORS_HEADER, 4, rows);

    for (size_t pos = 0; pos < rows.size(); pos++)
    {
        const csvrow& row = rows[pos];

        try
        {
            EntityId identity = parseid(row.fields[0], "DN");
            int node = parseint(row.fields[3], "node_id");

            if (node < 0 || node >= target.network.nodecount())
            {
                throw DataException(where(name, row.number) +
                                    "the node does not exist in the graph (V2)");
            }

            if (!target.donorindex.put(identity.toString(), (int)target.donors.size()))
            {
                throw DataException(where(name, row.number) + "duplicate id " +
                                    identity.toString() + " (V1)");
            }

            target.donors.push_back(Donor(identity, row.fields[1], row.fields[2], node));
        }
        catch (const FormatException& problem)
        {
            throw DataException(where(name, row.number) + problem.what());
        }
    }
}

// Reads recipients.csv.
static void loadrecipients(const string& path, const string& name, dataset& target)
{
    vector<csvrow> rows;
    readtable(path, name, RECIPIENTS_HEADER, 8, rows);

    for (size_t pos = 0; pos < rows.size(); pos++)
    {
        const csvrow& row = rows[pos];

        try
        {
            EntityId identity = parseid(row.fields[0], "R");
            int capacity = parseint(row.fields[3], "available_capacity");

            vector<string> words;
            splitfields(row.fields[4], ';', words);
            vector<FoodType> types;

            for (size_t kind = 0; kind < words.size(); kind++)
            {
                types.push_back(strToFood(words[kind]));
            }

            bool accepting = parsebool(row.fields[5], "accepting");
            bool pickup = parsebool(row.fields[6], "can_pickup");
            int node = parseint(row.fields[7], "node_id");

            if (capacity < 0)
            {
                throw DataException(where(name, row.number) +
                                    "available_capacity cannot be negative (V4)");
            }

            if (node < 0 || node >= target.network.nodecount())
            {
                throw DataException(where(name, row.number) +
                                    "the node does not exist in the graph (V2)");
            }

            if (!target.recipientindex.put(identity.toString(),
                                           (int)target.recipients.size()))
            {
                throw DataException(where(name, row.number) + "duplicate id " +
                                    identity.toString() + " (V1)");
            }

            target.recipients.push_back(Recipient(identity, row.fields[1], row.fields[2],
                                                  capacity, types, accepting, pickup,
                                                  node));
        }
        catch (const FormatException& problem)
        {
            throw DataException(where(name, row.number) + problem.what());
        }
    }
}

// Reads donations.csv.
static void loaddonations(const string& path, const string& name, dataset& target)
{
    vector<csvrow> rows;
    readtable(path, name, DONATIONS_HEADER, 7, rows);

    for (size_t pos = 0; pos < rows.size(); pos++)
    {
        const csvrow& row = rows[pos];

        try
        {
            EntityId identity = parseid(row.fields[0], "DON");
            EntityId donor = parseid(row.fields[1], "DN");
            FoodType food = strToFood(row.fields[2]);
            int original = parseint(row.fields[3], "original_quantity");
            int remaining = parseint(row.fields[4], "remaining_quantity");
            int usable = parseint(row.fields[5], "usable_minutes");
            DonationStatus status = strToStatus(row.fields[6]);
            int unused = 0;

            if (!target.donorindex.get(donor.toString(), unused))
            {
                throw DataException(where(name, row.number) + "the donor " +
                                    donor.toString() + " does not exist (V2)");
            }

            if (original < 1 || remaining < 0 || remaining > original)
            {
                throw DataException(where(name, row.number) +
                                    "quantities must satisfy 0 <= remaining <= original and "
                                    "original >= 1 (V3)");
            }

            if (usable < 1)
            {
                throw DataException(where(name, row.number) +
                                    "usable_minutes must be at least 1 (V4)");
            }

            if (!statusmatches(status, original, remaining))
            {
                throw DataException(where(name, row.number) +
                                    "the status does not match the quantities (V6)");
            }

            if (!target.donationindex.put(identity.toString(),
                                          (int)target.donations.size()))
            {
                throw DataException(where(name, row.number) + "duplicate id " +
                                    identity.toString() + " (V1)");
            }

            target.donations.push_back(
                Donation(identity, donor, food, original, remaining, usable, status));
        }
        catch (const FormatException& problem)
        {
            throw DataException(where(name, row.number) + problem.what());
        }
    }
}

// Reads transactions.csv and remembers the highest transaction id.
static void loadtransactions(const string& path, const string& name, dataset& target)
{
    vector<csvrow> rows;
    readtable(path, name, TRANSACTIONS_HEADER, 5, rows);
    hashindex seen;

    for (size_t pos = 0; pos < rows.size(); pos++)
    {
        const csvrow& row = rows[pos];

        try
        {
            EntityId identity = parseid(row.fields[0], "T");
            EntityId donation = parseid(row.fields[1], "DON");
            EntityId recipient = parseid(row.fields[2], "R");
            int quantity = parseint(row.fields[3], "quantity");
            int travel = parseint(row.fields[4], "travel_minutes");
            int unused = 0;

            if (!target.donationindex.get(donation.toString(), unused) ||
                !target.recipientindex.get(recipient.toString(), unused))
            {
                throw DataException(where(name, row.number) +
                                    "the donation or the recipient does not exist (V8)");
            }

            if (!seen.put(identity.toString(), (int)pos))
            {
                throw DataException(where(name, row.number) + "duplicate id " +
                                    identity.toString() + " (V1)");
            }

            if (target.lasttransaction < identity)
            {
                target.lasttransaction = identity;
            }

            target.transactions.push_back(
                Transaction(identity, donation, recipient, quantity, travel));
        }
        catch (const FormatException& problem)
        {
            throw DataException(where(name, row.number) + problem.what());
        }
    }
}

// Warns about every recipient whose node has no donor in its connected component (6.6).
static void warnunconnected(const dataset& source, list<string>& warnings)
{
    vector<int> label;
    labelcomponents(source.network, label);

    for (size_t pos = 0; pos < source.recipients.size(); pos++)
    {
        int node = source.recipients[pos].nodeId();
        bool hasdonor = false;

        for (size_t other = 0; other < source.donors.size(); other++)
        {
            if (label[source.donors[other].nodeId()] == label[node])
            {
                hasdonor = true;
            }
        }

        if (!hasdonor)
        {
            stringstream message;
            message << source.recipients[pos].id().toString() << " (node " << node
                    << ") is not connected to any donor; it can never be recommended.";
            warnings.push_back(message.str());
        }
    }
}

// Starts empty. The first transaction id issued after this is T001.
dataset::dataset()
{
    lasttransaction = EntityId("T", 0);
}

// Remembers the file name prefix.
datamanager::datamanager(const string& prefix)
{
    prefix_ = prefix;
}

// Recovery first, then the six files in dependency order, then the checks that need
// every table.
void datamanager::loadall(dataset& target, list<string>& warnings)
{
    target = dataset();

    recoverfile("donations.csv", warnings);
    recoverfile("recipients.csv", warnings);

    loadnodes(prefix_ + "graph_nodes.csv", "graph_nodes.csv", target);
    loadedges(prefix_ + "graph_edges.csv", "graph_edges.csv", target, warnings);
    loaddonors(prefix_ + "donors.csv", "donors.csv", target);
    loadrecipients(prefix_ + "recipients.csv", "recipients.csv", target);
    loaddonations(prefix_ + "donations.csv", "donations.csv", target);
    loadtransactions(prefix_ + "transactions.csv", "transactions.csv", target);

    warnunconnected(target, warnings);

    vector<string> problems = checkinvariants(target);

    if (!problems.empty())
    {
        throw DataException(problems[0]);
    }
}

// Writes the whole table to X.tmp, checks the stream, removes the old file only after
// the temp file is complete, and renames the temp file into place.
void datamanager::savetable(const string& filename, const string& header,
                            const vector<const CsvRecord*>& records)
{
    string path = prefix_ + filename;
    string temp = path + ".tmp";

    ofstream out(temp.c_str());

    if (!out.is_open())
    {
        throw DataException("Cannot write file: " + temp);
    }

    out << header << "\n";

    for (size_t pos = 0; pos < records.size(); pos++)
    {
        out << records[pos]->toCsvRow() << "\n";
    }

    out.flush();

    if (!out.good())
    {
        throw DataException("Writing failed for file: " + temp);
    }

    out.close();

    if (out.fail())
    {
        throw DataException("Closing failed for file: " + temp);
    }

    // Windows cannot rename onto an existing file, so the old file goes first
    if (fileexists(path) && remove(path.c_str()) != 0)
    {
        throw DataException("Cannot remove the old file: " + path);
    }

    if (rename(temp.c_str(), path.c_str()) != 0)
    {
        throw DataException("Cannot rename " + temp + " to " + path +
                            ". The new data is still in the temp file.");
    }
}

// Appends one row and flushes it. A missing file is created with its header first.
void datamanager::appendtransaction(const Transaction& entry)
{
    string path = prefix_ + "transactions.csv";
    bool needheader = !fileexists(path);

    ofstream out(path.c_str(), ios::app);

    if (!out.is_open())
    {
        throw DataException("Cannot write file: " + path);
    }

    if (needheader)
    {
        out << TRANSACTIONS_HEADER << "\n";
    }

    out << entry.toCsvRow() << "\n";
    out.flush();

    if (!out.good())
    {
        throw DataException("Writing failed for file: " + path);
    }
}

// Commit order of Spec 8.3.
void datamanager::commitallocation(const dataset& source, const Transaction& entry)
{
    appendtransaction(entry);
    savedonations(source);
    saverecipients(source);
}

// A close or a new donation changes only donations.csv.
void datamanager::commitdonations(const dataset& source)
{
    savedonations(source);
}

// Checks I1 to I6 and reports every problem. Nothing is repaired.
vector<string> datamanager::checkinvariants(const dataset& source) const
{
    vector<string> problems;
    vector<int> placed(source.donations.size(), 0);

    for (size_t pos = 0; pos < source.donations.size(); pos++)
    {
        const Donation& donation = source.donations[pos];
        stringstream message;

        if (donation.remainingQuantity() < 0 ||
            donation.remainingQuantity() > donation.originalQuantity())
        {
            message << "Invariant I1 broken: " << donation.id().toString()
                    << " must satisfy 0 <= remaining <= original";
            problems.push_back(message.str());
        }

        if (!statusmatches(donation.status(), donation.originalQuantity(),
                           donation.remainingQuantity()))
        {
            stringstream second;
            second << "Invariant I4 broken: the status of " << donation.id().toString()
                   << " does not match its quantities";
            problems.push_back(second.str());
        }
    }

    for (size_t pos = 0; pos < source.recipients.size(); pos++)
    {
        if (source.recipients[pos].availableCapacity() < 0)
        {
            problems.push_back("Invariant I2 broken: " +
                               source.recipients[pos].id().toString() +
                               " has a negative capacity");
        }
    }

    for (size_t pos = 0; pos < source.transactions.size(); pos++)
    {
        const Transaction& entry = source.transactions[pos];
        int donationpos = 0;
        int recipientpos = 0;
        bool donationfound = source.donationindex.get(entry.donationId().toString(),
                                                      donationpos);
        bool recipientfound = source.recipientindex.get(entry.recipientId().toString(),
                                                        recipientpos);

        if (!donationfound || !recipientfound)
        {
            problems.push_back("Invariant I5 broken: " + entry.id().toString() +
                               " refers to a donation or recipient that does not exist");
        }
        else
        {
            placed[donationpos] = placed[donationpos] + entry.quantity();
        }

        if (pos > 0 && !(source.transactions[pos - 1].id() < entry.id()))
        {
            problems.push_back("Invariant I6 broken: " + entry.id().toString() +
                               " does not come after the id before it");
        }
    }

    for (size_t pos = 0; pos < source.donations.size(); pos++)
    {
        const Donation& donation = source.donations[pos];
        int done = donation.originalQuantity() - donation.remainingQuantity();

        if (done != placed[pos])
        {
            stringstream message;
            message << "Invariant I3 broken: " << donation.id().toString() << " has original "
                    << donation.originalQuantity() << " and remaining "
                    << donation.remainingQuantity() << " but its transactions add up to "
                    << placed[pos];
            problems.push_back(message.str());
        }
    }

    return problems;
}

// X.csv missing and X.csv.tmp present: the crash came between remove and rename, so the
// temp file is complete and is renamed. Both present: X.csv is right and the temp file
// is stale and deleted.
void datamanager::recoverfile(const string& filename, list<string>& warnings)
{
    string path = prefix_ + filename;
    string temp = path + ".tmp";
    bool hasmain = fileexists(path);
    bool hastemp = fileexists(temp);

    if (!hasmain && hastemp)
    {
        if (rename(temp.c_str(), path.c_str()) != 0)
        {
            throw DataException("Cannot recover " + path + " from " + temp);
        }

        warnings.push_back(filename + " was recovered from " + filename +
                           ".tmp (the last save was interrupted).");
    }
    else if (hasmain && hastemp)
    {
        remove(temp.c_str());
        warnings.push_back("A stale " + filename + ".tmp was deleted.");
    }
}

// Collects pointers to the donations and writes them as one table.
void datamanager::savedonations(const dataset& source)
{
    vector<const CsvRecord*> records;

    for (size_t pos = 0; pos < source.donations.size(); pos++)
    {
        records.push_back(&source.donations[pos]);
    }

    savetable("donations.csv", DONATIONS_HEADER, records);
}

// Collects pointers to the recipients and writes them as one table.
void datamanager::saverecipients(const dataset& source)
{
    vector<const CsvRecord*> records;

    for (size_t pos = 0; pos < source.recipients.size(); pos++)
    {
        records.push_back(&source.recipients[pos]);
    }

    savetable("recipients.csv", RECIPIENTS_HEADER, records);
}