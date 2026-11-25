#include "Duration.h"
#include "EntityId.h"
#include "Models.h"
#include "RecommendationEngine.h"
#include "fixtures/ranking_fixture.h"

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
    cout << "--- test_recommendationengine_ranking ---\n";

    // 1. Key 1: Higher potential acceptance ranks first
    {
        Donation don(EntityId("DON", 1), EntityId("DN", 1), COOKED, 100, 100, 60, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_ranking_recipient(1, 40, COOKED, 0)); // accepts 40
        recs.push_back(make_ranking_recipient(2, 90, COOKED, 1)); // accepts 90

        vector<Duration> travel(2, Duration(10));
        RecommendationResult res = recommend(don, recs, travel);

        check(res.candidates().size() == 2 &&
              res.candidates()[0].recipientId() == EntityId("R", 2) &&
              res.candidates()[1].recipientId() == EntityId("R", 1),
              "RNK1: higher potential acceptance (90 vs 40) ranks first");
    }

    // 2. Key 2: Equal acceptance, shorter travel time ranks first
    {
        Donation don(EntityId("DON", 1), EntityId("DN", 1), COOKED, 50, 50, 60, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_ranking_recipient(1, 100, COOKED, 0)); // accepts 50, travel 25 min
        recs.push_back(make_ranking_recipient(2, 100, COOKED, 1)); // accepts 50, travel 15 min

        vector<Duration> travel;
        travel.push_back(Duration(25));
        travel.push_back(Duration(15));
        RecommendationResult res = recommend(don, recs, travel);

        check(res.candidates().size() == 2 &&
              res.candidates()[0].recipientId() == EntityId("R", 2) &&
              res.candidates()[1].recipientId() == EntityId("R", 1),
              "RNK2: shorter travel time (15 vs 25 min) breaks tie when acceptance is equal");
    }

    // 3. Key 3: Equal acceptance and travel time, higher unused capacity ranks first
    {
        Donation don(EntityId("DON", 1), EntityId("DN", 1), COOKED, 50, 50, 60, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_ranking_recipient(1, 100, COOKED, 0)); // accepts 50, unused 50
        recs.push_back(make_ranking_recipient(2, 200, COOKED, 1)); // accepts 50, unused 150

        vector<Duration> travel(2, Duration(15));
        RecommendationResult res = recommend(don, recs, travel);

        check(res.candidates().size() == 2 &&
              res.candidates()[0].recipientId() == EntityId("R", 1) &&
              res.candidates()[1].recipientId() == EntityId("R", 2),
              "RNK3: smaller unused capacity (50 vs 150) breaks tie when acceptance and travel match");
    }

    // 4. Key 4: Equal acceptance, travel, and unused capacity, smaller EntityId ranks first
    {
        Donation don(EntityId("DON", 1), EntityId("DN", 1), COOKED, 50, 50, 60, PENDING);
        vector<Recipient> recs;
        recs.push_back(make_ranking_recipient(5, 100, COOKED, 0)); // R005
        recs.push_back(make_ranking_recipient(2, 100, COOKED, 1)); // R002

        vector<Duration> travel(2, Duration(15));
        RecommendationResult res = recommend(don, recs, travel);

        check(res.candidates().size() == 2 &&
              res.candidates()[0].recipientId() == EntityId("R", 2) &&
              res.candidates()[1].recipientId() == EntityId("R", 5),
              "RNK4: smaller EntityId (R002 vs R005) breaks 4th-tier tie");
    }

    cout << "Total FAIL: " << failcount << "\n";
    return failcount > 0 ? 1 : 0;
}
