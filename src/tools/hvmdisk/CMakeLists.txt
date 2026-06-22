add_executable(hvmdisk
  main.cpp
)

target_link_libraries(hvmdisk PRIVATE
  hvm_sim_block
  hvm_sim_core
  CLI11::CLI11
  fmt::fmt
  spdlog::spdlog
)
