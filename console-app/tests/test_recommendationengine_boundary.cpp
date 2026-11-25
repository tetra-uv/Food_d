#include "Duration.h"
#include "EntityId.h"
#include "Models.h"
#include "RecommendationEngine.h"
#include "fixtures/boundary_travel_fixture.h"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

static int failcount = 0;

static void check(bool condition, const string& name)
{
    if (condition)
    {
        cout << "PASS " << name << "\n";
    }
    else
    {
        cout << "FAIL " << name << "\n";
        failcount++;
    }
}

int main()
{
    cout << "--- test_recommendationengine_boundary ---\n";

    // 1. Boundary equality: travel == usable (15 == 15) is ACCEPTED
    {
        Donation don(EntityId("DON", 1), EntityId("DN", 1), COOKED, 100, 100, 15, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_boundary_recipient(1, 100, COOKED, 0));

        vector<Duration> travel;
        travel.push_back(Duration(15)); // exactly equal to usable_minutes

        RecommendationResult res = recommend(don, recs, travel);
        check(res.candidates().size() == 1 && res.candidates()[0].recipientId() == EntityId("R", 1),
              "BND1: travel == usable (15 == 15) is accepted as feasible candidate");
    }

    // 2. Boundary reject: travel == usable + 1 (16 > 15) is REJECTED with TOO_FAR
    {
        Donation don(EntityId("DON", 1), EntityId("DN", 1), COOKED, 100, 100, 15, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_boundary_recipient(1, 100, COOKED, 0));

        vector<Duration> travel;
        travel.push_back(Duration(16)); // 1 min beyond usable

        RecommendationResult res = recommend(don, recs, travel);
        check(res.candidates().empty() && res.rejected().size() == 1 &&
              res.rejected()[0].reason() == TOO_FAR &&
              res.rejected()[0].travel().minutes() == 16,
              "BND2: travel == usable + 1 (16 > 15) is rejected with TOO_FAR (16 min)");
    }

    // 3. Huge usable minutes (999999999) is NEVER rejected as TOO_FAR against finite travel time
    {
        Donation don(EntityId("DON", 2), EntityId("DN", 1), PACKAGED, 50, 50, 999999999, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_boundary_recipient(1, 100, PACKAGED, 0));
        recs.push_back(make_boundary_recipient(2, 100, PACKAGED, 1));

        vector<Duration> travel;
        travel.push_back(Duration(5000));
        travel.push_back(Duration(100000));

        RecommendationResult res = recommend(don, recs, travel);
        check(res.candidates().size() == 2,
              "BND3: usable_minutes 999999999 accepts large finite travel times without TOO_FAR");
    }

    // 4. Zero travel time: travel == 0 is ACCEPTED
    {
        Donation don(EntityId("DON", 3), EntityId("DN", 1), PRODUCE, 80, 80, 20, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_boundary_recipient(1, 100, PRODUCE, 0));

        vector<Duration> travel;
        travel.push_back(Duration(0));

        RecommendationResult res = recommend(don, recs, travel);
        check(res.candidates().size() == 1 && res.candidates()[0].travel().minutes() == 0,
              "BND4: zero travel time is accepted as feasible");
    }

    // 5. Infinite travel time is REJECTED with UNREACHABLE
    {
        Donation don(EntityId("DON", 4), EntityId("DN", 1), BAKERY, 80, 80, 999999999, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_boundary_recipient(1, 100, BAKERY, 0));

        vector<Duration> travel;
        travel.push_back(Duration::infinite());

        RecommendationResult res = recommend(don, recs, travel);
        check(res.candidates().empty() && res.rejected().size() == 1 &&
              res.rejected()[0].reason() == UNREACHABLE,
              "BND5: infinite travel time is rejected as UNREACHABLE even with 999999999 usable");
    }

    cout << "Total FAIL: " << failcount << "\n";
    return failcount > 0 ? 1 : 0;
}
