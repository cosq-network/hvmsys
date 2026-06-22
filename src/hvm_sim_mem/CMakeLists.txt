add_library(hvm_sim_mem STATIC
  stub.cpp
)

target_include_directories(hvm_sim_mem PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sim/include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(hvm_sim_mem PUBLIC
  hvm_sim_core
)
