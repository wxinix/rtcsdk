set(TEST_EXE ${PROJECT_NAME}_test)

add_executable(${TEST_EXE}
        ${CMAKE_SOURCE_DIR}/tests/rtcsdk_tests.cpp)

set(INCLUDE_DIR
        "${CMAKE_SOURCE_DIR}")

target_include_directories(
        ${TEST_EXE}
        PRIVATE ${INCLUDE_DIR})