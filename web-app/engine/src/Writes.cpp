// Writes.cpp: the commands that change data (each one calls exactly one desktop app workflow function), the
// offer pre-check, and the classifier that turns desktop app message strings into protocol error codes.

#include "Engine.h"

#include "Exceptions.h"
#include "RecipientUpdate.h"
#include "RecommendationEngine.h"
#include "TextTools.h"
#include "Workflow.h"
#include "allocationmanager.h"

#include <sstream>

using cbjson::JVal;

namespace cb
{

static bool startsWith(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool endsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool contains(const std::string& s, const std::string& part)
{
    return s.find(part) != std::string::npos;
}

static bool allDigits(const std::string& s)
{
    if (s.empty() || s.size() > 9) return false;
    for (size_t k = 0; k < s.size(); k++) if (s[k] < '0' || s[k] > '9') return false;
    return true;
}

// ------------------------------------------------------------------ classifier

struct Known
{
    const char* message;
    const char* code;
};

// Exact desktop app messages (Workflow.cpp, RecipientUpdate.cpp). engine/tests checks every literal of those files.
static const Known KNOWN[] = {
    {"That offer could not be found", "OFFER_NOT_FOUND"},
    {"That donation could not be found", "DONATION_NOT_FOUND"},
    {"That donor could not be found", "DONOR_NOT_FOUND"},
    {"That recipient could not be found", "RECIPIENT_NOT_FOUND"},
    {"There is no donation at this position", "DONATION_NOT_FOUND"},
    {"The donation or the recipient of this offer could not be found", "DATA_INVALID"},
    {"The donor of this donation is not in the files", "DATA_INVALID"},
    {"This offer has already been answered", "OFFER_ALREADY_ANSWERED"},
    {"Only an offer that is still waiting can be withdrawn", "OFFER_ALREADY_ANSWERED"},
    {"This donation is no longer open", "DONATION_NOT_OPEN"},
    {"This donation is already finished so there is nothing to close", "DONATION_NOT_OPEN"},
    {"Please choose one of the recipients in the list", "NOT_A_CANDIDATE"},
    {"The name must be 1 to 60 characters and cannot contain a comma", "INVALID_NAME"},
    {"The type must be 1 to 30 characters long", "INVALID_TYPE"},
    {"The type can only have letters and digits and spaces and underscores", "INVALID_TYPE"},
    {"Please choose a location from the map", "INVALID_NODE"},
    {"Please choose at least one kind of food", "NO_FOOD_TYPES"},
    {"Choose at least one food type", "NO_FOOD_TYPES"},   // desktop app updaterecipientdetails (added after the engine)
    {"The capacity cannot be below 0", "INVALID_CAPACITY"},
    {"The quantity must be at least 1", "BAD_QUANTITY"},
    {"The usable minutes must be at least 1", "INVALID_MINUTES"},
};

CmdError classifyMessage(const std::string& message)
{
    // numbers in the text
    const std::string rangePrefix = "The quantity must be from 1 to ";
    if (startsWith(message, rangePrefix) && allDigits(message.substr(rangePrefix.size())))
    {
        JVal d = JVal::object();
        d.set("min", JVal::integer(1));
        d.set("max", JVal::integer(std::atoll(message.substr(rangePrefix.size()).c_str())));
        return CmdError("QUANTITY_OUT_OF_RANGE", message, d);
    }
    const std::string leftSuffix = " is left to offer for this donation";
    if (startsWith(message, "Only ") && endsWith(message, leftSuffix))
    {
        std::string num = message.substr(5, message.size() - 5 - leftSuffix.size());
        if (allDigits(num))
        {
            JVal d = JVal::object();
            d.set("max", JVal::integer(std::atoll(num.c_str())));
            return CmdError("EXCEEDS_REMAINING", message, d);
        }
    }

    for (size_t k = 0; k < sizeof(KNOWN) / sizeof(KNOWN[0]); k++)
    {
        if (message == KNOWN[k].message) return CmdError(KNOWN[k].code, message);
    }

    // a save that failed: desktop app always says that nothing was changed or added
    if (contains(message, "nothing was changed") || contains(message, "nothing was added") ||
        (contains(message, " was saved but the donation could not be saved")))
    {
        JVal d = JVal::object();
        d.set("retryable", JVal::boolean(true));
        return CmdError("SAVE_FAILED", message, d);
    }

    // the allocation refusals of console app, in the plain form that acceptoffer returns (plaintext of allocerrormessage)
    static const AllocError codes[] = {DONATION_NOT_OPEN, BAD_QUANTITY, EXCEEDS_REMAINING, EXCEEDS_CAPACITY, NOT_FEASIBLE};
    static const char* names[] = {"DONATION_NOT_OPEN", "BAD_QUANTITY", "EXCEEDS_REMAINING", "EXCEEDS_CAPACITY", "NOT_FEASIBLE"};
    for (size_t k = 0; k < sizeof(codes) / sizeof(codes[0]); k++)
    {
        if (startsWith(message, plaintext(allocerrormessage(codes[k]))))
        {
            JVal d = JVal::object();
            if (contains(message, "so the offer was withdrawn")) d.set("offerWithdrawn", JVal::boolean(true));
            return CmdError(names[k], message, d);
        }
    }

    return CmdError("OPERATION_REFUSED", message);
}

// Throws the classified refusal for a failed backend call.
static void refuse(const std::string& message)
{
    throw classifyMessage(message);
}

// Names and types come from the web: the backend only forbids a comma and a line break, but any control
// character would damage a CSV row, so the engine refuses them too.
static void checkNoControl(const std::string& text, const char* code, const char* message)
{
    for (size_t k = 0; k < text.size(); k++)
    {
        unsigned char c = (unsigned char)text[k];
        if (c < 0x20 || c == 0x7F) throw CmdError(code, message);
    }
}

static void checkName(const std::string& name)
{
    checkNoControl(name, "INVALID_NAME", "The name must be 1 to 60 characters and cannot contain a comma");
}

static void checkType(const std::string& type)
{
    checkNoControl(type, "INVALID_TYPE", "The type can only have letters and digits and spaces and underscores");
}

// ------------------------------------------------------------------ offers

void Engine::cmdOfferCheck(const JVal& a, Result& r)
{
    int pos = findOffer(argStr(a, "offerId"));
    if (pos < 0) throw CmdError("OFFER_NOT_FOUND", "That offer could not be found");
    const Offer& offer = app_->offers.at(pos);
    r.data.set("offerId", JVal::str(offer.id().toString()));
    if (offer.status() != OFFER_PENDING)
    {
        r.data.set("wouldSucceed", JVal::boolean(false));
        r.data.set("code", JVal::str("OFFER_ALREADY_ANSWERED"));
        r.data.set("message", JVal::str("This offer has already been answered"));
        return;
    }
    int dpos = findDonation(offer.donationId().toString());
    int rpos = findRecipient(offer.recipientId().toString());
    if (dpos < 0 || rpos < 0) throw CmdError("DATA_INVALID", "The donation or the recipient of this offer could not be found");

    Donation donation = app_->data.donations[dpos];   // copies: allocate() changes what it is given
    Recipient recipient = app_->data.recipients[rpos];
    AllocError code = allocate(donation, recipient, offer.quantity(), Duration(offer.travelMinutes()), EntityId());
    r.data.set("wouldSucceed", JVal::boolean(code == ALLOC_OK));
    if (code != ALLOC_OK)
    {
        CmdError c = classifyMessage(plaintext(allocerrormessage(code)));
        r.data.set("code", JVal::str(c.code));
        r.data.set("message", JVal::str(plaintext(allocerrormessage(code))));
        JVal d = JVal::object();
        if (code == EXCEEDS_CAPACITY) d.set("max", JVal::integer(app_->data.recipients[rpos].availableCapacity()));
        if (code == EXCEEDS_REMAINING) d.set("max", JVal::integer(app_->data.donations[dpos].remainingQuantity()));
        r.data.set("details", d);
    }
}

void Engine::cmdOfferSend(const JVal& a, Result& r)
{
    std::string donationId = argStr(a, "donationId");
    std::string recipientId = argStr(a, "recipientId");
    int qty = argInt(a, "quantity");

    int dpos = findDonation(donationId);
    if (dpos < 0) throw CmdError("DONATION_NOT_FOUND", "That donation could not be found");
    int rpos = findRecipient(recipientId);
    if (rpos < 0) throw CmdError("RECIPIENT_NOT_FOUND", "That recipient could not be found");
    if (!isOpen(dpos)) throw CmdError("DONATION_NOT_OPEN", "This donation is no longer open");

    matchview m = buildmatch(app_->data, app_->offers, *app_->provider, dpos);
    int number = 0;
    for (size_t k = 0; k < m.candidates.size(); k++)
    {
        if (m.candidates[k].recipientId().toString() == recipientId) number = (int)k + 1;
    }
    if (number == 0)
    {
        JVal d = JVal::object();
        std::string why = "NOTHING_LEFT_TO_OFFER";
        for (size_t k = 0; k < m.rejected.size(); k++)
        {
            if (m.rejected[k].recipientId().toString() == recipientId) why = reasonToStr(m.rejected[k].reason());
        }
        for (size_t k = 0; k < m.declined.size(); k++)
        {
            if (m.declined[k].toString() == recipientId) why = "DECLINED_EARLIER";
        }
        d.set("reason", JVal::str(why));
        throw CmdError("NOT_A_CANDIDATE", "That recipient cannot take this donation right now (" + why + ")", d);
    }

    std::string message;
    int pos = sendoffer(app_->data, app_->offers, *app_->files, m, number, qty, message);
    if (pos < 0) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("offer", offerJ(pos));
}

void Engine::cmdOfferAccept(const JVal& a, Result& r)
{
    int pos = findOffer(argStr(a, "offerId"));
    if (pos < 0) throw CmdError("OFFER_NOT_FOUND", "That offer could not be found");
    EntityId id = app_->offers.at(pos).id();
    int dpos = findDonation(app_->offers.at(pos).donationId().toString());
    int rpos = findRecipient(app_->offers.at(pos).recipientId().toString());

    std::string message;
    if (!acceptoffer(app_->data, app_->offers, *app_->files, id, message))
    {
        CmdError e = classifyMessage(message);
        if (e.code == "EXCEEDS_CAPACITY" && rpos >= 0) e.details.set("max", JVal::integer(app_->data.recipients[rpos].availableCapacity()));
        if (e.code == "EXCEEDS_REMAINING" && dpos >= 0) e.details.set("max", JVal::integer(app_->data.donations[dpos].remainingQuantity()));
        throw e;
    }
    r.message = message;
    r.hasMessage = true;
    r.data.set("offer", offerJ(pos));
    const Offer& offer = app_->offers.at(pos);
    if (offer.hastransaction())
    {
        int tpos = findTransaction(offer.transactionId().toString());
        if (tpos >= 0) r.data.set("transaction", transactionJ(tpos));
    }
}

void Engine::cmdOfferDecline(const JVal& a, Result& r)
{
    int pos = findOffer(argStr(a, "offerId"));
    if (pos < 0) throw CmdError("OFFER_NOT_FOUND", "That offer could not be found");
    std::string message;
    if (!declineoffer(app_->offers, *app_->files, EntityId(app_->offers.at(pos).id()), message)) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("offer", offerJ(pos));
}

void Engine::cmdOfferWithdraw(const JVal& a, Result& r)
{
    int pos = findOffer(argStr(a, "offerId"));
    if (pos < 0) throw CmdError("OFFER_NOT_FOUND", "That offer could not be found");
    std::string message;
    if (!withdrawoffer(app_->offers, *app_->files, EntityId(app_->offers.at(pos).id()), message)) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("offer", offerJ(pos));
}

// ------------------------------------------------------------------ donations, donors, recipients

void Engine::cmdDonationCreate(const JVal& a, Result& r)
{
    std::string donorId = argStr(a, "donorId");
    FoodType food = argFood(a, "food");
    int qty = argInt(a, "quantity");
    int usable = argInt(a, "usableMinutes");
    int dpos = findDonor(donorId);
    if (dpos < 0) throw CmdError("DONOR_NOT_FOUND", "That donor could not be found");

    std::string message;
    int pos = createdonation(app_->data, *app_->files, app_->data.donors[dpos].id(), food, qty, usable, message);
    if (pos < 0) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("donation", donationJ(pos));
}

void Engine::cmdDonationCreateNewDonor(const JVal& a, Result& r)
{
    std::string name = argStr(a, "donorName");
    std::string type = argStr(a, "donorType");
    int node = argInt(a, "node");
    FoodType food = argFood(a, "food");
    int qty = argInt(a, "quantity");
    int usable = argInt(a, "usableMinutes");
    checkName(name);
    checkType(type);

    std::string message;
    int pos = createdonationwithnewdonor(app_->data, *app_->files, name, type, node, food, qty, usable, message);
    if (pos < 0) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("donation", donationJ(pos));
    int dpos = findDonor(app_->data.donations[pos].donorId().toString());
    if (dpos >= 0) r.data.set("donor", donorJ(dpos));
}

void Engine::cmdDonationClose(const JVal& a, Result& r)
{
    int pos = findDonation(argStr(a, "donationId"));
    if (pos < 0) throw CmdError("DONATION_NOT_FOUND", "That donation could not be found");
    int wasted = app_->data.donations[pos].remainingQuantity();
    std::string message;
    if (!closedonationwithoffers(app_->data, app_->offers, *app_->files, pos, message)) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("donation", donationJ(pos));
    r.data.set("wasted", JVal::integer(wasted));
}

// "Donor DN013 added and no recipient is ..." -> the part after "added" is the warning
static void splitWarning(const std::string& message, const std::string& head, Result& r)
{
    size_t at = message.find(" and no ");
    (void)head;
    if (at != std::string::npos) r.data.set("warning", JVal::str(message.substr(at + 5)));
}

void Engine::cmdDonorCreate(const JVal& a, Result& r)
{
    std::string name = argStr(a, "name");
    std::string type = argStr(a, "type");
    int node = argInt(a, "node");
    checkName(name);
    checkType(type);
    std::string message;
    int pos = createdonor(app_->data, *app_->files, name, type, node, message);
    if (pos < 0) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("donor", donorJ(pos));
    splitWarning(message, "donor", r);
}

void Engine::cmdDonorValidate(const JVal& a, Result& r)
{
    std::string name = argStr(a, "name");
    std::string type = argStr(a, "type");
    int node = argInt(a, "node");
    checkName(name);
    checkType(type);
    std::string message;
    if (!checkdonordetails(app_->data, name, type, node, message)) refuse(message);
    r.data.set("ok", JVal::boolean(true));
}

void Engine::cmdRecipientCreate(const JVal& a, Result& r)
{
    std::string name = argStr(a, "name");
    std::string type = argStr(a, "type");
    int capacity = argInt(a, "capacity");
    bool accepting = argBool(a, "accepting");
    bool pickup = argBool(a, "pickup");
    int node = argInt(a, "node");
    const JVal& foodsArg = argReq(a, "foods");
    if (!foodsArg.isArr()) throw CmdError("BAD_ARGS", "Argument foods must be an array");
    std::vector<FoodType> foods;
    for (size_t k = 0; k < foodsArg.size(); k++)
    {
        JVal wrap = JVal::object();
        wrap.set("f", foodsArg.at(k));
        FoodType f = argFood(wrap, "f");
        bool seen = false;
        for (size_t j = 0; j < foods.size(); j++) if (foods[j] == f) seen = true;
        if (!seen) foods.push_back(f);
    }
    checkName(name);
    checkType(type);

    std::string message;
    int pos = createrecipient(app_->data, *app_->files, name, type, foods, capacity, accepting, pickup, node, message);
    if (pos < 0) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("recipient", recipientJ(pos));
    splitWarning(message, "recipient", r);
}

void Engine::cmdRecipientUpdate(const JVal& a, Result& r)
{
    int pos = findRecipient(argStr(a, "recipientId"));
    if (pos < 0) throw CmdError("RECIPIENT_NOT_FOUND", "That recipient could not be found");
    if (!argHas(a, "capacity") && !argHas(a, "accepting") && !argHas(a, "pickup"))
        throw CmdError("BAD_ARGS", "Give at least one of capacity, accepting or pickup");
    const Recipient& cur = app_->data.recipients[pos];
    EntityId rid = cur.id();   // a copy: updaterecipient replaces the element in the vector
    int capacity = argHas(a, "capacity") ? argInt(a, "capacity") : cur.availableCapacity();
    bool accepting = argHas(a, "accepting") ? argBool(a, "accepting") : cur.accepting();
    bool pickup = argHas(a, "pickup") ? argBool(a, "pickup") : cur.canPickup();
    std::string message;
    if (!updaterecipient(app_->data, *app_->files, rid, capacity, accepting, pickup, message)) refuse(message);
    r.message = message;
    r.hasMessage = true;
    r.data.set("recipient", recipientJ(pos));
}

}  // namespace cb
