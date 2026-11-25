#ifndef CONSOLE_VIEW_H
#define CONSOLE_VIEW_H

#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "allocationmanager.h"
#include "Datamanager.h"
#include "Exceptions.h"
#include "RecommendationEngine.h"
#include "traveltime.h"

class CoordinatorChoice {
public:
    CoordinatorChoice();
    bool valid() const { return valid_; }
    bool isclose() const { return close_; }
    bool close() const { return close_; }
    int candidatenumber() const { return candidateNumber_; }
    int candidateNumber() const { return candidateNumber_; }
    int quantity() const { return quantity_; }

    friend std::istream& operator>>(std::istream& in, CoordinatorChoice& choice);
private:
    bool valid_;
    bool close_;
    int candidateNumber_;
    int quantity_;
};

using coordinatorchoice = CoordinatorChoice;

class ConsoleView {
public:
    static void showCandidates(const std::vector<Candidate>& candidates);
    static void showCandidates(const std::vector<Candidate>& candidates,
                               const std::vector<std::string>& routes);
    static void showRejected(const std::vector<Rejection>& rejected);
    static void showNoCandidates();
    static void showAllocationError(AllocError error);
    static void showDonationClosed();
    static void showAllocationSuccess(const EntityId& recipientId, int quantity);
    static void showWarnings(const std::list<std::string>& warnings);
    static void showMenu();
    static int readMenu();
    static bool processDonation(dataset& data, datamanager& files,
                                const dijkstratraveltime& provider, int index);
    static int addDonation(dataset& data, datamanager& files);
    static void addRecipient(dataset& data, datamanager& files);
    static void showDonations(const dataset& data);
    static void showRecipients(const dataset& data);
    static void showTransactions(const dataset& data);
    static void showNetwork(const dataset& data);
    static void showTravelTimesFromDonor(const dataset& data, const dijkstratraveltime& provider);
    static void showSummary(const dataset& data);
};

using consoleview = ConsoleView;

#endif
