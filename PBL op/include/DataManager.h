#pragma once
#include <vector>
#include <string>
#include "Models.h"

// The DataManager is responsible for storing the core data collections
// and handling basic data loading (using file I/O).
class DataManager {
private:
    std::vector<Donor> donors;
    std::vector<Recipient> recipients;
    std::vector<Donation> donations;
    std::vector<Transaction> transactions;

    int nextDonationId; // Simple ID generation for Phase 1

public:
    // Constructor
    DataManager();

    // File loading using std::ifstream
    bool loadDonors(const std::string& filename);
    bool loadRecipients(const std::string& filename);

    // Add a new donation (generates simple unique ID)
    std::string createDonation(std::string donorID, int quantity, std::string foodType, int usableTime);

    // Search for a recipient by ID (Returns pointer to modify state)
    Recipient* getRecipientById(const std::string& id);

    // Getters for the collections (using const references to avoid copying)
    const std::vector<Donor>& getDonors() const { return donors; }
    const std::vector<Recipient>& getRecipients() const { return recipients; }
    const std::vector<Donation>& getDonations() const { return donations; }
};
