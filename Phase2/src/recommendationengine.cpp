#include "recommendationengine.h"
#include "Exceptions.h"
#include "sorting.h"

using namespace std;

// Six checks in the fixed order of Spec 7.1. The first failing check is the reason.
Reason checkfeasibility(const Recipient& recipient, const Donation& donation,
                        const Duration& travel)
{
    if (!recipient.accepting())
    {
        return NOT_ACCEPTING;
    }

    if (!recipient.accepts(donation.foodType()))
    {
        return FOOD_INCOMPATIBLE;
    }

    if (recipient.availableCapacity() <= 0)
    {
        return NO_CAPACITY;
    }

    if (!recipient.canPickup())
    {
        return NO_PICKUP;
    }

    if (travel.isInfinite())
    {
        return UNREACHABLE;
    }

    // Travel equal to the usable time is still feasible
    if (Duration(donation.usableMinutes()) < travel)
    {
        return TOO_FAR;
    }

    return OK;
}

// Creates an empty candidate.
candidate::candidate()
{
    accept_ = 0;
    unused_ = 0;
}

// Stores the values that decide the rank.
candidate::candidate(const EntityId& identity, int accept, const Duration& time,
                     int spare)
{
    recipientid_ = identity;
    accept_ = accept;
    travel_ = time;
    unused_ = spare;
}

// Returns the recipient number.
const EntityId& candidate::recipientid() const
{
    return recipientid_;
}

// Returns how much of the donation the recipient can take.
int candidate::potentialacceptance() const
{
    return accept_;
}

// Returns the travel time from the donor.
const Duration& candidate::traveltime() const
{
    return travel_;
}

// Returns the capacity that stays unused.
int candidate::unusedcapacity() const
{
    return unused_;
}

// The ranking rule of Spec 7.2. The first criterion that differs decides.
bool operator<(const candidate& first, const candidate& second)
{
    if (first.accept_ != second.accept_)
    {
        return first.accept_ > second.accept_;
    }

    if (!(first.travel_ == second.travel_))
    {
        return first.travel_ < second.travel_;
    }

    if (first.unused_ != second.unused_)
    {
        return first.unused_ < second.unused_;
    }

    return first.recipientid_ < second.recipientid_;
}

// Prints the candidate in one short line.
ostream& operator<<(ostream& out, const candidate& entry)
{
    out << entry.recipientid_ << " (" << entry.accept_ << " / " << entry.travel_ << " / "
        << entry.unused_ << ")";
    return out;
}

// Stores the recipient number, the reason and the travel time.
rejection::rejection(const EntityId& identity, Reason why, const Duration& time)
{
    recipientid_ = identity;
    reason_ = why;
    travel_ = time;
}

// Returns the recipient number.
const EntityId& rejection::recipientid() const
{
    return recipientid_;
}

// Returns the reason code.
Reason rejection::reason() const
{
    return reason_;
}

// Returns the travel time from the donor.
const Duration& rejection::traveltime() const
{
    return travel_;
}

// Prints the reason code, and the travel time only when the recipient was too far.
ostream& operator<<(ostream& out, const rejection& entry)
{
    out << entry.recipientid_ << " " << reasonToStr(entry.reason_);

    if (entry.reason_ == TOO_FAR)
    {
        out << " (" << entry.travel_ << ")";
    }

    return out;
}

// Splits the recipients into candidates and rejections, then ranks the candidates.
recommendationresult recommend(const Donation& donation,
                               const vector<Recipient>& recipients,
                               const vector<Duration>& traveltimes)
{
    recommendationresult found;

    for (size_t pos = 0; pos < recipients.size(); pos++)
    {
        const Recipient& current = recipients[pos];
        int node = current.nodeId();

        if (node < 0 || node >= (int)traveltimes.size())
        {
            throw StructureException("A recipient node has no travel time");
        }

        Duration travel = traveltimes[node];
        Reason why = checkfeasibility(current, donation, travel);

        if (why == OK)
        {
            int accept = donation.remainingQuantity();

            if (current.availableCapacity() < accept)
            {
                accept = current.availableCapacity();
            }

            int spare = current.availableCapacity() - accept;
            found.candidates.push_back(candidate(current.id(), accept, travel, spare));
        }
        else
        {
            found.rejections.push_back(rejection(current.id(), why, travel));
        }
    }

    // mergesort works on an array, so the candidates are copied there and back
    int count = (int)found.candidates.size();

    if (count > 1)
    {
        candidate* items = new candidate[count];

        for (int pos = 0; pos < count; pos++)
        {
            items[pos] = found.candidates[pos];
        }

        mergesort(items, count);

        for (int pos = 0; pos < count; pos++)
        {
            found.candidates[pos] = items[pos];
        }

        delete[] items;
    }

    return found;
}