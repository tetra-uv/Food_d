#ifndef RECOMMENDATIONENGINE_H
#define RECOMMENDATIONENGINE_H

#include "Duration.h"
#include "EntityId.h"
#include "Models.h"
#include <iostream>
#include <vector>

using namespace std;

// Runs the six feasibility checks in a fixed order and returns the first one that fails,
// or OK when the recipient can take the donation. Travel equal to the usable time is OK.
Reason checkfeasibility(const Recipient& recipient, const Donation& donation,
                        const Duration& travel);

// A recipient that passed every check, with the numbers that decide its rank.
class candidate
{
public:
    // Creates an empty candidate (needed to sort an array of candidates).
    candidate();

    // Stores the recipient number, how much it can take, the travel time and the
    // capacity that stays unused after that.
    candidate(const EntityId& identity, int accept, const Duration& time, int spare);

    // Returns the recipient number.
    const EntityId& recipientid() const;

    // Returns the smaller of the remaining quantity and the recipient capacity.
    int potentialacceptance() const;

    // Returns the travel time from the donor.
    const Duration& traveltime() const;

    // Returns the capacity that stays unused after the donation is placed.
    int unusedcapacity() const;

    // Ranks a before b: higher acceptance, then shorter travel, then smaller unused
    // capacity, then smaller recipient number. This is a strict total order.
    friend bool operator<(const candidate& first, const candidate& second);

    // Prints the candidate as R004 (80 / 12 / 0): acceptance, travel, unused capacity.
    friend ostream& operator<<(ostream& out, const candidate& entry);

private:
    // Recipient number
    EntityId recipientid_;
    // How much of the donation the recipient can take
    int accept_;
    // Travel time from the donor
    Duration travel_;
    // Capacity left after placing the donation
    int unused_;
};

// A recipient that failed a check, with the reason that was reported first.
class rejection
{
public:
    // Stores the recipient number, the reason and the travel time that was found.
    rejection(const EntityId& identity, Reason why, const Duration& time);

    // Returns the recipient number.
    const EntityId& recipientid() const;

    // Returns the reason code of the first failed check.
    Reason reason() const;

    // Returns the travel time from the donor.
    const Duration& traveltime() const;

    // Prints the recipient number and the reason code. TOO_FAR also shows the travel
    // time.
    friend ostream& operator<<(ostream& out, const rejection& entry);

private:
    // Recipient number
    EntityId recipientid_;
    // Reason code of the first failed check
    Reason reason_;
    // Travel time from the donor
    Duration travel_;
};

// The outcome of recommend: the ranked candidates and every rejected recipient.
struct recommendationresult
{
    vector<candidate> candidates;
    vector<rejection> rejections;
};

// Checks every recipient for the donation, then ranks the feasible ones with mergesort.
// traveltimes holds the travel time from the donor to every node, indexed by node number.
// The program only recommends. The coordinator makes the final choice.
recommendationresult recommend(const Donation& donation,
                               const vector<Recipient>& recipients,
                               const vector<Duration>& traveltimes);

#endif