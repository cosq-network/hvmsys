#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
  CLI::App app{"HVM Disk Image Manager"};

  app.require_subcommand(1);

  auto* create = app.add_subcommand("create", "Create a disk image");
  std::string createFormat;
  std::string createSize;
  std::string createOutput;
  create->add_option("-f,--format", createFormat, "Image format")->required();
  create->add_option("-s,--size", createSize, "Image size")->required();
  create->add_option("PATH", createOutput, "Output path")->required();

  auto* info = app.add_subcommand("info", "Show image info");
  std::string infoPath;
  info->add_option("PATH", infoPath, "Image path")->required();

  auto* check = app.add_subcommand("check", "Check image integrity");
  std::string checkPath;
  check->add_option("PATH", checkPath, "Image path")->required();

  auto* convert = app.add_subcommand("convert", "Convert image format");
  std::string convertFormat;
  std::string convertInput;
  std::string convertOutput;
  convert->add_option("-f,--format", convertFormat, "Target format")->required();
  convert->add_option("IN", convertInput, "Input path")->required();
  convert->add_option("OUT", convertOutput, "Output path")->required();

  auto* snap = app.add_subcommand("snapshot", "Snapshot operations");
  snap->require_subcommand(1);
  std::string snapAction;
  std::string snapPath;
  std::string snapName;
  auto* snapCreate = snap->add_subcommand("create", "Create snapshot");
  snapCreate->add_option("PATH", snapPath, "Image path")->required();
  snapCreate->add_option("--name", snapName, "Snapshot name");
  auto* snapDelete = snap->add_subcommand("delete", "Delete snapshot");
  snapDelete->add_option("PATH", snapPath, "Image path")->required();
  snapDelete->add_option("--name", snapName, "Snapshot name")->required();
  auto* snapList = snap->add_subcommand("list", "List snapshots");
  snapList->add_option("PATH", snapPath, "Image path")->required();

  auto* overlay = app.add_subcommand("overlay", "Overlay operations");
  std::string overlayBase;
  std::string overlayPath;
  overlay->add_option("-b,--base", overlayBase, "Base image")->required();
  overlay->add_option("OVERLAY", overlayPath, "Overlay path")->required();

  CLI11_PARSE(app, argc, argv);

  if (*create) {
    fmt::print("Would create {} image of size {} at {}\n", createFormat, createSize, createOutput);
  } else if (*info) {
    fmt::print("Would show info for {}\n", infoPath);
  } else if (*check) {
    fmt::print("Would check integrity of {}\n", checkPath);
  } else if (*convert) {
    fmt::print("Would convert {} to format {} and save to {}\n", convertInput, convertFormat, convertOutput);
  } else if (*snapCreate) {
    fmt::print("Would create snapshot of {} (name: {})\n", snapPath, snapName.empty() ? "(auto)" : snapName);
  } else if (*snapDelete) {
    fmt::print("Would delete snapshot '{}' from {}\n", snapName, snapPath);
  } else if (*snapList) {
    fmt::print("Would list snapshots of {}\n", snapPath);
  } else if (*overlay) {
    fmt::print("Would create overlay {} on base {}\n", overlayPath, overlayBase);
  }

  return 0;
}
