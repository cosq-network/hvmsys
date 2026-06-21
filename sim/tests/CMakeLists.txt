add_subdirectory(unit)

add_test(
  NAME hvm-sim-tests
  COMMAND hvm-sim-tests
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
