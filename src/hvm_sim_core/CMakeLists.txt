add_library(hvm_sim_core STATIC
  stub.cpp
  executor.cpp
)

target_include_directories(hvm_sim_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(hvm_sim_core PUBLIC
  fmt::fmt
  spdlog::spdlog
)
