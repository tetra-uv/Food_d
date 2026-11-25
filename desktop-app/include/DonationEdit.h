#ifndef DONATIONEDIT_H
#define DONATIONEDIT_H

// DonationEdit.h: lets a donor change or delete his own donation while none of it was given out yet (plain C++, no window code).

#include "Datamanager.h"
#include "EntityId.h"
#include "Models.h"
#include "OfferStore.h"

#include <string>

using namespace std;

// Tells whether the donation at pos may still be changed: it is waiting, nothing of it was given out and no transaction names
// it. Otherwise returns false with a plain reason.
bool candeditdonation(const dataset& data, const offerstore& store, int pos, string& reason);

// Gives the donor's donation at pos a new food type, quantity and usable minutes. Its waiting offers are withdrawn and saved
// first, then donations.csv is rewritten. Returns false with a plain message (and memory unchanged) when it is not allowed or
// a file could not be saved.
bool editdonation(dataset& data, offerstore& store, datamanager& files, const EntityId& donor, int pos, FoodType food,
                  int qty, int usable, string& message);

// Removes the donor's donation at pos together with every offer row of it. The offers file is saved first, then
// donations.csv is rewritten. Returns false with a plain message (and memory unchanged) when it is not allowed or a file could
// not be saved.
bool deletedonation(dataset& data, offerstore& store, datamanager& files, const EntityId& donor, int pos, string& message);

#endif
