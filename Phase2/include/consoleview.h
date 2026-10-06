#ifndef CONSOLEVIEW_H
#define CONSOLEVIEW_H

#include "allocationmanager.h"
#include "datamanager.h"
#include "recommendationengine.h"
#include "traveltime.h"
#include <iostream>
#include <list>
#include <string>

using namespace std;

// One answer of the coordinator while a donation is processed. It is either a candidate
// number with a quantity (for example 1 40), or the letter c to close the donation.
class coordinatorchoice
{
public:
    // Starts as an answer that was not understood.
    coordinatorchoice();

    // Checks whether the last line that was read could be understood.
    bool valid() const;

    // Checks whether the coordinator typed c to close the donation.
    bool isclose() const;

    // Returns the candidate number as typed (the first candidate is 1).
    int candidatenumber() const;

    // Returns the quantity as typed.
    int quantity() const;

    // Reads one line. The stream fails only when the input has ended. A line that cannot
    // be understood sets valid() to false so the caller can ask again.
    friend istream& operator>>(istream& in, coordinatorchoice& choice);

private:
    // True when the line was understood
    bool valid_;
    // True when the line was c
    bool close_;
    // The candidate number
    int candidate_;
    // The quantity
    int quantity_;
};

// The menu and the screens of the console tool. It reads from one stream and writes to
// another, so the tests can feed scripted answers and read what was printed. It never
// decides for the coordinator: every allocation or close needs an answer typed in.
class consoleview
{
public:
    // Remembers the input and output streams.
    consoleview(istream& in, ostream& out);

    // Explains what the tool does. States that the edge weights are simulated.
    void showabout();

    // Prints the menu.
    void showmenu();

    // Asks for a menu number from 0 to 6 until it gets one. Returns -1 when the input has
    // ended.
    int readmenu();

    // Prints the warnings that were found while loading.
    void showwarnings(const list<string>& warnings);

    // Prints the donations table.
    void showdonations(const dataset& data);

    // Prints the recipients table.
    void showrecipients(const dataset& data);

    // Prints the transaction log.
    void showtransactions(const dataset& data);

    // Processes one donation with the coordinator (Spec 7.4): rank the recipients, show
    // them, read the choice, allocate, save, and repeat until nothing is left or the
    // donation is closed. Returns false if the input ended before the donation was done.
    bool processdonation(dataset& data, datamanager& files,
                         const dijkstratraveltime& provider, int index);

    // Asks for a new donation, gives it the next DON number and saves it. Returns the
    // index of the new donation, or -1 if the coordinator cancelled or the input ended.
    int adddonation(dataset& data, datamanager& files);

private:
    // Where answers are read from
    istream& in_;
    // Where text is printed
    ostream& out_;

    // Prints a prompt and reads one trimmed line. Returns false when the input ended.
    bool ask(const string& prompt, string& answer);

    // Asks for a choice until the line is understood. Returns false when input ended.
    bool readchoice(coordinatorchoice& choice);

    // Prints the line that describes the donation being processed.
    void showdonation(const dataset& data, const Donation& donation, int source);

    // Prints the ranked candidates with their routes, then the rejected recipients.
    void showrecommendation(const dataset& data, const recommendationresult& found,
                            const dijkstratraveltime& provider, int source);
};

#endif