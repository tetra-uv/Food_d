// Session.cpp: platform-neutral boot + one-line dispatch (see Session.h). Moved out of main.cpp so the native exe
// and the WebAssembly build share exactly the same code path.

#include "Session.h"
#include "Exceptions.h"
#include "TextTools.h"

#include <new>

namespace cb
{

static JVal errorObject(const char* code, const std::string& message)
{
    JVal e = JVal::object();
    e.set("code", JVal::str(code));
    e.set("message", JVal::str(message));
    e.set("details", JVal::object());
    return e;
}

static std::string bootFailed(const char* code, const std::string& message)
{
    JVal ev = JVal::object();
    ev.set("event", JVal::str("boot_failed"));
    ev.set("error", errorObject(code, message));
    return ev.dump();
}

std::string bootEngine(const std::string& folder, Engine*& out)
{
    out = NULL;
    try
    {
        Engine* engine = new Engine(folder);
        std::string ready = engine->readyEvent().dump();
        out = engine;
        return ready;
    }
    catch (const AppException& e)
    {
        return bootFailed("DATA_INVALID", plaintextkeepfiles(e.what()));
    }
    catch (const std::bad_alloc&)
    {
        return bootFailed("INTERNAL", "Out of memory");
    }
    catch (const std::exception& e)
    {
        return bootFailed("INTERNAL", e.what());
    }
    catch (...)
    {
        return bootFailed("INTERNAL", "Internal error");
    }
}

static std::string errorLine(long long rev, const char* code, const std::string& message)
{
    JVal o = JVal::object();
    o.set("id", JVal::null());
    o.set("ok", JVal::boolean(false));
    o.set("rev", JVal::integer(rev));
    o.set("error", errorObject(code, message));
    return o.dump();
}

std::string tooLongLine(long long rev)
{
    return errorLine(rev, "LINE_TOO_LONG", "A request line may have at most 1 MiB");
}

std::string handleSafe(Engine& engine, const std::string& line, bool& shutdown, bool& fatal)
{
    try
    {
        return engine.handleLine(line, shutdown, fatal);
    }
    catch (const std::bad_alloc&)
    {
        fatal = true;
        return errorLine(engine.rev_, "INTERNAL", "Out of memory");
    }
    catch (...)
    {
        return errorLine(engine.rev_, "INTERNAL", "Internal error");
    }
}

}  // namespace cb
