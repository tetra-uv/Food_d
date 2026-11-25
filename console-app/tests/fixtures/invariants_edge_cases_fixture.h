#ifndef INVARIANTS_EDGE_CASES_FIXTURE_H
#define INVARIANTS_EDGE_CASES_FIXTURE_H

#include "../csv_fixture.h"
#include <string>

inline void setup_invariants_fixture(const std::string& prefix)
{
    removefixture(prefix);
    writefixture(prefix);
}

inline void teardown_invariants_fixture(const std::string& prefix)
{
    removefixture(prefix);
}

#endif
