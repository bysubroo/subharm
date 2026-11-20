cmake_minimum_required (VERSION 3.22)
project (SubHarm VERSION 0.5.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)
if (MSVC)
    add_compile_options (/utf-8)
endif()
if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

option (SUBHARM_BUILD_PLUGIN "Build the SubHarm plugin (needs JUCE)" ON)
option (SUBHARM_BUILD_TESTS  "Build the offline core test" ON)

include (FetchContent)

# ---------------- WORLD vocoder (modified BSD) ----------------
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/World/src)
    set (WORLD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/World)
else()
    FetchContent_Declare (world
        GIT_REPOSITORY https://github.com/mmorise/World.git
        GIT_TAG c794856b537758a3d300757db2e5e31685a822c2)
    FetchContent_GetProperties (world)
    if (NOT world_POPULATED)
        FetchContent_Populate (world)
    endif()
    set (WORLD_DIR ${world_SOURCE_DIR})
endif()

add_library (world STATIC
    ${WORLD_DIR}/src/cheaptrick.cpp ${WORLD_DIR}/src/common.cpp ${WORLD_DIR}/src/d4c.cpp
    ${WORLD_DIR}/src/dio.cpp ${WORLD_DIR}/src/fft.cpp ${WORLD_DIR}/src/harvest.cpp
    ${WORLD_DIR}/src/matlabfunctions.cpp ${WORLD_DIR}/src/stonemask.cpp
    ${WORLD_DIR}/src/synthesis.cpp ${WORLD_DIR}/src/codec.cpp)
# Seeded copy of WORLD's Synthesis(): the original reseeds its noise generator to the
# same value on every call, so every rendered voice got identical breath/aperiodic
# noise -> correlated voices -> phasing. SynthesisSeeded() takes a per-voice seed.
file (READ ${WORLD_DIR}/src/synthesis.cpp SUBHARM_SYN)
string (FIND "${SUBHARM_SYN}" "void Synthesis(const double *f0, int f0_length," SUBHARM_SYN_FN)
string (FIND "${SUBHARM_SYN}" "randn_reseed(&randn_state);" SUBHARM_SYN_RS)
if (SUBHARM_SYN_FN EQUAL -1 OR SUBHARM_SYN_RS EQUAL -1)
    message (FATAL_ERROR "WORLD synthesis.cpp changed: cannot create SynthesisSeeded")
endif()
string (REPLACE "void Synthesis(const double *f0, int f0_length,"
                "void SynthesisSeeded(unsigned int subharm_seed, const double *f0, int f0_length,"
                SUBHARM_SYN "${SUBHARM_SYN}")
string (REPLACE "randn_reseed(&randn_state);"
                "randn_reseed(&randn_state);\n  randn_state.g_randn_x ^= subharm_seed * 2654435761u;\n  randn_state.g_randn_w ^= subharm_seed * 40503u + 1u;\n  for (int warm = 0; warm < 16; ++warm) randn(&randn_state);"
                SUBHARM_SYN "${SUBHARM_SYN}")
file (WRITE ${CMAKE_BINARY_DIR}/world_seeded/synthesis_seeded.cpp "${SUBHARM_SYN}")
target_sources (world PRIVATE ${CMAKE_BINARY_DIR}/world_seeded/synthesis_seeded.cpp)
target_include_directories (world PUBLIC ${WORLD_DIR}/src)
if (MSVC)
    target_compile_definitions (world PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options (world PRIVATE /W0)
else()
    target_compile_options (world PRIVATE -w -O2)
endif()

# ---------------- SubHarm core (no JUCE) ----------------
add_library (subharm_core STATIC
    src/core/Types.cpp src/core/ChordAnalysis.cpp src/core/Harmony.cpp src/core/Renderer.cpp)
target_include_directories (subharm_core PUBLIC src/core)
find_package (Threads REQUIRED)
target_link_libraries (subharm_core PUBLIC world Threads::Threads)
if (NOT MSVC)
    target_compile_options (subharm_core PRIVATE -Wall -Wextra -O2)
endif()

# ---------------- Plugin ----------------
if (SUBHARM_BUILD_PLUGIN)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt)
        add_subdirectory (libs/JUCE)
    else()
        FetchContent_Declare (juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.10
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable (juce)
    endif()
    add_subdirectory (src/plugin)
endif()

if (SUBHARM_BUILD_TESTS)
    add_subdirectory (tests)
endif()
