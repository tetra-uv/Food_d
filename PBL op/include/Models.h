#pragma once
#include <string>
#include <vector>

// Donation state enumeration
enum class DonationState {
    OPEN,
    PARTIAL,
    CLOSED,
    EXPIRED
};

// Represents a Donor (e.g., restaurant, hotel)
class Donor {
private:
    std::string donorID;
    std::string name;
    int locationX;
    int locationY;
    std::string pickupAvailability;

public:
    // Constructor
    Donor(std::string id, std::string n, int x, int y, std::string avail) 
        : donorID(id), name(n), locationX(x), locationY(y), pickupAvailability(avail) {}

    // Simple public getters
    std::string getDonorID() const { return donorID; }
    std::string getName() const { return name; }
    int getLocationX() const { return locationX; }
    int getLocationY() const { return locationY; }
    std::string getPickupAvailability() const { return pickupAvailability; }
};

// Represents a Recipient (e.g., NGO, Shelter)
class Recipient {
private:
    std::string recipientID;
    std::string name;
    std::string type;
    int locationX;
    int locationY;
    int currentCapacity;
    bool currentlyAccepting;
    std::string acceptedFoodTypes;
    std::string pickupCapability;

public:
    // Constructor
    Recipient(std::string id, std::string n, std::string t, int x, int y, 
              int cap, bool accepting, std::string foodTypes, std::string pickupCap)
        : recipientID(id), name(n), type(t), locationX(x), locationY(y),
          currentCapacity(cap), currentlyAccepting(accepting), 
          acceptedFoodTypes(foodTypes), pickupCapability(pickupCap) {}

    // Public getters
    std::string getRecipientID() const { return recipientID; }
    std::string getName() const { return name; }
    std::string getType() const { return type; }
    int getLocationX() const { return locationX; }
    int getLocationY() const { return locationY; }
    int getCapacity() const { return currentCapacity; }
    bool isAccepting() const { return currentlyAccepting; }
    std::string getAcceptedFoodTypes() const { return acceptedFoodTypes; }
    std::string getPickupCapability() const { return pickupCapability; }

    // Public setters for state management
    void setCapacity(int cap) { currentCapacity = cap; }
    void setAccepting(bool accepting) { currentlyAccepting = accepting; }
    void setPickupCapability(const std::string& cap) { pickupCapability = cap; }
};

// Represents a Donation created by a Donor
class Donation {
private:
    std::string donationID;
    std::string donorID;
    int originalQuantity;
    int remainingQuantity;
    std::string foodType;
    int usableTimeRemaining; // In minutes
    DonationState state;

public:
    // Constructor initializes remaining equal to original and state to OPEN
    Donation(std::string id, std::string d_id, int qty, std::string type, int timeRemaining)
        : donationID(id), donorID(d_id), originalQuantity(qty), remainingQuantity(qty),
          foodType(type), usableTimeRemaining(timeRemaining), state(DonationState::OPEN) {}

    // Public getters
    std::string getDonationID() const { return donationID; }
    std::string getDonorID() const { return donorID; }
    int getOriginalQuantity() const { return originalQuantity; }
    int getRemainingQuantity() const { return remainingQuantity; }
    std::string getFoodType() const { return foodType; }
    int getUsableTimeRemaining() const { return usableTimeRemaining; }
    DonationState getState() const { return state; }
    
    // Helper to print state as string
    std::string getStateString() const {
        switch(state) {
            case DonationState::OPEN: return "OPEN";
            case DonationState::PARTIAL: return "PARTIAL";
            case DonationState::CLOSED: return "CLOSED";
            case DonationState::EXPIRED: return "EXPIRED";
            default: return "UNKNOWN";
        }
    }

    // Public setters for state management
    void setRemainingQuantity(int remaining) { 
        remainingQuantity = remaining; 
        if (remainingQuantity <= 0) {
            state = DonationState::CLOSED;
        } else if (remainingQuantity < originalQuantity) {
            state = DonationState::PARTIAL;
        } else {
            state = DonationState::OPEN;
        }
    }
    
    void setState(DonationState newState) { state = newState; }
};

// Represents an Allocation/Transaction
class Transaction {
private:
    std::string transactionID;
    std::string donationID;
    std::string recipientID;
    int allocatedQuantity;

public:
    // Constructor
    Transaction(std::string t_id, std::string d_id, std::string r_id, int qty)
        : transactionID(t_id), donationID(d_id), recipientID(r_id), allocatedQuantity(qty) {}

    // Public getters
    std::string getTransactionID() const { return transactionID; }
    std::string getDonationID() const { return donationID; }
    std::string getRecipientID() const { return recipientID; }
    int getAllocatedQuantity() const { return allocatedQuantity; }
};
