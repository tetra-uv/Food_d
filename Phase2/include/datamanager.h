#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include "EntityId.h"
#include "Models.h"
#include "graph.h"
#include "hashindex.h"
#include <list>
#include <string>
#include <vector>

using namespace std;

// Everything the program keeps in memory. The vectors and the graph own their data and
// every member copies deeply, so copying a dataset gives a fresh independent dataset.
// The three hash indexes map an id text such as R004 to the position in its vector.
struct dataset
{
    // Starts empty. The last transaction id is T000 until a file says otherwise.
    dataset();

    vector<Donor> donors;
    vector<Recipient> recipients;
    vector<Donation> donations;
    vector<Transaction> transactions;
    graph network;
    vector<string> nodelabels;
    vector<double> nodexs;
    vector<double> nodeys;
    hashindex donorindex;
    hashindex recipientindex;
    hashindex donationindex;
    EntityId lasttransaction;
};

// Reads and writes the CSV files. Every file name is the prefix plus the table name,
// for example the prefix Phase2/data/ gives Phase2/data/donors.csv.
class datamanager
{
public:
    // Remembers the text that is put in front of every file name.
    datamanager(const string& prefix);

    // Loads and validates all six files (Spec 8.2). Throws DataException with the file,
    // the line and the reason, and refuses to continue on bad data. Non-fatal findings
    // such as a duplicate edge or an unconnected recipient are added to warnings.
    void loadall(dataset& target, list<string>& warnings);

    // Writes any list of records to a file in a safe way (Spec 8.3): the new content goes
    // to a .tmp file first, then the old file is removed and the .tmp file is renamed.
    void savetable(const string& filename, const string& header,
                   const vector<const CsvRecord*>& records);

    // Adds one row to the end of transactions.csv and flushes it.
    void appendtransaction(const Transaction& entry);

    // Saves after an allocation in the commit order of Spec 8.3: the transaction row,
    // then donations.csv, then recipients.csv.
    void commitallocation(const dataset& source, const Transaction& entry);

    // Saves donations.csv only. Used when a donation is closed or a new one is added.
    void commitdonations(const dataset& source);

    // Checks the invariants I1 to I6 of Spec 11.4 and returns one message per problem.
    // An empty list means the dataset is consistent. Nothing is ever repaired silently.
    vector<string> checkinvariants(const dataset& source) const;

private:
    // Text put in front of every file name
    string prefix_;

    // Handles a leftover .tmp file of a table at startup (Spec 8.3).
    void recoverfile(const string& filename, list<string>& warnings);

    // Writes donations.csv through savetable.
    void savedonations(const dataset& source);

    // Writes recipients.csv through savetable.
    void saverecipients(const dataset& source);
};

#endif