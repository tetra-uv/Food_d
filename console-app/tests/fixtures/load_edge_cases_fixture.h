#ifndef LOAD_EDGE_CASES_FIXTURE_H
#define LOAD_EDGE_CASES_FIXTURE_H

#include "../csv_fixture.h"
#include <string>

inline void setup_load_edge_cases(const std::string& prefix)
{
    removefixture(prefix);
    writefixture(prefix);
}

inline void teardown_load_edge_cases(const std::string& prefix)
{
    removefixture(prefix);
}

#endif
