#include <iostream>
#include <string>
#include <limits>
#include "../include/DataManager.h"

// Helper to safely get an integer from the user
int getIntInput() {
    int value;
    while (!(std::cin >> value)) {
        std::cin.clear(); // clear error flag
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
        std::cout << "Invalid input. Please enter a number: ";
    }
    // Clear the rest of the line (e.g. trailing newline)
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return value;
}

// Helper to safely get a string input
std::string getStringInput() {
    std::string value;
    std::getline(std::cin, value);
    return value;
}

void displayDonors(const DataManager& dataManager) {
    std::cout << "\n--- DONORS ---\n";
    for (const Donor& donor : dataManager.getDonors()) {
        std::cout << "ID: " << donor.getDonorID() 
                  << " | Name: " << donor.getName() 
                  << " | Location: (" << donor.getLocationX() << ", " << donor.getLocationY() << ")"
                  << " | Pickup Avail: " << donor.getPickupAvailability() << "\n";
    }
    std::cout << "-----------------------\n";
}

void displayRecipients(const DataManager& dataManager) {
    std::cout << "\n--- RECIPIENTS ---\n";
    for (const Recipient& recipient : dataManager.getRecipients()) {
        std::cout << "----------------------------------------\n";
        std::cout << "Recipient ID: " << recipient.getRecipientID() << "\n";
        std::cout << "Name: " << recipient.getName() << "\n";
        std::cout << "Type: " << recipient.getType() << "\n";
        std::cout << "Location: (" << recipient.getLocationX() << ", " << recipient.getLocationY() << ")\n";
        std::cout << "Capacity: " << recipient.getCapacity() << " meals\n";
        std::cout << "Accepted Food: " << recipient.getAcceptedFoodTypes() << "\n";
        std::cout << "Currently Accepting: " << (recipient.isAccepting() ? "Yes" : "No") << "\n";
        std::cout << "Pickup Capability: " << recipient.getPickupCapability() << "\n";
    }
    std::cout << "----------------------------------------\n";
}

void updateRecipientMenu(DataManager& dataManager) {
    std::cout << "\nEnter Recipient ID to update: ";
    std::string id = getStringInput();

    // Search for recipient (Demonstrates simple linear search through vector)
    Recipient* recipient = dataManager.getRecipientById(id);
    if (recipient == nullptr) {
        std::cout << "Error: Recipient ID '" << id << "' not found.\n";
        return;
    }

    bool updating = true;
    while (updating) {
        std::cout << "\n--- UPDATE RECIPIENT: " << recipient->getName() << " ---\n";
        std::cout << "1. Update available capacity\n";
        std::cout << "2. Update accepting status\n";
        std::cout << "3. Update pickup capability\n";
        std::cout << "4. Back\n";
        std::cout << "Choose an option: ";

        int choice = getIntInput();

        switch (choice) {
            case 1: {
                std::cout << "Enter new capacity (cannot be negative): ";
                int newCap = getIntInput();
                if (newCap < 0) {
                    std::cout << "Error: Capacity cannot be negative.\n";
                } else {
                    recipient->setCapacity(newCap);
                    std::cout << "Capacity updated to " << newCap << ".\n";
                }
                break;
            }
            case 2: {
                std::cout << "Is recipient currently accepting? (1 for Yes, 2 for No): ";
                int statusChoice = getIntInput();
                if (statusChoice == 1) {
                    recipient->setAccepting(true);
                    std::cout << "Status updated to Yes.\n";
                } else if (statusChoice == 2) {
                    recipient->setAccepting(false);
                    std::cout << "Status updated to No.\n";
                } else {
                    std::cout << "Error: Invalid choice.\n";
                }
                break;
            }
            case 3: {
                std::cout << "Does recipient have pickup capability? (1 for Yes, 2 for No): ";
                int pickupChoice = getIntInput();
                if (pickupChoice == 1) {
                    recipient->setPickupCapability("Yes");
                    std::cout << "Pickup capability updated to Yes.\n";
                } else if (pickupChoice == 2) {
                    recipient->setPickupCapability("No");
                    std::cout << "Pickup capability updated to No.\n";
                } else {
                    std::cout << "Error: Invalid choice.\n";
                }
                break;
            }
            case 4:
                updating = false;
                break;
            default:
                std::cout << "Invalid option. Please try again.\n";
        }
    }
}

int main() {
    DataManager dataManager;
    
    std::cout << "Loading data from CSV files...\n";
    bool donorsLoaded = dataManager.loadDonors("data/donors.csv");
    bool recipientsLoaded = dataManager.loadRecipients("data/recipients.csv");

    if (!donorsLoaded || !recipientsLoaded) {
        std::cerr << "\nCritical Error: Failed to load core datasets. Please check the 'data/' directory.\n";
    }

    bool running = true;
    while (running) {
        std::cout << "\n========================================\n";
        std::cout << "      FOOD RESCUE SYSTEM\n";
        std::cout << "========================================\n";
        std::cout << "1. View Donors\n";
        std::cout << "2. View Recipients\n";
        std::cout << "3. Update Recipient\n";
        std::cout << "4. Create Donation\n";
        std::cout << "5. Exit\n";
        std::cout << "Choose an option: ";

        int choice = getIntInput();

        switch (choice) {
            case 1:
                displayDonors(dataManager);
                break;
            case 2:
                displayRecipients(dataManager);
                break;
            case 3:
                updateRecipientMenu(dataManager);
                break;
            case 4:
                std::cout << "\nDonation creation will be implemented in Phase 4.\n";
                break;
            case 5:
                running = false;
                std::cout << "Exiting program. Goodbye!\n";
                break;
            default:
                std::cout << "Invalid option. Please choose between 1 and 5.\n";
        }
    }

    return 0;
}
