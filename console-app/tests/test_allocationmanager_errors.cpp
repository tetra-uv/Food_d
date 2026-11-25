#include "Duration.h"
#include "EntityId.h"
#include "Models.h"
#include "allocationmanager.h"
#include "fixtures/allocation_errors_fixture.h"

#include <iostream>
#include <string>

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
    cout << "--- test_allocationmanager_errors ---\n";

    // 1. DONATION_NOT_OPEN: CLOSED donation
    {
        Donation don = make_alloc_donation(1, COOKED, 100, 50, 60, CLOSED);
        Recipient rec = make_alloc_recipient(1, 100, COOKED, true, true, 0);
        EntityId txId("T", 1);
        AllocError err = allocate(don, rec, 20, Duration(10), txId);

        check(err == DONATION_NOT_OPEN && don.remainingQuantity() == 50 &&
              rec.availableCapacity() == 100,
              "ERR1: allocating on CLOSED donation returns DONATION_NOT_OPEN and mutates nothing");
    }

    // 2. DONATION_NOT_OPEN: COMPLETED donation
    {
        Donation don = make_alloc_donation(1, COOKED, 100, 0, 60, COMPLETED);
        Recipient rec = make_alloc_recipient(1, 100, COOKED, true, true, 0);
        EntityId txId("T", 1);
        AllocError err = allocate(don, rec, 10, Duration(10), txId);

        check(err == DONATION_NOT_OPEN && don.remainingQuantity() == 0,
              "ERR2: allocating on COMPLETED donation returns DONATION_NOT_OPEN");
    }

    // 3. BAD_QUANTITY: 0 and negative
    {
        Donation don = make_alloc_donation(1, COOKED, 100, 100, 60, PENDING);
        Recipient rec = make_alloc_recipient(1, 100, COOKED, true, true, 0);
        EntityId txId("T", 1);

        AllocError err0 = allocate(don, rec, 0, Duration(10), txId);
        AllocError errNeg = allocate(don, rec, -10, Duration(10), txId);

        check(err0 == BAD_QUANTITY && errNeg == BAD_QUANTITY &&
              don.remainingQuantity() == 100 && rec.availableCapacity() == 100,
              "ERR3: quantity <= 0 returns BAD_QUANTITY and mutates nothing");
    }

    // 4. EXCEEDS_REMAINING: quantity > remaining
    {
        Donation don = make_alloc_donation(1, COOKED, 100, 30, 60, PARTIAL);
        Recipient rec = make_alloc_recipient(1, 200, COOKED, true, true, 0);
        EntityId txId("T", 1);

        AllocError err = allocate(don, rec, 31, Duration(10), txId);

        check(err == EXCEEDS_REMAINING && don.remainingQuantity() == 30 &&
              rec.availableCapacity() == 200,
              "ERR4: quantity exceeding remaining returns EXCEEDS_REMAINING");
    }

    // 5. EXCEEDS_CAPACITY: quantity > capacity
    {
        Donation don = make_alloc_donation(1, COOKED, 100, 100, 60, PENDING);
        Recipient rec = make_alloc_recipient(1, 40, COOKED, true, true, 0);
        EntityId txId("T", 1);

        AllocError err = allocate(don, rec, 50, Duration(10), txId);

        check(err == EXCEEDS_CAPACITY && don.remainingQuantity() == 100 &&
              rec.availableCapacity() == 40,
              "ERR5: quantity exceeding recipient capacity returns EXCEEDS_CAPACITY");
    }

    // 6. NOT_FEASIBLE: food incompatible, cannot pickup, not accepting, travel > usable
    {
        Donation don = make_alloc_donation(1, COOKED, 100, 100, 60, PENDING);
        EntityId txId("T", 1);

        Recipient wrongFood = make_alloc_recipient(1, 100, BAKERY, true, true, 0);
        Recipient noPickup = make_alloc_recipient(2, 100, COOKED, true, false, 0);
        Recipient notAccepting = make_alloc_recipient(3, 100, COOKED, false, true, 0);
        Recipient tooFarRec = make_alloc_recipient(4, 100, COOKED, true, true, 0);

        AllocError errFood = allocate(don, wrongFood, 10, Duration(10), txId);
        AllocError errPickup = allocate(don, noPickup, 10, Duration(10), txId);
        AllocError errAccept = allocate(don, notAccepting, 10, Duration(10), txId);
        AllocError errFar = allocate(don, tooFarRec, 10, Duration(65), txId);

        check(errFood == NOT_FEASIBLE && errPickup == NOT_FEASIBLE &&
              errAccept == NOT_FEASIBLE && errFar == NOT_FEASIBLE &&
              don.remainingQuantity() == 100,
              "ERR6: infeasible recipient conditions return NOT_FEASIBLE");
    }

    // 7. Successful allocation mutates both donation and recipient
    {
        Donation don = make_alloc_donation(1, COOKED, 100, 100, 60, PENDING);
        Recipient rec = make_alloc_recipient(1, 100, COOKED, true, true, 0);
        EntityId txId("T", 1);

        AllocError ok = allocate(don, rec, 40, Duration(10), txId);

        check(ok == ALLOC_OK && don.remainingQuantity() == 60 &&
              don.status() == PARTIAL && rec.availableCapacity() == 60,
              "ERR7: ALLOC_OK correctly updates donation remaining and recipient capacity");
    }

    cout << "Total FAIL: " << failcount << "\n";
    return failcount > 0 ? 1 : 0;
}
