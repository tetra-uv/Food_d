#ifndef RANKING_FIXTURE_H
#define RANKING_FIXTURE_H

#include "Duration.h"
#include "EntityId.h"
#include "Models.h"
#include <vector>

inline Recipient make_ranking_recipient(int idNum, int cap, FoodType ft, int node)
{
    std::vector<FoodType> types;
    types.push_back(ft);
    return Recipient(EntityId("R", idNum), "Recipient " + std::to_string(idNum),
                     "SHELTER", cap, types, true, true, node);
}

#endif
