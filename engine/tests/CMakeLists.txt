file(GLOB_RECURSE TEST_SOURCES *.cpp)
add_executable(efl_tests ${TEST_SOURCES} ${EFL_SOURCES})
target_link_libraries(efl_tests PRIVATE
    GTest::gtest_main
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator
)
target_include_directories(efl_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_definitions(efl_tests PRIVATE
    EFL_BUILDING_DLL
    EFL_STUB_SDK
    EFL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    EFL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    EFL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    EFL_VERSION_PRERELEASE="${EFL_VERSION_PRERELEASE}"
)
include(GoogleTest)
gtest_discover_tests(efl_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
