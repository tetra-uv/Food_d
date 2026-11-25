# cb_engine WebAssembly build: same backend + engine sources as ../engine, entry src/wasm_main.cpp instead of main.cpp.
# Configure only through build.ps1 (emcmake). Output: dist/cb_engine.mjs + dist/cb_engine.wasm
cmake_minimum_required(VERSION 3.16)
project(cb_engine_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(ENGINE "${CMAKE_CURRENT_SOURCE_DIR}/../engine")

file(GLOB P2 CONFIGURE_DEPENDS "${REPO}/console-app/src/*.cpp")
list(FILTER P2 EXCLUDE REGEX "/(ConsoleView|main)\\.cpp$")
file(GLOB P3 CONFIGURE_DEPENDS "${REPO}/desktop-app/src/*.cpp")
list(FILTER P3 EXCLUDE REGEX "/(Gui[^/]*|Page[^/]*|Theme|gui_main)\\.cpp$")
file(GLOB ENGINE_SRC CONFIGURE_DEPENDS "${ENGINE}/src/*.cpp")
list(FILTER ENGINE_SRC EXCLUDE REGEX "/main\\.cpp$")

set(CB_OPT "-O2" CACHE STRING "optimisation flag (-O2 or -Os)")
set(CB_EH "-fwasm-exceptions" CACHE STRING "exception mode (-fwasm-exceptions or -fexceptions)")

add_executable(cb_engine ${P2} ${P3} ${ENGINE_SRC} "${CMAKE_CURRENT_SOURCE_DIR}/src/wasm_main.cpp")
set_target_properties(cb_engine PROPERTIES SUFFIX ".mjs" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist")
target_include_directories(cb_engine PRIVATE "${REPO}/console-app/include" "${REPO}/desktop-app/include" "${ENGINE}/src")
target_compile_options(cb_engine PRIVATE ${CB_OPT} ${CB_EH} -Wall -Wextra -Wno-unused-parameter)
target_link_options(cb_engine PRIVATE ${CB_OPT} ${CB_EH}
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_ES6=1"
    "SHELL:-s EXPORT_NAME=createCbEngine"
    "SHELL:-s ENVIRONMENT=web,worker,node"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s STACK_SIZE=2097152"
    "SHELL:-s FILESYSTEM=1"
    "SHELL:-s FORCE_FILESYSTEM=1"
    "SHELL:-s EXIT_RUNTIME=0"
    "SHELL:-s INVOKE_RUN=0"
    "SHELL:-s EXPORTED_FUNCTIONS=['_cb_init','_cb_command','_cb_command_n','_cb_rev','_malloc','_free']"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['FS','ccall','cwrap','UTF8ToString','stringToUTF8','lengthBytesUTF8','HEAPU8']"
)
