// main.cpp: process entry of cb_engine. stdin/stdout carry only protocol lines (UTF-8 JSON, one per line);
// diagnostics go to stderr as JSON lines. Usage: cb_engine --data <folder> [--protocol 1] [--log-level info|debug]
// [--self-test] [--version]. Exit codes: 0 clean, 2 boot failed, 64 usage, 70 internal / out of memory.
// Boot and per-line dispatch live in Session.cpp (shared with the WebAssembly build in engine-wasm/).

#include "Engine.h"
#include "Session.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <string>

using cbjson::JVal;

static void writeLine(FILE* out, const std::string& line)
{
    std::string text = line;
    text += '\n';
    std::fwrite(text.data(), 1, text.size(), out);
    std::fflush(out);
}

static void logLine(const char* level, const std::string& msg)
{
    JVal o = JVal::object();
    o.set("level", JVal::str(level));
    o.set("msg", JVal::str(msg));
    writeLine(stderr, o.dump());
}

int main(int argc, char** argv)
{
    _setmode(0, _O_BINARY);   // stdin, stdout, stderr: no newline rewriting on Windows
    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);

    std::string data;
    bool selfTest = false;
    for (int k = 1; k < argc; k++)
    {
        std::string arg = argv[k];
        if (arg == "--data" && k + 1 < argc) data = argv[++k];
        else if (arg == "--protocol" && k + 1 < argc) { std::string v = argv[++k]; if (v != "1") { logLine("error", "only protocol 1 is supported"); return 64; } }
        else if (arg == "--log-level" && k + 1 < argc) ++k;
        else if (arg == "--self-test") selfTest = true;
        else if (arg == "--version") { std::printf("cb_engine 0.1.0 protocol 1\n"); return 0; }
        else { logLine("error", "usage: cb_engine --data <folder> [--protocol 1] [--log-level info|debug] [--self-test] [--version]"); return 64; }
    }
    if (data.empty()) { logLine("error", "usage: cb_engine --data <folder> [--protocol 1] [--self-test] [--version]"); return 64; }

    cb::Engine* engine = NULL;
    std::string first = cb::bootEngine(data, engine);
    if (engine == NULL)
    {
        writeLine(stdout, first);
        return 2;
    }

    if (selfTest)
    {
        JVal report = engine->selfTest();
        writeLine(stdout, report.dump());
        bool ok = report.get("ok") != NULL && report.get("ok")->asBool();
        delete engine;
        return ok ? 0 : 1;
    }

    writeLine(stdout, first);

    int exitCode = 0;
    std::string line;
    bool tooLong = false;
    bool shutdown = false;
    while (!shutdown)
    {
        int c = std::getc(stdin);
        if (c == EOF) break;                       // the gateway went away: exit quietly
        if (c != '\n')
        {
            if (line.size() < cb::MAX_LINE) line += (char)c;
            else tooLong = true;
            continue;
        }
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (tooLong)
        {
            writeLine(stdout, cb::tooLongLine(engine->rev_));
            line.clear();
            tooLong = false;
            continue;
        }
        if (line.empty()) continue;

        bool fatal = false;
        std::string reply = cb::handleSafe(*engine, line, shutdown, fatal);
        writeLine(stdout, reply);
        line.clear();
        if (fatal) { exitCode = 70; break; }
    }

    delete engine;
    return exitCode;
}
