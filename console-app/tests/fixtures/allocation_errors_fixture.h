#ifndef ALLOCATION_ERRORS_FIXTURE_H
#define ALLOCATION_ERRORS_FIXTURE_H

#include "Duration.h"
#include "EntityId.h"
#include "Models.h"
#include <vector>

inline Donation make_alloc_donation(int idNum, FoodType ft, int orig, int rem, int usable, DonationStatus st)
{
    return Donation(EntityId("DON", idNum), EntityId("DN", 1), ft, orig, rem, usable, st);
}

inline Recipient make_alloc_recipient(int idNum, int cap, FoodType ft, bool accepting, bool pickup, int node)
{
    std::vector<FoodType> types;
    types.push_back(ft);
    return Recipient(EntityId("R", idNum), "Recipient " + std::to_string(idNum),
                     "SHELTER", cap, types, accepting, pickup, node);
}

#endif
