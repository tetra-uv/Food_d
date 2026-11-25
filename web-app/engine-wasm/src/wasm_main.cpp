// wasm_main.cpp: WebAssembly entry of the CommunityBridge engine. Same backend, same Engine/Session code as the
// native cb_engine.exe; only the transport differs (function calls instead of stdin/stdout).
//
// C API (exported, see engine-wasm/README.md):
//   const char* cb_init(const char* dataDir)      boots (or re-boots) on a folder in the Emscripten FS and returns
//                                                  the first event line: {"event":"ready",...} or {"event":"boot_failed",...}
//   const char* cb_command(const char* request)   one protocol request line (doc 04) in, the response line out.
//                                                  Returns "" for an empty line (the native engine ignores those).
//   const char* cb_command_n(const char* p, int n) same, for raw bytes of length n (may contain any byte but NUL).
// Every returned pointer is owned by the module and stays valid until the next cb_* call. Copy it (UTF8ToString).
// After sys.shutdown or a fatal error the engine is released; further commands answer NOT_BOOTED until cb_init.

#include "Engine.h"
#include "Session.h"

#include <cstring>
#include <string>

#include <emscripten/emscripten.h>

namespace
{
cb::Engine* g_engine = NULL;
std::string g_out;

const char* keep(const std::string& s)
{
    g_out = s;
    return g_out.c_str();
}

const char* notBooted()
{
    return keep("{\"id\":null,\"ok\":false,\"rev\":0,\"error\":{\"code\":\"NOT_BOOTED\",\"message\":"
                "\"The engine is not running; call cb_init first\",\"details\":{}}}");
}

const char* run(const char* p, size_t n)
{
    if (n > 0 && p[n - 1] == '\n') n--;                  // a trailing newline is allowed
    if (g_engine == NULL) return notBooted();
    if (n > cb::MAX_LINE) return keep(cb::tooLongLine(g_engine->rev_));
    std::string line(p, n);
    if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
    if (line.empty()) return keep("");
    bool shutdown = false, fatal = false;
    std::string reply = cb::handleSafe(*g_engine, line, shutdown, fatal);
    if (shutdown || fatal)
    {
        delete g_engine;
        g_engine = NULL;
    }
    return keep(reply);
}
}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE const char* cb_init(const char* dataDir)
{
    if (g_engine != NULL)
    {
        delete g_engine;
        g_engine = NULL;
    }
    std::string dir = dataDir ? dataDir : "";
    if (dir.empty()) return keep("{\"event\":\"boot_failed\",\"error\":{\"code\":\"DATA_INVALID\",\"message\":\"No data folder given\",\"details\":{}}}");
    return keep(cb::bootEngine(dir, g_engine));
}

EMSCRIPTEN_KEEPALIVE const char* cb_command(const char* request)
{
    if (request == NULL) return run("", 0);
    return run(request, std::strlen(request));
}

EMSCRIPTEN_KEEPALIVE const char* cb_command_n(const char* p, int n)
{
    if (p == NULL || n < 0) return run("", 0);
    return run(p, (size_t)n);
}

EMSCRIPTEN_KEEPALIVE int cb_rev()
{
    return g_engine ? (int)g_engine->rev_ : -1;
}

}  // extern "C"
