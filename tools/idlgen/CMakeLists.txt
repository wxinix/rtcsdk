add_executable(idlgen
    main.cpp
    preprocessor.cpp
    parser.cpp
    type_mapper.cpp
    idl_generator.cpp)

target_include_directories(idlgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(idlgen PRIVATE cxx_std_23)
