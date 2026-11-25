#ifndef FLAGS_H
#define FLAGS_H

// Flags.h: finds records that look like typing errors (a huge quantity, a huge usable time, a huge room). They stay in every
// total; they are only flagged in red with the word Check and named in plain notes (plain C++, no window code).

#include "Datamanager.h"
#include "EntityId.h"
#include "Models.h"

#include <string>
#include <vector>

using namespace std;

static const int BIGQUANTITY = 10000;  // a donation quantity above this is flagged
static const int LONGMINUTES = 10080;  // usable minutes above this (seven days) are flagged
static const int BIGCAPACITY = 5000;   // a recipient room above this is flagged

// "" for a normal donation, else "Very large quantity", "Very long usable time" or both joined with "and".
string donationflagtext(const Donation& donation);

// "" for a normal recipient, else "Very large room".
string recipientflagtext(const Recipient& recipient);

// The notes for the coordinator: every flagged open donation (waiting or partly given) and every flagged recipient.
vector<string> flagnotes(const dataset& data);

// The notes for one donor: only his own flagged open donations (without "from" and his name).
vector<string> flagnotesfordonor(const dataset& data, const EntityId& donor);

// The note for one recipient when his own room is flagged.
vector<string> flagnotesforrecipient(const dataset& data, const EntityId& recipient);

#endif
