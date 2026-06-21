#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
  CLI::App app{"HVM Disk Formatter"};

  std::string size;
  app.add_option("--size", size, "Disk size")->required();

  std::string filesystem;
  app.add_option("--filesystem", filesystem, "Filesystem type")
      ->required()
      ->check(CLI::IsMember({"ext4", "fat32", "exfat"}));

  std::string output;
  app.add_option("--output", output, "Output path")->required();

  CLI11_PARSE(app, argc, argv);

  fmt::print("Would create {} disk image of size {} at {}\n", filesystem, size, output);

  return 0;
}
