#ifndef RECIPIENTUPDATE_H
#define RECIPIENTUPDATE_H

// RecipientUpdate.h: lets a recipient change its own capacity and flags.

#include "Datamanager.h"
#include "EntityId.h"

#include <string>
#include <vector>

using namespace std;

// Changes the capacity, the accepting flag and the pickup flag of one recipient and saves
// recipients.csv. Returns false and a plain message when the recipient is unknown, the capacity
// is below 0 or the save failed (then the old recipient is put back in memory).
bool updaterecipient(dataset& data, datamanager& files, const EntityId& recipient, int capacity,
                     bool accepting, bool pickup, string& message);

// Like updaterecipient, and also sets the food types the recipient takes. Refuses an empty list with "Choose at least one
// food type"; duplicates are stored once and the types are kept in the order COOKED, PACKAGED, PRODUCE, BAKERY.
bool updaterecipientdetails(dataset& data, datamanager& files, const EntityId& recipient, int capacity, bool accepting,
                            bool pickup, const vector<FoodType>& types, string& message);

#endif
