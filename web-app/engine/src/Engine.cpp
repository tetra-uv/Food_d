#include "Engine.h"

#include "BackendManifest.generated.h"
#include "Dashboard.h"
#include "Datamanager.h"
#include "Exceptions.h"
#include "Offer.h"
#include "OfferStore.h"
#include "Queries.h"
#include "RecommendationEngine.h"
#include "Tables.h"
#include "TextTools.h"
#include "Workflow.h"
#include "allocationmanager.h"
#include "graph.h"
#include "traveltime.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <new>
#ifdef __EMSCRIPTEN__
#include <unistd.h>
#define CB_GETPID getpid
#else
#include <process.h>
#define CB_GETPID _getpid
#endif
#include <set>
#include <sstream>

using cbjson::JVal;

namespace cb
{

static const int PROTOCOL = 1;
static const char* ENGINE_VERSION = "0.1.0";

static std::string numtext(long long v)
{
    std::stringstream t;
    t << v;
    return t.str();
}

// ------------------------------------------------------------------ argument helpers

const JVal& argReq(const JVal& args, const char* key)
{
    const JVal* v = args.get(key);
    if (v == NULL)
    {
        JVal d = JVal::object();
        d.set("field", JVal::str(key));
        throw CmdError("BAD_ARGS", std::string("Missing argument ") + key, d);
    }
    return *v;
}

bool argHas(const JVal& args, const char* key)
{
    const JVal* v = args.get(key);
    return v != NULL && !v->isNull();
}

static CmdError badArg(const char* key, const char* what)
{
    JVal d = JVal::object();
    d.set("field", JVal::str(key));
    return CmdError("BAD_ARGS", std::string("Argument ") + key + " " + what, d);
}

std::string argStr(const JVal& args, const char* key)
{
    const JVal& v = argReq(args, key);
    if (!v.isStr()) throw badArg(key, "must be a string");
    return v.asStr();
}

int argInt(const JVal& args, const char* key)
{
    const JVal& v = argReq(args, key);
    if (!v.isInt()) throw badArg(key, "must be a whole number");
    if (v.asInt() > 2147483647LL || v.asInt() < -2147483647LL) throw badArg(key, "is out of range");
    return (int)v.asInt();
}

bool argBool(const JVal& args, const char* key)
{
    const JVal& v = argReq(args, key);
    if (!v.isBool()) throw badArg(key, "must be true or false");
    return v.asBool();
}

FoodType argFood(const JVal& args, const char* key)
{
    std::string s = argStr(args, key);
    if (s == "COOKED") return COOKED;
    if (s == "PACKAGED") return PACKAGED;
    if (s == "PRODUCE") return PRODUCE;
    if (s == "BAKERY") return BAKERY;
    throw badArg(key, "must be COOKED, PACKAGED, PRODUCE or BAKERY");
}

// ------------------------------------------------------------------ construction

Engine::Engine(const std::string& folder) : app_(NULL), rev_(0), shutdown_(false)
{
    std::string dir = folder;
    for (size_t k = 0; k < dir.size(); k++)
    {
        if (dir[k] == '\\') dir[k] = '/';
    }
    if (dir.empty() || dir[dir.size() - 1] != '/') dir += '/';
    dir_ = dir;

    app_ = new AppState(dir_);   // throws an AppException when the files are bad
    makeBootId();

    struct Row { const char* name; Handler fn; bool write; };
    static const Row rows[] = {
        {"sys.hello", &Engine::cmdHello, false},
        {"sys.ping", &Engine::cmdPing, false},
        {"sys.shutdown", &Engine::cmdShutdown, false},
        {"sys.echo", &Engine::cmdEcho, false},
        {"sys.classify", &Engine::cmdClassify, false},
        {"sys.reload", &Engine::cmdReload, false},
        {"read.snapshot", &Engine::cmdSnapshot, false},
        {"read.network", &Engine::cmdNetwork, false},
        {"read.queue", &Engine::cmdQueue, false},
        {"read.stats", &Engine::cmdStats, false},
        {"match.build", &Engine::cmdMatchBuild, false},
        {"match.preview", &Engine::cmdMatchPreview, false},
        {"match.compare", &Engine::cmdMatchCompare, false},
        {"net.field", &Engine::cmdNetField, false},
        {"net.route", &Engine::cmdNetRoute, false},
        {"offer.check", &Engine::cmdOfferCheck, false},
        {"offer.send", &Engine::cmdOfferSend, true},
        {"offer.accept", &Engine::cmdOfferAccept, true},
        {"offer.decline", &Engine::cmdOfferDecline, true},
        {"offer.withdraw", &Engine::cmdOfferWithdraw, true},
        {"donation.create", &Engine::cmdDonationCreate, true},
        {"donation.createWithNewDonor", &Engine::cmdDonationCreateNewDonor, true},
        {"donation.close", &Engine::cmdDonationClose, true},
        {"donor.create", &Engine::cmdDonorCreate, true},
        {"donor.validate", &Engine::cmdDonorValidate, false},
        {"recipient.create", &Engine::cmdRecipientCreate, true},
        {"recipient.update", &Engine::cmdRecipientUpdate, true},
    };
    for (size_t k = 0; k < sizeof(rows) / sizeof(rows[0]); k++)
    {
        Entry e;
        e.fn = rows[k].fn;
        e.write = rows[k].write;
        table_[rows[k].name] = e;
    }
}

Engine::~Engine()
{
    delete app_;
}

void Engine::makeBootId()
{
    unsigned long seed = (unsigned long)std::time(NULL) * 2654435761UL;
    seed ^= (unsigned long)std::clock() * 40503UL;
    seed ^= (unsigned long)CB_GETPID() << 7;
    static unsigned long boots = 0;          // distinct ids for several boots in the same process and second
    seed ^= (++boots) * 0x9E3779B9UL;
    int local = 0;
    seed ^= (unsigned long)(size_t)&local;
    std::srand((unsigned)seed);
    static const char hex[] = "0123456789abcdef";
    std::string id;
    for (int k = 0; k < 12; k++) id += hex[std::rand() % 16];
    bootId_ = id;
}

// Lets every id issuer continue after the highest id that is really in memory (like the start-up does).
void Engine::reseedIssuers()
{
    const dataset& d = app_->data;
    EntityId t("T", 0), dn("DON", 0), dr("DN", 0), r("R", 0), o("OFF", 0);
    for (size_t k = 0; k < d.transactions.size(); k++) if (t < d.transactions[k].id()) t = d.transactions[k].id();
    for (size_t k = 0; k < d.donations.size(); k++) if (dn < d.donations[k].id()) dn = d.donations[k].id();
    for (size_t k = 0; k < d.donors.size(); k++) if (dr < d.donors[k].id()) dr = d.donors[k].id();
    for (size_t k = 0; k < d.recipients.size(); k++) if (r < d.recipients[k].id()) r = d.recipients[k].id();
    for (int k = 0; k < app_->offers.count(); k++) if (o < app_->offers.at(k).id()) o = app_->offers.at(k).id();
    transactionidissuer::seed(t);
    donationidissuer::seed(dn);
    donoridissuer::seed(dr);
    recipientidissuer::seed(r);
    offeridissuer::seed(o);
}

// ------------------------------------------------------------------ lookups

int Engine::findDonation(const std::string& id) const
{
    int pos = 0;
    return app_->data.donationindex.get(id, pos) ? pos : -1;
}

int Engine::findRecipient(const std::string& id) const
{
    int pos = 0;
    return app_->data.recipientindex.get(id, pos) ? pos : -1;
}

int Engine::findDonor(const std::string& id) const
{
    int pos = 0;
    return app_->data.donorindex.get(id, pos) ? pos : -1;
}

int Engine::findOffer(const std::string& id) const
{
    for (int k = 0; k < app_->offers.count(); k++)
    {
        if (app_->offers.at(k).id().toString() == id) return k;
    }
    return -1;
}

int Engine::findTransaction(const std::string& id) const
{
    const std::vector<Transaction>& t = app_->data.transactions;
    for (size_t k = 0; k < t.size(); k++)
    {
        if (t[k].id().toString() == id) return (int)k;
    }
    return -1;
}

bool Engine::isOpen(int pos) const
{
    DonationStatus s = app_->data.donations[pos].status();
    return s == PENDING || s == PARTIAL;
}

// ------------------------------------------------------------------ codec

JVal Engine::donorJ(int pos) const
{
    const Donor& d = app_->data.donors[pos];
    JVal o = JVal::object();
    o.set("id", JVal::str(d.id().toString()));
    o.set("name", JVal::str(d.name()));
    o.set("type", JVal::str(d.type()));
    o.set("node", JVal::integer(d.nodeId()));
    return o;
}

JVal Engine::recipientJ(int pos) const
{
    const Recipient& r = app_->data.recipients[pos];
    JVal types = JVal::array();
    for (size_t k = 0; k < r.acceptedTypes().size(); k++) types.push(JVal::str(foodToStr(r.acceptedTypes()[k])));
    JVal o = JVal::object();
    o.set("id", JVal::str(r.id().toString()));
    o.set("name", JVal::str(r.name()));
    o.set("type", JVal::str(r.type()));
    o.set("availableCapacity", JVal::integer(r.availableCapacity()));
    o.set("acceptedTypes", types);
    o.set("accepting", JVal::boolean(r.accepting()));
    o.set("canPickup", JVal::boolean(r.canPickup()));
    o.set("node", JVal::integer(r.nodeId()));
    return o;
}

JVal Engine::donationJ(int pos) const
{
    const Donation& d = app_->data.donations[pos];
    JVal o = JVal::object();
    o.set("id", JVal::str(d.id().toString()));
    o.set("donorId", JVal::str(d.donorId().toString()));
    o.set("food", JVal::str(foodToStr(d.foodType())));
    o.set("original", JVal::integer(d.originalQuantity()));
    o.set("remaining", JVal::integer(d.remainingQuantity()));
    o.set("usableMinutes", JVal::integer(d.usableMinutes()));
    o.set("status", JVal::str(statusToStr(d.status())));
    o.set("pendingOffered", JVal::integer(app_->offers.pendingquantity(d.id())));
    o.set("effectiveRemaining", JVal::integer(isOpen(pos) ? effectiveremaining(app_->data, app_->offers, pos) : 0));
    return o;
}

JVal Engine::transactionJ(int pos) const
{
    const Transaction& t = app_->data.transactions[pos];
    JVal o = JVal::object();
    o.set("id", JVal::str(t.id().toString()));
    o.set("donationId", JVal::str(t.donationId().toString()));
    o.set("recipientId", JVal::str(t.recipientId().toString()));
    o.set("quantity", JVal::integer(t.quantity()));
    o.set("travelMinutes", JVal::integer(t.travelMinutes()));
    return o;
}

JVal Engine::offerJ(int pos) const
{
    const Offer& f = app_->offers.at(pos);
    JVal o = JVal::object();
    o.set("id", JVal::str(f.id().toString()));
    o.set("donationId", JVal::str(f.donationId().toString()));
    o.set("recipientId", JVal::str(f.recipientId().toString()));
    o.set("quantity", JVal::integer(f.quantity()));
    o.set("travelMinutes", JVal::integer(f.travelMinutes()));
    o.set("status", JVal::str(offerstatustostr(f.status())));
    if (f.hastransaction()) o.set("txnId", JVal::str(f.transactionId().toString()));
    else o.set("txnId", JVal::null());
    return o;
}

Snap Engine::capture() const
{
    Snap s;
    const dataset& d = app_->data;
    for (size_t k = 0; k < d.donors.size(); k++) s.donors[d.donors[k].id().toString()] = donorJ((int)k);
    for (size_t k = 0; k < d.recipients.size(); k++) s.recipients[d.recipients[k].id().toString()] = recipientJ((int)k);
    for (size_t k = 0; k < d.donations.size(); k++) s.donations[d.donations[k].id().toString()] = donationJ((int)k);
    for (size_t k = 0; k < d.transactions.size(); k++) s.transactions[d.transactions[k].id().toString()] = transactionJ((int)k);
    for (int k = 0; k < app_->offers.count(); k++) s.offers[app_->offers.at(k).id().toString()] = offerJ(k);
    return s;
}

static void diffKind(const std::map<std::string, JVal>& before, const std::map<std::string, JVal>& after, JVal& out)
{
    for (std::map<std::string, JVal>::const_iterator it = after.begin(); it != after.end(); ++it)
    {
        std::map<std::string, JVal>::const_iterator old = before.find(it->first);
        JVal entry = JVal::object();
        entry.set("id", JVal::str(it->first));
        if (old == before.end())
        {
            entry.set("after", it->second);
            out.push(entry);
            continue;
        }
        if (old->second == it->second) continue;
        JVal b = JVal::object(), a = JVal::object();
        const std::vector<std::pair<std::string, JVal> >& m = it->second.members();
        for (size_t k = 0; k < m.size(); k++)
        {
            const JVal* was = old->second.get(m[k].first);
            if (was == NULL || !(*was == m[k].second))
            {
                b.set(m[k].first, was == NULL ? JVal::null() : *was);
                a.set(m[k].first, m[k].second);
            }
        }
        entry.set("before", b);
        entry.set("after", a);
        out.push(entry);
    }
}

JVal Engine::diff(const Snap& before, const Snap& after) const
{
    JVal fx = JVal::object();
    JVal donors = JVal::array(), recipients = JVal::array(), donations = JVal::array(), transactions = JVal::array(),
         offers = JVal::array();
    diffKind(before.donors, after.donors, donors);
    diffKind(before.recipients, after.recipients, recipients);
    diffKind(before.donations, after.donations, donations);
    diffKind(before.transactions, after.transactions, transactions);
    diffKind(before.offers, after.offers, offers);
    fx.set("offers", offers);
    fx.set("donations", donations);
    fx.set("recipients", recipients);
    fx.set("transactions", transactions);
    fx.set("donors", donors);
    return fx;
}

static bool effectsEmpty(const JVal& fx)
{
    const std::vector<std::pair<std::string, JVal> >& m = fx.members();
    for (size_t k = 0; k < m.size(); k++)
    {
        if (m[k].second.size() > 0) return false;
    }
    return true;
}

// ------------------------------------------------------------------ dispatcher

static std::string errorLine(const JVal& id, long long rev, const CmdError& e)
{
    JVal err = JVal::object();
    err.set("code", JVal::str(e.code));
    err.set("message", JVal::str(e.message));
    err.set("details", e.details);
    JVal o = JVal::object();
    o.set("id", id);
    o.set("ok", JVal::boolean(false));
    o.set("rev", JVal::integer(rev));
    o.set("error", err);
    return o.dump();
}

// Called inside a catch block: turns the exception in flight into a CmdError.
static CmdError currentError(bool& fatal)
{
    try
    {
        throw;
    }
    catch (const CmdError& e)
    {
        return e;
    }
    catch (const DataException& e)
    {
        return CmdError("DATA_INVALID", e.what());
    }
    catch (const AppException& e)
    {
        CmdError c = classifyMessage(e.what());
        return c;
    }
    catch (const std::bad_alloc&)
    {
        fatal = true;
        return CmdError("INTERNAL", "The engine ran out of memory and will restart");
    }
    catch (const std::exception& e)
    {
        return CmdError("INTERNAL", std::string("Internal error: ") + e.what());
    }
    catch (...)
    {
        return CmdError("INTERNAL", "Internal error");
    }
}

std::string Engine::handleLine(const std::string& line, bool& shutdown, bool& fatal)
{
    JVal id = JVal::null();
    bool failed = false;
    CmdError err("INTERNAL", "");

    try
    {
        JVal req;
        std::string perr;
        if (!cbjson::parse(line, req, perr)) throw CmdError("BAD_REQUEST", "The line is not valid JSON: " + perr);
        if (!req.isObj()) throw CmdError("BAD_REQUEST", "A request must be a JSON object");

        const JVal* idv = req.get("id");
        if (idv != NULL && (idv->isInt() || idv->isStr())) id = *idv;

        const JVal* v = req.get("v");
        if (v != NULL && !(v->isInt() && v->asInt() == PROTOCOL))
        {
            throw CmdError("PROTOCOL_VERSION", "Only protocol version 1 is supported");
        }
        const JVal* cmd = req.get("cmd");
        if (cmd == NULL || !cmd->isStr()) throw CmdError("BAD_REQUEST", "A request needs a cmd string");
        JVal args = JVal::object();
        const JVal* a = req.get("args");
        if (a != NULL && !a->isNull())
        {
            if (!a->isObj()) throw CmdError("BAD_REQUEST", "args must be an object");
            args = *a;
        }

        std::map<std::string, Entry>::const_iterator it = table_.find(cmd->asStr());
        if (it == table_.end()) throw CmdError("UNKNOWN_COMMAND", "Unknown command " + cmd->asStr());

        Snap before;
        if (it->second.write) before = capture();

        Result r;
        try
        {
            (this->*(it->second.fn))(args, r);
        }
        catch (...)
        {
            err = currentError(fatal);
            failed = true;
        }

        if (failed)
        {
            if (it->second.write)
            {
                // a refusal can still have changed something (an offer withdrawn, a donor saved): report it
                try
                {
                    JVal fx = diff(before, capture());
                    if (!effectsEmpty(fx))
                    {
                        rev_++;
                        err.details.set("effects", fx);
                        err.details.set("stateChanged", JVal::boolean(true));
                    }
                }
                catch (...)
                {
                }
            }
            return errorLine(id, rev_, err);
        }

        JVal out = JVal::object();
        out.set("id", id);
        out.set("ok", JVal::boolean(true));
        if (it->second.write)
        {
            JVal fx = diff(before, capture());
            rev_++;
            out.set("rev", JVal::integer(rev_));
            if (r.hasMessage) out.set("message", JVal::str(r.message));
            out.set("data", r.data);
            out.set("effects", fx);
        }
        else
        {
            out.set("rev", JVal::integer(rev_));
            if (r.hasMessage) out.set("message", JVal::str(r.message));
            out.set("data", r.data);
        }
        if (cmd->asStr() == "sys.shutdown") shutdown = true;
        return out.dump();
    }
    catch (...)
    {
        err = currentError(fatal);
    }
    return errorLine(id, rev_, err);
}

// ------------------------------------------------------------------ sys.*

JVal Engine::helloData() const
{
    const dataset& d = app_->data;
    int edges = 0;
    for (int n = 0; n < d.network.nodecount(); n++)
    {
        for (const edgenode* e = d.network.neighbors(n); e != NULL; e = e->next)
        {
            if (n < e->to) edges++;
        }
    }
    JVal counts = JVal::object();
    counts.set("donors", JVal::integer((long long)d.donors.size()));
    counts.set("recipients", JVal::integer((long long)d.recipients.size()));
    counts.set("donations", JVal::integer((long long)d.donations.size()));
    counts.set("transactions", JVal::integer((long long)d.transactions.size()));
    counts.set("offers", JVal::integer(app_->offers.count()));
    counts.set("nodes", JVal::integer(d.network.nodecount()));
    counts.set("edges", JVal::integer(edges));
    JVal manifest = JVal::object();
    manifest.set("sha256", JVal::str(CB_BACKEND_MANIFEST_SHA256));
    manifest.set("files", JVal::integer(CB_BACKEND_MANIFEST_FILES));
    JVal o = JVal::object();
    o.set("protocol", JVal::integer(PROTOCOL));
    o.set("engineVersion", JVal::str(ENGINE_VERSION));
    o.set("bootId", JVal::str(bootId_));
    o.set("dataDir", JVal::str(dir_));
    o.set("counts", counts);
    o.set("rev", JVal::integer(rev_));
    o.set("backendManifest", manifest);
    return o;
}

static JVal warningsJ(const AppState* app)
{
    JVal w = JVal::array();
    std::vector<std::string> notes = plainnotes(app->warnings, app->data);
    for (size_t k = 0; k < notes.size(); k++) w.push(JVal::str(notes[k]));
    return w;
}

JVal Engine::readyEvent() const
{
    JVal o = JVal::object();
    o.set("event", JVal::str("ready"));
    o.set("protocol", JVal::integer(PROTOCOL));
    o.set("bootId", JVal::str(bootId_));
    o.set("rev", JVal::integer(rev_));
    o.set("engineVersion", JVal::str(ENGINE_VERSION));
    o.set("dataDir", JVal::str(dir_));
    o.set("warnings", warningsJ(app_));
    return o;
}

void Engine::cmdHello(const JVal&, Result& r) { r.data = helloData(); }

void Engine::cmdPing(const JVal&, Result& r) { r.data.set("pong", JVal::boolean(true)); }

void Engine::cmdShutdown(const JVal&, Result& r) { r.data.set("bye", JVal::boolean(true)); }

// debug: returns args unchanged (used to test the JSON layer end to end)
void Engine::cmdEcho(const JVal& a, Result& r) { r.data = a; }

// debug: shows the code a desktop app message is classified as (used by the classifier test)
void Engine::cmdClassify(const JVal& a, Result& r)
{
    CmdError e = classifyMessage(argStr(a, "message"));
    r.data.set("code", JVal::str(e.code));
    r.data.set("details", e.details);
}

void Engine::cmdReload(const JVal&, Result& r)
{
    AppState* fresh = NULL;
    try
    {
        fresh = new AppState(dir_);
    }
    catch (const AppException& e)
    {
        reseedIssuers();   // a failed start may have re-seeded the issuers from files that were not used
        throw CmdError("DATA_INVALID", plaintextkeepfiles(e.what()));
    }
    AppState* old = app_;
    app_ = fresh;
    delete old;
    makeBootId();
    rev_ = 0;
    r.data = helloData();
    r.data.set("warnings", warningsJ(app_));
}

JVal Engine::selfTest()
{
    JVal o = JVal::object();
    std::vector<std::string> problems = app_->files->checkinvariants(app_->data);
    JVal p = JVal::array();
    for (size_t k = 0; k < problems.size(); k++) p.push(JVal::str(problems[k]));
    int built = 0, open = 0;
    for (size_t k = 0; k < app_->data.donations.size(); k++)
    {
        if (!isOpen((int)k)) continue;
        open++;
        buildmatch(app_->data, app_->offers, *app_->provider, (int)k);
        built++;
    }
    o.set("ok", JVal::boolean(problems.empty()));
    o.set("invariantProblems", p);
    o.set("warnings", warningsJ(app_));
    o.set("openDonations", JVal::integer(open));
    o.set("matchesBuilt", JVal::integer(built));
    o.set("hello", helloData());
    return o;
}

// ------------------------------------------------------------------ read.*

void Engine::cmdSnapshot(const JVal&, Result& r)
{
    const dataset& d = app_->data;
    JVal donors = JVal::array(), recipients = JVal::array(), donations = JVal::array(), transactions = JVal::array(),
         offers = JVal::array();
    for (size_t k = 0; k < d.donors.size(); k++) donors.push(donorJ((int)k));
    for (size_t k = 0; k < d.recipients.size(); k++) recipients.push(recipientJ((int)k));
    for (size_t k = 0; k < d.donations.size(); k++) donations.push(donationJ((int)k));
    for (size_t k = 0; k < d.transactions.size(); k++) transactions.push(transactionJ((int)k));
    for (int k = 0; k < app_->offers.count(); k++) offers.push(offerJ(k));

    totals t = computetotals(d);
    JVal byFood = JVal::object();
    for (int k = 0; k < 4; k++) byFood.set(foodToStr((FoodType)k), JVal::integer(t.byfood[k]));
    JVal summary = JVal::object();
    summary.set("offered", JVal::integer(t.offered));
    summary.set("allocated", JVal::integer(t.allocated));
    summary.set("wasted", JVal::integer(t.wasted));
    summary.set("stillOpen", JVal::integer(t.stillopen));
    summary.set("byFood", byFood);

    coordinatorstats cs = coordinatorstatsof(d, app_->offers);
    JVal stats = JVal::object();
    stats.set("openDonations", JVal::integer(cs.opendonations));
    stats.set("waiting", JVal::integer(cs.waiting));
    stats.set("pendingOffers", JVal::integer(cs.pendingoffers));
    stats.set("givenOut", JVal::integer(cs.givenout));
    stats.set("wasted", JVal::integer(cs.wasted));

    r.data.set("donors", donors);
    r.data.set("recipients", recipients);
    r.data.set("donations", donations);
    r.data.set("transactions", transactions);
    r.data.set("offers", offers);
    r.data.set("summary", summary);
    r.data.set("coordinatorStats", stats);
    r.data.set("warnings", warningsJ(app_));
}

void Engine::cmdNetwork(const JVal&, Result& r)
{
    const dataset& d = app_->data;
    int n = d.network.nodecount();
    std::vector<int> label;
    int comps = labelcomponents(d.network, label);

    JVal nodes = JVal::array(), edges = JVal::array(), components = JVal::array();
    for (int k = 0; k < n; k++)
    {
        JVal donors = JVal::array(), recips = JVal::array();
        for (size_t j = 0; j < d.donors.size(); j++) if (d.donors[j].nodeId() == k) donors.push(JVal::str(d.donors[j].id().toString()));
        for (size_t j = 0; j < d.recipients.size(); j++) if (d.recipients[j].nodeId() == k) recips.push(JVal::str(d.recipients[j].id().toString()));
        JVal o = JVal::object();
        o.set("id", JVal::integer(k));
        o.set("label", JVal::str(k < (int)d.nodelabels.size() ? d.nodelabels[k] : std::string()));
        o.set("xKm", JVal::number(k < (int)d.nodexs.size() ? d.nodexs[k] : 0.0));
        o.set("yKm", JVal::number(k < (int)d.nodeys.size() ? d.nodeys[k] : 0.0));
        o.set("componentId", JVal::integer(label[k]));
        o.set("donors", donors);
        o.set("recipients", recips);
        nodes.push(o);
        for (const edgenode* e = d.network.neighbors(k); e != NULL; e = e->next)
        {
            if (k < e->to)
            {
                JVal ed = JVal::object();
                ed.set("u", JVal::integer(k));
                ed.set("v", JVal::integer(e->to));
                ed.set("minutes", JVal::integer(e->minutes));
                edges.push(ed);
            }
        }
    }
    for (int c = 0; c < comps; c++)
    {
        JVal ids = JVal::array();
        bool hasD = false, hasR = false;
        for (int k = 0; k < n; k++)
        {
            if (label[k] != c) continue;
            ids.push(JVal::integer(k));
        }
        for (size_t j = 0; j < d.donors.size(); j++) if (label[d.donors[j].nodeId()] == c) hasD = true;
        for (size_t j = 0; j < d.recipients.size(); j++) if (label[d.recipients[j].nodeId()] == c) hasR = true;
        JVal o = JVal::object();
        o.set("id", JVal::integer(c));
        o.set("nodeIds", ids);
        o.set("hasDonor", JVal::boolean(hasD));
        o.set("hasRecipient", JVal::boolean(hasR));
        components.push(o);
    }
    r.data.set("nodes", nodes);
    r.data.set("edges", edges);
    r.data.set("components", components);
}

void Engine::cmdQueue(const JVal&, Result& r)
{
    const dataset& d = app_->data;
    urgencyqueue waiting;
    for (size_t k = 0; k < d.donations.size(); k++)
    {
        if (isOpen((int)k) && effectiveremaining(d, app_->offers, (int)k) > 0) waiting.push(d.donations[k], (int)k);
    }
    JVal order = JVal::array();
    while (!waiting.empty())
    {
        int pos = waiting.popmosturgent();
        JVal o = JVal::object();
        o.set("donationId", JVal::str(d.donations[pos].id().toString()));
        o.set("usableMinutes", JVal::integer(d.donations[pos].usableMinutes()));
        o.set("effectiveRemaining", JVal::integer(effectiveremaining(d, app_->offers, pos)));
        order.push(o);
    }
    r.data.set("order", order);
    if (order.size() > 0) r.data.set("nextId", *order.at(0).get("donationId"));
    else r.data.set("nextId", JVal::null());
}

void Engine::cmdStats(const JVal& a, Result& r)
{
    std::string role = argStr(a, "role");
    const dataset& d = app_->data;
    if (role == "coordinator")
    {
        coordinatorstats cs = coordinatorstatsof(d, app_->offers);
        r.data.set("openDonations", JVal::integer(cs.opendonations));
        r.data.set("waiting", JVal::integer(cs.waiting));
        r.data.set("pendingOffers", JVal::integer(cs.pendingoffers));
        r.data.set("givenOut", JVal::integer(cs.givenout));
        r.data.set("wasted", JVal::integer(cs.wasted));
    }
    else if (role == "donor")
    {
        int pos = findDonor(argStr(a, "id"));
        if (pos < 0) throw CmdError("DONOR_NOT_FOUND", "That donor could not be found");
        donorstats s = donorstatsof(d, app_->offers, d.donors[pos].id());
        r.data.set("donations", JVal::integer(s.donations));
        r.data.set("openDonations", JVal::integer(s.opendonations));
        r.data.set("pendingOffers", JVal::integer(s.pendingoffers));
        r.data.set("givenOut", JVal::integer(s.givenout));
    }
    else if (role == "recipient")
    {
        int pos = findRecipient(argStr(a, "id"));
        if (pos < 0) throw CmdError("RECIPIENT_NOT_FOUND", "That recipient could not be found");
        recipientstats s = recipientstatsof(d, app_->offers, d.recipients[pos].id());
        r.data.set("incoming", JVal::integer(s.incoming));
        r.data.set("received", JVal::integer(s.received));
    }
    else throw CmdError("BAD_ARGS", "role must be coordinator, donor or recipient");
}

// ------------------------------------------------------------------ match.*

static int gateOf(Reason reason)
{
    switch (reason)
    {
    case NOT_ACCEPTING: return 1;
    case FOOD_INCOMPATIBLE: return 2;
    case NO_CAPACITY: return 3;
    case NO_PICKUP: return 4;
    case UNREACHABLE: return 5;
    case TOO_FAR: return 6;
    default: return 0;
    }
}

static JVal durationJ(const Duration& t)
{
    if (t.isInfinite()) return JVal::null();
    return JVal::integer(t.minutes());
}

JVal Engine::matchViewJ(const matchview& m, const Donation& donation, int donorNode, bool hypothetical) const
{
    const dataset& d = app_->data;
    std::vector<Duration> dist = app_->provider->traveltimesfrom(donorNode);

    // route of one recipient node as {nodes, legs}; the leg minutes are the differences of the shortest times
    struct RouteMaker
    {
        static JVal make(const dijkstratraveltime* provider, int from, int to, const std::vector<Duration>& dist)
        {
            std::vector<int> path = provider->routeto(from, to);
            if (path.empty()) return JVal::null();
            JVal nodes = JVal::array(), legs = JVal::array();
            for (size_t k = 0; k < path.size(); k++)
            {
                nodes.push(JVal::integer(path[k]));
                if (k + 1 < path.size())
                {
                    JVal leg = JVal::object();
                    leg.set("u", JVal::integer(path[k]));
                    leg.set("v", JVal::integer(path[k + 1]));
                    leg.set("minutes", JVal::integer(dist[path[k + 1]].minutes() - dist[path[k]].minutes()));
                    legs.push(leg);
                }
            }
            JVal o = JVal::object();
            o.set("nodes", nodes);
            o.set("legs", legs);
            return o;
        }
    };

    JVal candidates = JVal::array();
    for (size_t k = 0; k < m.candidates.size(); k++)
    {
        const Candidate& c = m.candidates[k];
        int rpos = findRecipient(c.recipientId().toString());
        int node = rpos >= 0 ? d.recipients[rpos].nodeId() : donorNode;
        int margin = donation.usableMinutes() - c.travel().minutes();
        JVal o = JVal::object();
        o.set("rank", JVal::integer((long long)k + 1));
        o.set("recipientId", JVal::str(c.recipientId().toString()));
        o.set("accept", JVal::integer(c.potentialAcceptance()));
        o.set("travelMinutes", JVal::integer(c.travel().minutes()));
        o.set("unusedCapacity", JVal::integer(c.unusedCapacity()));
        o.set("marginMinutes", JVal::integer(margin));
        o.set("onBoundary", JVal::boolean(margin == 0));
        if (k == 0) o.set("separatedFromPrevious", JVal::null());
        else
        {
            const Candidate& p = m.candidates[k - 1];
            std::string why = "ID";
            if (p.potentialAcceptance() != c.potentialAcceptance()) why = "ACCEPT";
            else if (!(p.travel() == c.travel())) why = "TRAVEL";
            else if (p.unusedCapacity() != c.unusedCapacity()) why = "UNUSED";
            o.set("separatedFromPrevious", JVal::str(why));
        }
        o.set("route", RouteMaker::make(app_->provider, donorNode, node, dist));
        std::string text;
        if (k < m.routes.size()) text = m.routes[k];
        else
        {
            std::vector<int> path = app_->provider->routeto(donorNode, node);
            for (size_t j = 0; j < path.size(); j++)
            {
                if (j > 0) text += " -> ";
                text += (path[j] >= 0 && path[j] < (int)d.nodelabels.size()) ? d.nodelabels[path[j]] : numtext(path[j]);
            }
        }
        o.set("routeText", JVal::str(text));
        candidates.push(o);
    }

    int total = (int)d.recipients.size();
    int counts[7] = {0, 0, 0, 0, 0, 0, 0};
    JVal rejected = JVal::array();
    for (size_t k = 0; k < m.rejected.size(); k++)
    {
        const Rejection& rj = m.rejected[k];
        int rpos = findRecipient(rj.recipientId().toString());
        int node = rpos >= 0 ? d.recipients[rpos].nodeId() : donorNode;
        JVal o = JVal::object();
        o.set("recipientId", JVal::str(rj.recipientId().toString()));
        o.set("reason", JVal::str(reasonToStr(rj.reason())));
        o.set("gate", JVal::integer(gateOf(rj.reason())));
        o.set("text", JVal::str(reasontext(rj.reason())));
        o.set("travelMinutes", durationJ(rj.travel()));
        o.set("reachable", JVal::boolean(!rj.travel().isInfinite()));
        if (rj.reason() == TOO_FAR) o.set("overByMinutes", JVal::integer(rj.travel().minutes() - donation.usableMinutes()));
        if (rj.travel().isInfinite()) o.set("route", JVal::null());
        else o.set("route", RouteMaker::make(app_->provider, donorNode, node, dist));
        rejected.push(o);
        counts[gateOf(rj.reason())]++;
    }

    JVal gates = JVal::array();
    static const Reason order[6] = {NOT_ACCEPTING, FOOD_INCOMPATIBLE, NO_CAPACITY, NO_PICKUP, UNREACHABLE, TOO_FAR};
    int entering = total;
    for (int g = 1; g <= 6; g++)
    {
        JVal o = JVal::object();
        o.set("gate", JVal::integer(g));
        o.set("reason", JVal::str(reasonToStr(order[g - 1])));
        o.set("entering", JVal::integer(entering));
        o.set("rejected", JVal::integer(counts[g]));
        gates.push(o);
        entering -= counts[g];
    }

    JVal declined = JVal::array();
    for (size_t k = 0; k < m.declined.size(); k++)
    {
        int rpos = findRecipient(m.declined[k].toString());
        JVal o = JVal::object();
        o.set("recipientId", JVal::str(m.declined[k].toString()));
        o.set("travelMinutes", rpos >= 0 ? durationJ(dist[d.recipients[rpos].nodeId()]) : JVal::null());
        declined.push(o);
    }

    JVal don = JVal::object();
    don.set("id", JVal::str(hypothetical ? std::string("PREVIEW") : donation.id().toString()));
    don.set("food", JVal::str(foodToStr(donation.foodType())));
    don.set("original", JVal::integer(donation.originalQuantity()));
    don.set("remaining", JVal::integer(hypothetical ? donation.remainingQuantity() : donation.remainingQuantity()));
    don.set("effectiveRemaining", JVal::integer(m.effectiveremaining));
    don.set("pendingOffered", JVal::integer(m.pendingoffers));
    don.set("usableMinutes", JVal::integer(donation.usableMinutes()));
    don.set("status", JVal::str(statusToStr(donation.status())));
    don.set("donorId", hypothetical && donation.donorId().toString() == EntityId("DN", 0).toString()
                           ? JVal::null() : JVal::str(donation.donorId().toString()));
    don.set("donorNode", JVal::integer(donorNode));
    don.set("hypothetical", JVal::boolean(hypothetical));

    JVal totalsJ = JVal::object();
    totalsJ.set("recipients", JVal::integer(total));
    totalsJ.set("feasible", JVal::integer((long long)m.candidates.size()));
    totalsJ.set("rejected", JVal::integer((long long)m.rejected.size()));
    totalsJ.set("declinedEarlier", JVal::integer((long long)m.declined.size()));

    JVal keys = JVal::array();
    keys.push(JVal::str("ACCEPT_DESC")).push(JVal::str("TRAVEL_ASC")).push(JVal::str("UNUSED_ASC")).push(JVal::str("ID_ASC"));

    JVal o = JVal::object();
    o.set("donation", don);
    o.set("totals", totalsJ);
    o.set("candidates", candidates);
    o.set("rejected", rejected);
    o.set("declined", declined);
    o.set("gates", gates);
    o.set("survivors", JVal::integer(entering));
    o.set("rankingKeys", keys);
    return o;
}

void Engine::cmdMatchBuild(const JVal& a, Result& r)
{
    int pos = findDonation(argStr(a, "donationId"));
    if (pos < 0) throw CmdError("DONATION_NOT_FOUND", "That donation could not be found");
    matchview m = buildmatch(app_->data, app_->offers, *app_->provider, pos);
    int donorPos = findDonor(app_->data.donations[pos].donorId().toString());
    int node = app_->data.donors[donorPos].nodeId();
    r.data = matchViewJ(m, app_->data.donations[pos], node, false);
}

// Resolves the donor node of a preview: donorId wins over node.
static int previewNode(const Engine& e, const JVal& a, std::string& donorId)
{
    if (argHas(a, "donorId"))
    {
        donorId = argStr(a, "donorId");
        int pos = e.findDonor(donorId);
        if (pos < 0) throw CmdError("DONOR_NOT_FOUND", "That donor could not be found");
        return e.app_->data.donors[pos].nodeId();
    }
    if (argHas(a, "node"))
    {
        int node = argInt(a, "node");
        if (node < 0 || node >= e.app_->data.network.nodecount()) throw CmdError("NODE_NOT_FOUND", "That location is not on the map");
        return node;
    }
    throw CmdError("BAD_ARGS", "Give donorId or node");
}

void Engine::cmdMatchPreview(const JVal& a, Result& r)
{
    std::string donorId;
    int node = previewNode(*this, a, donorId);
    FoodType food = argFood(a, "food");
    int qty = argInt(a, "quantity");
    int usable = argInt(a, "usableMinutes");
    if (qty < 1) throw CmdError("BAD_QUANTITY", "The quantity must be at least 1");
    if (usable < 1) throw CmdError("INVALID_MINUTES", "The usable minutes must be at least 1");

    EntityId donor = donorId.empty() ? EntityId("DN", 0) : app_->data.donors[findDonor(donorId)].id();
    Donation temp(EntityId("DON", 0), donor, food, qty, qty, usable, PENDING);
    std::vector<Duration> travel = app_->provider->traveltimesfrom(node);
    RecommendationResult res = recommend(temp, app_->data.recipients, travel);

    matchview m;
    m.donation = temp.id();
    m.effectiveremaining = qty;
    m.pendingoffers = 0;
    m.rejected = res.rejected();
    for (size_t k = 0; k < res.candidates().size(); k++)
    {
        if (res.candidates()[k].potentialAcceptance() >= 1) m.candidates.push_back(res.candidates()[k]);
    }
    r.data = matchViewJ(m, temp, node, true);
}

static JVal providerResultJ(const RecommendationResult& res)
{
    JVal cands = JVal::array(), rej = JVal::array();
    for (size_t k = 0; k < res.candidates().size(); k++)
    {
        const Candidate& c = res.candidates()[k];
        if (c.potentialAcceptance() < 1) continue;
        JVal o = JVal::object();
        o.set("recipientId", JVal::str(c.recipientId().toString()));
        o.set("accept", JVal::integer(c.potentialAcceptance()));
        o.set("travelMinutes", JVal::integer(c.travel().minutes()));
        o.set("unusedCapacity", JVal::integer(c.unusedCapacity()));
        cands.push(o);
    }
    for (size_t k = 0; k < res.rejected().size(); k++)
    {
        JVal o = JVal::object();
        o.set("recipientId", JVal::str(res.rejected()[k].recipientId().toString()));
        o.set("reason", JVal::str(reasonToStr(res.rejected()[k].reason())));
        rej.push(o);
    }
    JVal o = JVal::object();
    o.set("candidates", cands);
    o.set("rejected", rej);
    return o;
}

static std::set<std::string> candidateSet(const RecommendationResult& res)
{
    std::set<std::string> s;
    for (size_t k = 0; k < res.candidates().size(); k++)
    {
        if (res.candidates()[k].potentialAcceptance() >= 1) s.insert(res.candidates()[k].recipientId().toString());
    }
    return s;
}

static JVal compareJ(const std::set<std::string>& truth, const std::set<std::string>& other)
{
    JVal fp = JVal::array(), fn = JVal::array();
    for (std::set<std::string>::const_iterator it = other.begin(); it != other.end(); ++it)
        if (truth.find(*it) == truth.end()) fp.push(JVal::str(*it));
    for (std::set<std::string>::const_iterator it = truth.begin(); it != truth.end(); ++it)
        if (other.find(*it) == other.end()) fn.push(JVal::str(*it));
    JVal o = JVal::object();
    o.set("falsePositives", fp);
    o.set("falseNegatives", fn);
    return o;
}

void Engine::cmdMatchCompare(const JVal& a, Result& r)
{
    int pos = findDonation(argStr(a, "donationId"));
    if (pos < 0) throw CmdError("DONATION_NOT_FOUND", "That donation could not be found");
    const dataset& d = app_->data;
    const Donation& real = d.donations[pos];
    int effective = isOpen(pos) ? effectiveremaining(d, app_->offers, pos) : 0;
    Donation copy(real.id(), real.donorId(), real.foodType(), real.originalQuantity(), effective, real.usableMinutes(), real.status());
    int node = d.donors[findDonor(real.donorId().toString())].nodeId();

    straightlinetraveltime line(d.nodexs, d.nodeys, 0.5);
    notravelcheck none((int)d.network.nodecount());
    RecommendationResult byDijkstra = recommend(copy, d.recipients, app_->provider->traveltimesfrom(node));
    RecommendationResult byLine = recommend(copy, d.recipients, line.traveltimesfrom(node));
    RecommendationResult byNone = recommend(copy, d.recipients, none.traveltimesfrom(node));

    r.data.set("donationId", JVal::str(real.id().toString()));
    r.data.set("dijkstra", providerResultJ(byDijkstra));
    r.data.set("straightline", providerResultJ(byLine));
    r.data.set("none", providerResultJ(byNone));
    std::set<std::string> truth = candidateSet(byDijkstra);
    JVal diffs = JVal::object();
    diffs.set("straightline", compareJ(truth, candidateSet(byLine)));
    diffs.set("none", compareJ(truth, candidateSet(byNone)));
    r.data.set("diff", diffs);
}

// ------------------------------------------------------------------ net.*

void Engine::cmdNetField(const JVal& a, Result& r)
{
    int source = argInt(a, "source");
    const graph& net = app_->data.network;
    if (source < 0 || source >= net.nodecount()) throw CmdError("NODE_NOT_FOUND", "That location is not on the map");
    std::vector<Duration> dist;
    std::vector<int> prev;
    dijkstra(net, source, dist, prev);
    JVal jd = JVal::array(), jp = JVal::array(), order = JVal::array();
    std::vector<std::pair<int, int> > reach;
    for (size_t k = 0; k < dist.size(); k++)
    {
        jd.push(durationJ(dist[k]));
        jp.push(JVal::integer(prev[k]));
        if (!dist[k].isInfinite()) reach.push_back(std::make_pair(dist[k].minutes(), (int)k));
    }
    std::sort(reach.begin(), reach.end());
    for (size_t k = 0; k < reach.size(); k++) order.push(JVal::integer(reach[k].second));
    r.data.set("source", JVal::integer(source));
    r.data.set("dist", jd);
    r.data.set("prev", jp);
    r.data.set("order", order);
    r.data.set("reachableCount", JVal::integer((long long)reach.size()));
}

void Engine::cmdNetRoute(const JVal& a, Result& r)
{
    int from = argInt(a, "from");
    int to = argInt(a, "to");
    int n = app_->data.network.nodecount();
    if (from < 0 || from >= n || to < 0 || to >= n) throw CmdError("NODE_NOT_FOUND", "That location is not on the map");
    std::vector<Duration> dist = app_->provider->traveltimesfrom(from);
    std::vector<int> path = app_->provider->routeto(from, to);
    JVal nodes = JVal::array(), legs = JVal::array();
    for (size_t k = 0; k < path.size(); k++)
    {
        nodes.push(JVal::integer(path[k]));
        if (k + 1 < path.size())
        {
            JVal leg = JVal::object();
            leg.set("u", JVal::integer(path[k]));
            leg.set("v", JVal::integer(path[k + 1]));
            leg.set("minutes", JVal::integer(dist[path[k + 1]].minutes() - dist[path[k]].minutes()));
            legs.push(leg);
        }
    }
    bool reachable = !dist[to].isInfinite();
    r.data.set("from", JVal::integer(from));
    r.data.set("to", JVal::integer(to));
    r.data.set("reachable", JVal::boolean(reachable));
    r.data.set("minutes", durationJ(dist[to]));
    r.data.set("nodes", reachable ? nodes : JVal::array());
    r.data.set("legs", reachable ? legs : JVal::array());
}

}  // namespace cb
