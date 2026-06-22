add_library(hvm_sim_fw STATIC
  stub.cpp
)

target_include_directories(hvm_sim_fw PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(hvm_sim_fw PUBLIC
  hvm_sim_core
  hvm_sim_block
)
