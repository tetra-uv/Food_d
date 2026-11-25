#ifndef CB_SESSION_H
#define CB_SESSION_H
// Session.h: platform-neutral boot + one-line dispatch, shared by the native process entry (main.cpp) and the
// WebAssembly entry (engine-wasm/src/wasm_main.cpp). No I/O here: callers move the lines in and out.

#include "Engine.h"

#include <string>

namespace cb
{

// Maximum request line in bytes (without the newline). Longer lines get LINE_TOO_LONG.
static const size_t MAX_LINE = 1024 * 1024;

// Boots an engine on folder. On success out is the new engine and the ready event line is returned.
// On failure out is NULL and the boot_failed event line is returned.
std::string bootEngine(const std::string& folder, Engine*& out);

// The LINE_TOO_LONG error line.
std::string tooLongLine(long long rev);

// Handles one request line (no newline, no trailing CR) and never throws. Sets shutdown after sys.shutdown and
// fatal when the engine must not be used any more (out of memory).
std::string handleSafe(Engine& engine, const std::string& line, bool& shutdown, bool& fatal);

}  // namespace cb

#endif
