# Copyright © 2023 CCP ehf.

find_package(GTest REQUIRED)

find_package(Python3 COMPONENTS Development REQUIRED)

add_executable(BlueCppUnitTests
    main.cpp
    TestPyMemory.cpp
    ../../src/PyMemory.cpp
    TestBlueResMan.cpp
)
target_precompile_headers(BlueCppUnitTests
    PRIVATE
        <Python.h>
        <gtest/gtest.h>
        "StdAfx.h"
)
target_include_directories(BlueCppUnitTests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../src
)
target_link_libraries(BlueCppUnitTests
    Blue
    GTest::GTest
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Same single-runtime contract as Exefile. The imported extensions resolve
    # Python symbols from this executable instead of carrying another copy.
    target_include_directories(BlueCppUnitTests PRIVATE ${Python3_INCLUDE_DIRS})
    target_link_options(BlueCppUnitTests PRIVATE
        "SHELL:-Wl,--export-dynamic"
        "SHELL:-Wl,--whole-archive ${Python3_LIBRARIES} -Wl,--no-whole-archive")
else()
    target_link_libraries(BlueCppUnitTests PRIVATE Python3::Python)
endif()

if(CMAKE_IMPORT_LIBRARY_SUFFIX)
    add_custom_command(TARGET BlueCppUnitTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:BlueCppUnitTests> $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Blue> $<TARGET_FILE_DIR:BlueCppUnitTests>
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()

gtest_discover_tests(BlueCppUnitTests
        PROPERTIES
            # This is relying on undocumented CMake behavior in order to pass multiple environment variables to the tests.
            # (The ENVIRONMENT keyword can be used multiple times to specify more than one variable)
            ENVIRONMENT "RES_PATH=${CMAKE_CURRENT_SOURCE_DIR}/res"
            # Pass required information in order for test executable to be able to initialize a python interpreter and
            # import dependencies installed through vcpkg.
            ENVIRONMENT "PYTHON_STDLIB=${Python3_STDLIB}"
            ENVIRONMENT "IMPORT_PATH=${VCPKG_IMPORT_PREFIX}"
            ENVIRONMENT "BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>"
        WORKING_DIRECTORY $<TARGET_FILE_DIR:BlueCppUnitTests>
)

# gtest_discover_tests runs each case in its own process. This runs the same
# binary once so a second Python initialization cannot hide inside CTest.
add_test(NAME BlueCppUnitTestsOneProcess COMMAND BlueCppUnitTests)
set_tests_properties(BlueCppUnitTestsOneProcess PROPERTIES
    ENVIRONMENT "RES_PATH=${CMAKE_CURRENT_SOURCE_DIR}/res;PYTHON_STDLIB=${Python3_STDLIB};IMPORT_PATH=${VCPKG_IMPORT_PREFIX};BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>"
    WORKING_DIRECTORY $<TARGET_FILE_DIR:BlueCppUnitTests>
)
