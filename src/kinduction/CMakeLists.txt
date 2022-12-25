set(DEPENDENCIES_DIR "${PROJECT_SOURCE_DIR}/dependencies")

set(LLVM_DIR "${DEPENDENCIES_DIR}/clang+llvm-11.0.0-x86_64-apple-darwin/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)

set(Z3_INSTALL_PREFIX "${DEPENDENCIES_DIR}/z3-4.8.12-x64-osx-10.15.7")
set(Z3_INCLUDE_DIR ${Z3_INSTALL_PREFIX}/include)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

add_definitions("-Wall -fno-rtti" ${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(LLVM_LIBS support core irreader)

include_directories(${PROJECT_SOURCE_DIR}/include ${LLVM_INCLUDE_DIR} ${Z3_INCLUDE_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} KINDUCTION_SRC_DIR)

add_library(kinduction ${KINDUCTION_SRC_DIR})

target_link_libraries(kinduction z3 ${LLVM_LIBS})

