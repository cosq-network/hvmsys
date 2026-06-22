find_package(Python3 REQUIRED)

# Capture module directory at include time
set(_HVM_GEN_OPCODE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(generate_opcode_table target)
  # CSV file is in the project root docs/ directory
  set(CSV_FILE "${CMAKE_SOURCE_DIR}/docs/hvm_instruction_set.csv")
  set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
  set(OUTPUT_FILE "${GENERATED_DIR}/hvm_opcode_table.hpp")

  file(MAKE_DIRECTORY "${GENERATED_DIR}")

  add_custom_command(
    OUTPUT "${OUTPUT_FILE}"
    COMMAND "${Python3_EXECUTABLE}" "${_HVM_GEN_OPCODE_DIR}/generate_opcode_table.py"
            "${CSV_FILE}" "${OUTPUT_FILE}"
    DEPENDS "${_HVM_GEN_OPCODE_DIR}/generate_opcode_table.py" "${CSV_FILE}"
    COMMENT "Generating HVM opcode table from CSV"
  )

  add_custom_target("${target}" DEPENDS "${OUTPUT_FILE}")
endfunction()
