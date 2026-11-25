# cb_engine: JSON-lines bridge over the frozen console app + desktop app backend (no backend file is changed or copied).
cmake_minimum_required(VERSION 3.16)
project(cb_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)            # GCC 6 maps this to -std=c++1z, same as desktop-app/CMakeLists.txt
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra>")

get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

file(GLOB P2 CONFIGURE_DEPENDS "${REPO}/console-app/src/*.cpp")
list(FILTER P2 EXCLUDE REGEX "/(ConsoleView|main)\.cpp$")

file(GLOB P3 CONFIGURE_DEPENDS "${REPO}/desktop-app/src/*.cpp")
# AppState.cpp is KEPT (GUI-free start-up sequence); the extern theapp is never referenced by the engine
list(FILTER P3 EXCLUDE REGEX "/(Gui[^/]*|Page[^/]*|Theme|gui_main)\.cpp$")

file(GLOB ENGINE_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(cb_engine ${P2} ${P3} ${ENGINE_SRC})
target_include_directories(cb_engine PRIVATE "${REPO}/console-app/include" "${REPO}/desktop-app/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_options(cb_engine PRIVATE -static -static-libgcc -static-libstdc++)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
target_link_options(cb_engine PRIVATE "$<$<CONFIG:Release>:-s>")
