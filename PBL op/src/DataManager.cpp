#include "../include/DataManager.h"
#include <iostream>

DataManager::DataManager() {
    nextDonationId = 1;
}

void DataManager::loadSampleData() {
    // Load 3 Donors (ID, Name, LocX, LocY, Pickup Availability)
    donors.push_back(Donor("D001", "City Hotel", 10, 20, "10:00-22:00"));
    donors.push_back(Donor("D002", "Bella Restaurant", 15, 25, "12:00-23:00"));
    donors.push_back(Donor("D003", "Wedding Caterers", 30, 40, "08:00-20:00"));

    // Load 4 Recipients 
    // (ID, Name, Type, LocX, LocY, Capacity, Accepting, FoodTypes, PickupCapability)
    recipients.push_back(Recipient("R001", "Hope NGO", "NGO", 12, 22, 140, true, "Veg", "Van"));
    recipients.push_back(Recipient("R002", "Downtown Shelter", "Shelter", 18, 30, 250, true, "Veg,Non-Veg", "Truck"));
    recipients.push_back(Recipient("R003", "Community Kitchen", "Kitchen", 35, 45, 300, false, "Raw", "None"));
    recipients.push_back(Recipient("R004", "Food Bank East", "FoodBank", 11, 25, 400, true, "All", "Van"));

    // Create 2 initial sample donations
    createDonation("D001", 250, "Veg", 120); // 120 minutes usable time remaining
    createDonation("D003", 100, "Raw", 240); // 240 minutes usable time remaining
}

std::string DataManager::createDonation(std::string donorID, int quantity, std::string foodType, int usableTime) {
    // Simple unique ID generator for Phase 1
    std::string newId = "DON" + std::to_string(nextDonationId++);
    
    Donation newDonation(newId, donorID, quantity, foodType, usableTime);
    donations.push_back(newDonation);
    
    return newId;
}
