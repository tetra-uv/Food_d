#include "../include/DataManager.h"
#include <iostream>
#include <fstream>
#include <sstream>

DataManager::DataManager() {
    nextDonationId = 1;
}

bool DataManager::loadDonors(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }

    std::string line;
    // Skip the header line
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string id, name, locX_str, locY_str, avail;

        // String parsing using std::stringstream
        if (std::getline(ss, id, ',') &&
            std::getline(ss, name, ',') &&
            std::getline(ss, locX_str, ',') &&
            std::getline(ss, locY_str, ',') &&
            std::getline(ss, avail, ',')) {
            
            try {
                // String-to-integer conversion using std::stoi
                int x = std::stoi(locX_str);
                int y = std::stoi(locY_str);
                donors.push_back(Donor(id, name, x, y, avail));
            } catch (const std::exception& e) {
                std::cerr << "Warning: Malformed row in " << filename << ". Skipping line: " << line << std::endl;
            }
        }
    }
    return true;
}

bool DataManager::loadRecipients(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }

    std::string line;
    // Skip the header line
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string id, name, type, locX_str, locY_str, cap_str, accepting_str, foodTypes, pickupCap;

        // String parsing using std::stringstream
        if (std::getline(ss, id, ',') &&
            std::getline(ss, name, ',') &&
            std::getline(ss, type, ',') &&
            std::getline(ss, locX_str, ',') &&
            std::getline(ss, locY_str, ',') &&
            std::getline(ss, cap_str, ',') &&
            std::getline(ss, accepting_str, ',') &&
            std::getline(ss, foodTypes, ',') &&
            std::getline(ss, pickupCap, ',')) {
            
            try {
                // String-to-integer conversion using std::stoi
                int x = std::stoi(locX_str);
                int y = std::stoi(locY_str);
                int capacity = std::stoi(cap_str);
                bool accepting = (accepting_str == "1");

                recipients.push_back(Recipient(id, name, type, x, y, capacity, accepting, foodTypes, pickupCap));
            } catch (const std::exception& e) {
                std::cerr << "Warning: Malformed row in " << filename << ". Skipping line: " << line << std::endl;
            }
        }
    }
    return true;
}

std::string DataManager::createDonation(std::string donorID, int quantity, std::string foodType, int usableTime) {
    // Simple unique ID generator for Phase 1
    std::string newId = "DON" + std::to_string(nextDonationId++);
    
    Donation newDonation(newId, donorID, quantity, foodType, usableTime);
    donations.push_back(newDonation);
    
    return newId;
}

Recipient* DataManager::getRecipientById(const std::string& id) {
    for (size_t i = 0; i < recipients.size(); i++) {
        if (recipients[i].getRecipientID() == id) {
            return &recipients[i];
        }
    }
    return nullptr;
}
