# libFuzzer harness (clang-only, opt-in).
#
# Build with:
#   cmake -S . -B build-fuzz -DCJHSH_ENABLE_FUZZER=ON \
#         -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTS=OFF
#   cmake --build build-fuzz --target CJHSH_parser_fuzzer
# Run:
#   ./build-fuzz/CJHSH_parser_fuzzer fuzz/corpus -max_total_time=60

option(CJHSH_ENABLE_FUZZER "Build the libFuzzer parser harness (clang required)" OFF)
if(CJHSH_ENABLE_FUZZER)
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
            CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
        message(FATAL_ERROR
            "CJHSH_ENABLE_FUZZER requires clang (got ${CMAKE_CXX_COMPILER_ID})")
    endif()
    # Parse-only surface: just parser.cpp. CJHSH/core.h declares symbols
    # defined elsewhere (write_stderr inline, builtins, etc.) but the
    # parser never calls them, so the linker resolves without the rest
    # of the shell. Keeps instrumentation tight.
    set(CJHSH_FUZZ_SOURCES
        src/core/parser.cpp
        src/util/io.cpp
    )
    add_executable(CJHSH_parser_fuzzer fuzz/parser_fuzz.cpp ${CJHSH_FUZZ_SOURCES})
    target_include_directories(CJHSH_parser_fuzzer PRIVATE
        ${CJHSH_INCLUDE_DIRS}
        ${nlohmann_json_SOURCE_DIR}/include)
    target_compile_definitions(CJHSH_parser_fuzzer PRIVATE
        TESTING_BUILD
        CJHSH_VERSION_STRING="${PROJECT_VERSION}"
        CJHSH_THEMES_DIR="${CMAKE_SOURCE_DIR}/data/themes")
    target_compile_options(CJHSH_parser_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined -g -O1)
    target_link_options(CJHSH_parser_fuzzer PRIVATE
        -fsanitize=fuzzer,address,undefined)
    target_link_libraries(CJHSH_parser_fuzzer PRIVATE replxx)
endif()
