#ifndef CB_ENGINE_H
#define CB_ENGINE_H
// Engine.h: the bridge between the JSON-lines protocol (doc 04) and the frozen console app / desktop app backend.
// It holds no business rule: it decodes arguments, calls backend functions, encodes the answers, classifies
// refusals and computes the effects diff of every write. Single threaded (the toolchain has no threads).

#include "AppState.h"
#include "Json.h"
#include "Match.h"

#include <map>
#include <string>
#include <vector>

namespace cb
{

using cbjson::JVal;

// A refusal with a protocol error code. Thrown by command handlers, caught by the dispatcher.
struct CmdError
{
    CmdError(const std::string& c, const std::string& m) : code(c), message(m), details(JVal::object()) {}
    CmdError(const std::string& c, const std::string& m, const JVal& d) : code(c), message(m), details(d) {}
    std::string code;
    std::string message;
    JVal details;
};

// What a handler returns.
struct Result
{
    Result() : data(JVal::object()), message(), hasMessage(false) {}
    JVal data;
    std::string message;
    bool hasMessage;
};

// Turns a desktop app refusal message into a protocol error code (+ numbers in details). Unknown -> OPERATION_REFUSED.
CmdError classifyMessage(const std::string& message);

// Entities by id, used for the before/after diff of a write.
struct Snap
{
    std::map<std::string, JVal> donors, recipients, donations, transactions, offers;
};

class Engine
{
public:
    // Boots like the console app and the desktop app do. Throws an AppException when the data files are bad.
    Engine(const std::string& folder);
    ~Engine();

    // Handles one request line (without the newline) and returns the response line (without the newline).
    // shutdown is set to true after sys.shutdown. fatal is set when the process should exit with code 70.
    std::string handleLine(const std::string& line, bool& shutdown, bool& fatal);

    JVal readyEvent() const;
    JVal selfTest();

    const std::string& bootId() const { return bootId_; }

    // ---- state used by the handlers (public so the handler functions in Writes.cpp can reach it)
    AppState* app_;
    std::string dir_;       // data folder with a trailing slash
    std::string bootId_;
    long long rev_;

    // ---- codec
    JVal donorJ(int pos) const;
    JVal recipientJ(int pos) const;
    JVal donationJ(int pos) const;
    JVal transactionJ(int pos) const;
    JVal offerJ(int pos) const;
    JVal matchViewJ(const matchview& m, const Donation& donation, int donorNode, bool hypothetical) const;
    Snap capture() const;
    JVal diff(const Snap& before, const Snap& after) const;

    // ---- lookups (positions or -1)
    int findDonation(const std::string& id) const;
    int findRecipient(const std::string& id) const;
    int findDonor(const std::string& id) const;
    int findOffer(const std::string& id) const;
    int findTransaction(const std::string& id) const;
    bool isOpen(int donationPos) const;

    // ---- commands
    void cmdHello(const JVal& a, Result& r);
    void cmdPing(const JVal& a, Result& r);
    void cmdShutdown(const JVal& a, Result& r);
    void cmdEcho(const JVal& a, Result& r);
    void cmdClassify(const JVal& a, Result& r);
    void cmdReload(const JVal& a, Result& r);
    void cmdSnapshot(const JVal& a, Result& r);
    void cmdNetwork(const JVal& a, Result& r);
    void cmdQueue(const JVal& a, Result& r);
    void cmdStats(const JVal& a, Result& r);
    void cmdMatchBuild(const JVal& a, Result& r);
    void cmdMatchPreview(const JVal& a, Result& r);
    void cmdMatchCompare(const JVal& a, Result& r);
    void cmdNetField(const JVal& a, Result& r);
    void cmdNetRoute(const JVal& a, Result& r);
    void cmdOfferCheck(const JVal& a, Result& r);
    void cmdOfferSend(const JVal& a, Result& r);
    void cmdOfferAccept(const JVal& a, Result& r);
    void cmdOfferDecline(const JVal& a, Result& r);
    void cmdOfferWithdraw(const JVal& a, Result& r);
    void cmdDonationCreate(const JVal& a, Result& r);
    void cmdDonationCreateNewDonor(const JVal& a, Result& r);
    void cmdDonationClose(const JVal& a, Result& r);
    void cmdDonorCreate(const JVal& a, Result& r);
    void cmdDonorValidate(const JVal& a, Result& r);
    void cmdRecipientCreate(const JVal& a, Result& r);
    void cmdRecipientUpdate(const JVal& a, Result& r);

private:
    Engine(const Engine&);
    Engine& operator=(const Engine&);

    JVal helloData() const;
    void reseedIssuers();
    void makeBootId();

    typedef void (Engine::*Handler)(const JVal&, Result&);
    struct Entry { Handler fn; bool write; };
    std::map<std::string, Entry> table_;
    bool shutdown_;
};

// ---- argument helpers (throw CmdError BAD_ARGS)
const JVal& argReq(const JVal& args, const char* key);
std::string argStr(const JVal& args, const char* key);
int argInt(const JVal& args, const char* key);
bool argBool(const JVal& args, const char* key);
bool argHas(const JVal& args, const char* key);
FoodType argFood(const JVal& args, const char* key);

}  // namespace cb

#endif
