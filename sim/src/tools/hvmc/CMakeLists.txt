# Library target for hvmc (used by both executable and tests)
add_library(hvmc_lib STATIC
  src/encoder.cpp
  src/elf.cpp
  src/assembler.cpp
  src/lexer.cpp
  src/parser.cpp
  src/codegen.cpp
)

target_include_directories(hvmc_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(hvmc_lib PUBLIC
  fmt::fmt
)

# Executable target
add_executable(hvmc
  src/main.cpp
)

target_link_libraries(hvmc PRIVATE
  hvmc_lib
)
