add_library(hvm_sim_jit STATIC
  stub.cpp
)

target_include_directories(hvm_sim_jit PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(hvm_sim_jit PUBLIC
  hvm_sim_core
)

if(HVM_SIM_ENABLE_JIT AND LLVM_FOUND)
  target_link_libraries(hvm_sim_jit PUBLIC
    LLVM::Core
    LLVM::OrcJIT
    LLVM::Target
    LLVM::Analysis
  )
endif()
