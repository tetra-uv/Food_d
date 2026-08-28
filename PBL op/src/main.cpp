#include <iostream>
#include "../include/DataManager.h"

void displayDonors(const DataManager& dataManager) {
    std::cout << "\n--- DONORS ---\n";
    for (const Donor& donor : dataManager.getDonors()) {
        std::cout << "ID: " << donor.getDonorID() 
                  << " | Name: " << donor.getName() 
                  << " | Location: (" << donor.getLocationX() << ", " << donor.getLocationY() << ")"
                  << " | Pickup Avail: " << donor.getPickupAvailability() << "\n";
    }
}

void displayRecipients(const DataManager& dataManager) {
    std::cout << "\n--- RECIPIENTS ---\n";
    for (const Recipient& recipient : dataManager.getRecipients()) {
        std::cout << "ID: " << recipient.getRecipientID() 
                  << " | Name: " << recipient.getName() 
                  << " | Cap: " << recipient.getCapacity() 
                  << " | Accepting: " << (recipient.isAccepting() ? "Yes" : "No")
                  << " | Food Types: " << recipient.getAcceptedFoodTypes() << "\n";
    }
}

void displayDonations(const DataManager& dataManager) {
    std::cout << "\n--- DONATIONS ---\n";
    for (const Donation& donation : dataManager.getDonations()) {
        std::cout << "ID: " << donation.getDonationID() 
                  << " | Donor ID: " << donation.getDonorID() 
                  << " | Original Qty: " << donation.getOriginalQuantity()
                  << " | Remaining Qty: " << donation.getRemainingQuantity()
                  << " | Usable Time: " << donation.getUsableTimeRemaining() << " mins"
                  << " | State: " << donation.getStateString() << "\n";
    }
}

int main() {
    std::cout << "=========================================\n";
    std::cout << "FOOD RESCUE DECISION SUPPORT SYSTEM\n";
    std::cout << "Phase 1: Initialization & Data Models\n";
    std::cout << "=========================================\n";

    DataManager dataManager;
    
    std::cout << "Loading sample data...\n";
    dataManager.loadSampleData();

    // Display loaded data to verify Phase 1 requirements
    displayDonors(dataManager);
    displayRecipients(dataManager);
    displayDonations(dataManager);

    std::cout << "\n=========================================\n";
    std::cout << "Phase 1 Verification Complete.\n";
    std::cout << "=========================================\n";

    return 0;
}
