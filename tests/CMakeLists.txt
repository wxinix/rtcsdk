set(TEST_EXE ${PROJECT_NAME}_test)

add_executable(${TEST_EXE}
        ${CMAKE_SOURCE_DIR}/tests/rtcsdk_tests.cpp)

set(INCLUDE_DIR
        "${CMAKE_SOURCE_DIR}")

target_include_directories(
        ${TEST_EXE}
        PRIVATE ${INCLUDE_DIR})

# IDL generator tests
set(IDLGEN_TEST_EXE idlgen_test)

add_executable(${IDLGEN_TEST_EXE}
        ${CMAKE_SOURCE_DIR}/tests/idlgen_tests.cpp
        ${CMAKE_SOURCE_DIR}/tools/idlgen/preprocessor.cpp
        ${CMAKE_SOURCE_DIR}/tools/idlgen/parser.cpp
        ${CMAKE_SOURCE_DIR}/tools/idlgen/type_mapper.cpp
        ${CMAKE_SOURCE_DIR}/tools/idlgen/idl_generator.cpp)

target_include_directories(
        ${IDLGEN_TEST_EXE}
        PRIVATE ${INCLUDE_DIR}
        PRIVATE ${CMAKE_SOURCE_DIR}/tools/idlgen)

target_compile_features(${IDLGEN_TEST_EXE} PRIVATE cxx_std_23)