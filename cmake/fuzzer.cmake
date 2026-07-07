# libFuzzer harness (clang-only, opt-in).
#
# Build with:
#   cmake -S . -B build-fuzz -DXTFSH_ENABLE_FUZZER=ON \
#         -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTS=OFF
#   cmake --build build-fuzz --target XTFSH_parser_fuzzer
# Run:
#   ./build-fuzz/XTFSH_parser_fuzzer fuzz/corpus -max_total_time=60

option(XTFSH_ENABLE_FUZZER "Build the libFuzzer parser harness (clang required)" OFF)
if(XTFSH_ENABLE_FUZZER)
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
            CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
        message(FATAL_ERROR
            "XTFSH_ENABLE_FUZZER requires clang (got ${CMAKE_CXX_COMPILER_ID})")
    endif()
    # Parse-only surface: just parser.cpp. XTFSH/core.h declares symbols
    # defined elsewhere (write_stderr inline, builtins, etc.) but the
    # parser never calls them, so the linker resolves without the rest
    # of the shell. Keeps instrumentation tight.
    set(XTFSH_FUZZ_SOURCES
        src/core/parser.cpp
        src/util/io.cpp
    )
    add_executable(XTFSH_parser_fuzzer fuzz/parser_fuzz.cpp ${XTFSH_FUZZ_SOURCES})
    target_include_directories(XTFSH_parser_fuzzer PRIVATE
        ${XTFSH_INCLUDE_DIRS}
        ${nlohmann_json_SOURCE_DIR}/include)
    target_compile_definitions(XTFSH_parser_fuzzer PRIVATE
        TESTING_BUILD
        XTFSH_VERSION_STRING="${PROJECT_VERSION}"
        XTFSH_THEMES_DIR="${CMAKE_SOURCE_DIR}/data/themes")
    target_compile_options(XTFSH_parser_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined -g -O1)
    target_link_options(XTFSH_parser_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(XTFSH_parser_fuzzer PRIVATE replxx)
endif()
