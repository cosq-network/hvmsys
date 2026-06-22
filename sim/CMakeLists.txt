cmake_minimum_required(VERSION 3.25)
project(hvm-sim VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(HVM_SIM_ENABLE_JIT "Enable LLVM JIT backend" OFF)
option(ENABLE_UI "Enable SDL3 UI" OFF)
option(BUILD_TOOLS "Build CLI tools" ON)
option(BUILD_TESTS "Build tests" ON)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_CRYPTO "Enable OpenSSL crypto support" OFF)
option(GENERATE_OPCODES "Generate opcode table from CSV" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
elseif(MSVC)
  add_compile_options(/W4)
endif()

if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
  add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=undefined)
endif()

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GTest REQUIRED)

if(HVM_SIM_ENABLE_JIT)
  find_package(LLVM CONFIG REQUIRED)
endif()

if(ENABLE_UI)
  find_package(SDL3 CONFIG REQUIRED)
endif()

if(ENABLE_CRYPTO)
  find_package(OpenSSL CONFIG REQUIRED)
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/hvm-sim/version.hpp.in"
  "${CMAKE_BINARY_DIR}/generated/hvm-sim/version.hpp"
  @ONLY
)

if(GENERATE_OPCODES)
  include(GenerateOpcodeTable)
  generate_opcode_table(hvm-sim-opcodes)
endif()

include_directories("${CMAKE_BINARY_DIR}/generated")

add_subdirectory(src/hvm_sim_core)

if(GENERATE_OPCODES)
  add_dependencies(hvm_sim_core hvm-sim-opcodes)
endif()

add_subdirectory(src/hvm_sim_mem)
add_subdirectory(src/hvm_sim_dev)
add_subdirectory(src/hvm_sim_board)
add_subdirectory(src/hvm_sim_block)
add_subdirectory(src/hvm_sim_fw)
add_subdirectory(src/hvm_sim_monitor)
add_subdirectory(src/hvm_sim_host)

if(HVM_SIM_ENABLE_JIT)
  add_subdirectory(src/hvm_sim_jit)
endif()

if(BUILD_TOOLS)
  add_subdirectory(src/tools)
endif()

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(src/tests)
endif()
