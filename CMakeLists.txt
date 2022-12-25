project(k-induction)
cmake_minimum_required(VERSION 3.15.0)


find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

add_definitions("-Wall -fno-rtti" ${LLVM_DEFINITIONS_LIST})

add_compile_definitions(CLANG_EXEC_PATH=${TOOL_CLANG_PATH})

llvm_map_components_to_libnames(LLVM_LIBS support core irreader)

add_subdirectory(${PROJECT_SOURCE_DIR}/src/kinduction)

include_directories(${PROJECT_SOURCE_DIR}/include ${LLVM_INCLUDE_DIR})

aux_source_directory(${PROJECT_SOURCE_DIR}/src MAIN)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)

link_directories(${Z3_LIBRARY_DIR} ${LLVM_LIBRARY_DIR})

add_executable(test_kinduction ${MAIN})

target_link_libraries(test_kinduction clangCodeGen)

target_link_libraries(test_kinduction kinduction)


