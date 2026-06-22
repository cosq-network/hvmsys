add_subdirectory(hvm-sim-tests)

add_test(
  NAME hvm-sim-tests
  COMMAND hvm-sim-tests
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
