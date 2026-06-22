add_executable(hvm-sim-tests
  main.cpp
  test_cpu.cpp
  test_hvmc.cpp
)

target_link_libraries(hvm-sim-tests PRIVATE
  GTest::gtest
  GTest::gtest_main
  hvm_sim_core
  hvm_sim_mem
  hvm_sim_dev
  hvm_sim_board
  hvm_sim_block
  hvm_sim_fw
  hvm_sim_monitor
  hvm_sim_host
  hvmc_lib
  fmt::fmt
  spdlog::spdlog
)

target_include_directories(hvm-sim-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../sim/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/tools/hvmc/include
  ${CMAKE_BINARY_DIR}/generated
)
