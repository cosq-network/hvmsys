add_executable(hvmsys
  main.cpp
)

target_link_libraries(hvmsys PRIVATE
  hvm_sim_core
  hvm_sim_mem
  hvm_sim_dev
  hvm_sim_board
  hvm_sim_block
  hvm_sim_fw
  hvm_sim_monitor
  hvm_sim_host
  CLI11::CLI11
  fmt::fmt
  spdlog::spdlog
)
