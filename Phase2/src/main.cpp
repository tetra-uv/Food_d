#include "Exceptions.h"
#include "allocationmanager.h"
#include "consoleview.h"
#include "datamanager.h"
#include "traveltime.h"
#include <iostream>
#include <list>
#include <string>

using namespace std;

// Menu and orchestration only. All logic lives in the other files.
int main(int argc, char* argv[])
{
    // Run from the repository root. A different data folder prefix can be given.
    string prefix = "Phase2/data/";

    if (argc > 1)
    {
        prefix = argv[1];
    }

    consoleview view(cin, cout);

    try
    {
        datamanager files(prefix);
        dataset data;
        list<string> warnings;

        files.loadall(data, warnings);
        transactionidissuer::seed(data.lasttransaction);

        dijkstratraveltime provider(data.network);
        urgencyqueue waiting;
        fillurgencyqueue(data.donations, waiting);

        cout << "CommunityBridge: location-aware travel-time feasibility\n";
        cout << "(travel times are simulated minutes, not real road data)\n";
        view.showwarnings(warnings);

        bool running = true;

        while (running)
        {
            view.showmenu();
            int choice = view.readmenu();

            if (choice == 1)
            {
                if (waiting.empty())
                {
                    cout << "\nNo donation is waiting.\n";
                }
                else
                {
                    int index = waiting.popmosturgent();
                    running = view.processdonation(data, files, provider, index);
                }
            }
            else if (choice == 2)
            {
                int index = view.adddonation(data, files);

                if (index >= 0)
                {
                    waiting.push(data.donations[index], index);
                }
            }
            else if (choice == 3)
            {
                view.showdonations(data);
            }
            else if (choice == 4)
            {
                view.showrecipients(data);
            }
            else if (choice == 5)
            {
                view.showtransactions(data);
            }
            else if (choice == 6)
            {
                view.showabout();
            }
            else
            {
                running = false;
            }
        }
    }
    catch (const AppException& problem)
    {
        cout << "\nError: " << problem.what() << "\n";
        return 1;
    }

    cout << "\nGoodbye.\n";
    return 0;
}