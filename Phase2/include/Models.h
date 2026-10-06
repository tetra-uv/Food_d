#ifndef MODELS_H
#define MODELS_H

#include "Duration.h"
#include "EntityId.h"
#include "Exceptions.h"
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum FoodType
{
    COOKED,
    PACKAGED,
    PRODUCE,
    BAKERY
};

enum DonationStatus
{
    PENDING,
    PARTIAL,
    COMPLETED,
    CLOSED
};

enum Reason
{
    OK,
    NOT_ACCEPTING,
    FOOD_INCOMPATIBLE,
    NO_CAPACITY,
    NO_PICKUP,
    UNREACHABLE,
    TOO_FAR
};

// Converts FoodType to string.
string foodToStr(FoodType type);

// Converts string to FoodType.
FoodType strToFood(const string& text);

// Converts DonationStatus to string.
string statusToStr(DonationStatus status);

// Converts string to DonationStatus.
DonationStatus strToStatus(const string& text);

// Converts Reason to string.
string reasonToStr(Reason reason);

// Converts string to Reason.
Reason strToReason(const string& text);

// Base class for CSV record formatting.
class CsvRecord
{
public:
    virtual ~CsvRecord();
    virtual string toCsvRow() const = 0;
};

// Base class for organizations (donors and recipients).
class Organization : public CsvRecord
{
public:
    Organization(const EntityId& id, const string& name, const string& type, int node);
    virtual ~Organization();
    const EntityId& id() const;
    string name() const;
    string type() const;
    int nodeId() const;

protected:
    EntityId id_; // #id
    string name_; // #name
    string type_; // #type
    int node_;    // #node
};

// Represents a donor organization.
class Donor : public Organization
{
public:
    Donor(const EntityId& id, const string& name, const string& type, int node);
    virtual ~Donor();
    string toCsvRow() const;
};

// Represents a recipient organization.
class Recipient : public Organization
{
public:
    Recipient(const EntityId& id, const string& name, const string& type, int capacity, const vector<FoodType>& types, bool accepting, bool pickup, int node);
    virtual ~Recipient();
    bool accepts(FoodType t) const;
    int availableCapacity() const;
    void reduceCapacity(int qty);
    bool accepting() const;
    bool canPickup() const;
    const vector<FoodType>& acceptedTypes() const;
    string toCsvRow() const;

private:
    int capacity_;           // #capacity
    vector<FoodType> types_; // #types
    bool accepting_;         // #accepting
    bool pickup_;            // #pickup
};

// Represents a donation event.
class Donation : public CsvRecord
{
public:
    Donation(const EntityId& id, const EntityId& donor, FoodType food, int original, int remaining, int usable, DonationStatus status);
    virtual ~Donation();
    const EntityId& id() const;
    const EntityId& donorId() const;
    FoodType foodType() const;
    int originalQuantity() const;
    int remainingQuantity() const;
    int usableMinutes() const;
    DonationStatus status() const;
    void allocateQuantity(int qty);
    void close();
    string toCsvRow() const;

private:
    EntityId id_;           // #id
    EntityId donor_;        // #donor
    FoodType food_;         // #food
    int original_;          // #original
    int remaining_;         // #remaining
    int usable_;            // #usable
    DonationStatus status_; // #status
};

// Represents an allocation transaction.
class Transaction : public CsvRecord
{
public:
    Transaction();
    Transaction(const EntityId& id, const EntityId& donation, const EntityId& recipient, int qty, int travel);
    virtual ~Transaction();
    const EntityId& id() const;
    const EntityId& donationId() const;
    const EntityId& recipientId() const;
    int quantity() const;
    int travelMinutes() const;
    string toCsvRow() const;

private:
    EntityId id_;        // #id
    EntityId donation_;  // #donation
    EntityId recipient_; // #recipient
    int qty_;            // #qty
    int travel_;         // #travel
};

#endif