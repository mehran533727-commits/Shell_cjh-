# Test suite: GoogleTest fetch + the unit / plugin / integration test
# executables. Relies on the main target (CJHSH.out) and SHELL_SOURCES
# being defined in the root CMakeLists, and on plugin_list.cmake having
# already registered its per-plugin test specs via CJHSH_register_plugin().

option(BUILD_TESTS "Build test suite" ON)

if(BUILD_TESTS)
    enable_testing()
    include(GoogleTest)

    include(FetchContent)
    set(CJHSH_FETCHCONTENT_TIMESTAMP_ARGS)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
        list(APPEND CJHSH_FETCHCONTENT_TIMESTAMP_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
        ${CJHSH_FETCHCONTENT_TIMESTAMP_ARGS}
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Same sources as CJHSH.out minus main.cpp. Under TESTING_BUILD
    # main.cpp's int main() is #ifdef'd out, so including it in the
    # static lib would have ranlib warn "has no symbols". Computed
    # here (not earlier) so plugin SOURCES from CJHSH_register_plugin
    # are already appended to SHELL_SOURCES.
    set(SHELL_LIB_SOURCES ${SHELL_SOURCES})
    list(REMOVE_ITEM SHELL_LIB_SOURCES src/main.cpp)
    add_library(shell_lib STATIC ${SHELL_LIB_SOURCES})
    target_link_libraries(shell_lib PRIVATE replxx)
    target_include_directories(shell_lib PRIVATE
        ${CJHSH_INCLUDE_DIRS}
        ${nlohmann_json_SOURCE_DIR}/include)
    target_compile_definitions(shell_lib PRIVATE
        TESTING_BUILD
        CJHSH_VERSION_STRING="${PROJECT_VERSION}"
        CJHSH_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        CJHSH_THEMES_DIR="${CMAKE_SOURCE_DIR}/data/themes")

    # Same embed as CJHSH.out — tests need the AI model registry too.
    target_sources(shell_lib PRIVATE "${CMAKE_BINARY_DIR}/generated/ai_models_embedded.cpp")

    target_link_libraries(shell_lib PRIVATE CURL::libcurl Threads::Threads)

    if(CJHSH_SQLITE_ENABLED)
        target_compile_definitions(shell_lib PRIVATE CJHSH_SQLITE_ENABLED)
        target_include_directories(shell_lib PRIVATE ${SQLite3_INCLUDE_DIRS})
        target_link_libraries(shell_lib PRIVATE ${SQLite3_LIBRARIES})
    endif()

    add_executable(test_tokenizer tests/unit/test_tokenizer.cpp)
    target_include_directories(test_tokenizer PRIVATE ${CJHSH_INCLUDE_DIRS})
    target_link_libraries(test_tokenizer GTest::gtest_main shell_lib)
    gtest_discover_tests(test_tokenizer TEST_PREFIX "unit/")

    add_executable(test_parser_properties tests/unit/test_parser_properties.cpp)
    target_include_directories(test_parser_properties PRIVATE ${CJHSH_INCLUDE_DIRS})
    target_link_libraries(test_parser_properties GTest::gtest_main shell_lib)
    gtest_discover_tests(test_parser_properties TEST_PREFIX "unit/")

    add_executable(test_parser_errors tests/unit/test_parser_errors.cpp)
    target_include_directories(test_parser_errors PRIVATE ${CJHSH_INCLUDE_DIRS})
    target_link_libraries(test_parser_errors GTest::gtest_main shell_lib)
    gtest_discover_tests(test_parser_errors TEST_PREFIX "unit/")

    add_executable(test_builtins_help tests/unit/test_builtins_help.cpp)
    target_include_directories(test_builtins_help PRIVATE ${CJHSH_INCLUDE_DIRS})
    target_link_libraries(test_builtins_help GTest::gtest_main shell_lib)
    gtest_discover_tests(test_builtins_help TEST_PREFIX "unit/")

    # Create all plugin tests declared via CJHSH_register_plugin(TEST_SOURCES ...)
    CJHSH_finalize_plugin_tests()

    # ── Integration tests ─────────────────────────────────────
    # Auto-discover every test_*.cpp under tests/integration/ so new
    # test files don't require CMakeLists edits. CONFIGURE_DEPENDS
    # forces a re-glob on file-add/remove so builds stay correct.
    file(GLOB CJHSH_INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/tests/integration/test_*.cpp)
    add_executable(test_integration ${CJHSH_INTEGRATION_TEST_SOURCES})
    target_include_directories(test_integration PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CJHSH_INCLUDE_DIRS}
    )
    target_link_libraries(test_integration GTest::gtest)
    gtest_discover_tests(test_integration
        TEST_PREFIX "integration/"
        PROPERTIES ENVIRONMENT "CJHSH_SHELL_BIN=$<TARGET_FILE:CJHSH.out>"
    )
endif()
