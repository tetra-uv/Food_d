// RecipientUpdate.cpp: the edit of a recipient without any change to console app.
// Recipient has no setters, so a replacement with the same id, name, type, food types and
// location is built and put in the same place of the vector (decision D5).

#include "RecipientUpdate.h"
#include "Exceptions.h"

using namespace std;

// Builds the replacement, saves the whole recipients table and undoes the change if the save fails.
bool updaterecipient(dataset& data, datamanager& files, const EntityId& recipient, int capacity,
                     bool accepting, bool pickup, string& message)
{
    int pos = 0;

    if (!data.recipientindex.get(recipient.toString(), pos))
    {
        message = "That recipient could not be found";
        return false;
    }

    if (capacity < 0)
    {
        message = "The capacity cannot be below 0";
        return false;
    }

    Recipient old = data.recipients[pos];
    data.recipients[pos] = Recipient(old.id(), old.name(), old.type(), capacity, old.acceptedTypes(),
                                     accepting, pickup, old.nodeId());

    try
    {
        // the id is already in the table, so the table is rewritten from memory
        files.appendrecipient(data, data.recipients[pos]);
    }
    catch (const AppException&)
    {
        data.recipients[pos] = old;
        message = "Your details could not be saved so nothing was changed and you can try again";
        return false;
    }

    message = "Your details were saved";
    return true;
}

// The same replacement and save as updaterecipient, and the food types as well: at least one, stored once each and in the
// order of the food types (COOKED, PACKAGED, PRODUCE, BAKERY), so the saved text is always the same for the same choice.
bool updaterecipientdetails(dataset& data, datamanager& files, const EntityId& recipient, int capacity, bool accepting,
                            bool pickup, const vector<FoodType>& types, string& message)
{
    int pos = 0;

    if (!data.recipientindex.get(recipient.toString(), pos))
    {
        message = "That recipient could not be found";
        return false;
    }

    if (capacity < 0)
    {
        message = "The capacity cannot be below 0";
        return false;
    }

    static const FoodType order[4] = {COOKED, PACKAGED, PRODUCE, BAKERY};
    vector<FoodType> chosen;

    for (int kind = 0; kind < 4; kind++)
    {
        bool wanted = false;

        for (size_t at = 0; at < types.size(); at++)
        {
            wanted = wanted || types[at] == order[kind];
        }

        if (wanted)
        {
            chosen.push_back(order[kind]);
        }
    }

    if (chosen.empty())
    {
        message = "Choose at least one food type";
        return false;
    }

    Recipient old = data.recipients[pos];
    data.recipients[pos] = Recipient(old.id(), old.name(), old.type(), capacity, chosen, accepting, pickup, old.nodeId());

    try
    {
        // the id is already in the table, so the table is rewritten from memory
        files.appendrecipient(data, data.recipients[pos]);
    }
    catch (const AppException&)
    {
        data.recipients[pos] = old;
        message = "Your details could not be saved so nothing was changed and you can try again";
        return false;
    }

    message = "Your details were saved";
    return true;
}
