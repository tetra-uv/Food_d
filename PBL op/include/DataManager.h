#pragma once
#include <vector>
#include <string>
#include "Models.h"

// The DataManager is responsible for storing the core data collections
// and handling basic data loading (simulated for Phase 1).
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

    // Setup function for Phase 1 to load hardcoded sample data
    void loadSampleData();

    // Add a new donation (generates simple unique ID)
    std::string createDonation(std::string donorID, int quantity, std::string foodType, int usableTime);

    // Getters for the collections (using const references to avoid copying)
    const std::vector<Donor>& getDonors() const { return donors; }
    const std::vector<Recipient>& getRecipients() const { return recipients; }
    const std::vector<Donation>& getDonations() const { return donations; }
};
