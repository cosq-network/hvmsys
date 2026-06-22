#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  CLI::App app{"HVM System Simulator"};

  std::string machine;
  app.add_option("-m,--machine", machine, "Machine profile")
      ->required()
      ->check(CLI::IsMember({"hvm-mobile", "hvm-desktop", "hvm-server", "hvm-robot"}));

  std::string ram = "4G";
  app.add_option("--ram", ram, "RAM size (default 4G)");

  int smp = 1;
  app.add_option("--smp", smp, "Number of CPUs (default 1)");

  std::vector<std::string> drives;
  app.add_option("--drive", drives, "Drive NAME=PATH (repeatable)");

  std::string display = "none";
  app.add_option("--display", display, "Display backend (none|sdl)")
      ->check(CLI::IsMember({"none", "sdl"}));

  std::string bios;
  app.add_option("--bios", bios, "BIOS/firmware image");

  std::string kernel;
  app.add_option("--kernel", kernel, "Kernel image");

  std::string initrd;
  app.add_option("--initrd", initrd, "Initrd image");

  std::string dtb;
  app.add_option("--dtb", dtb, "Device tree blob");

  std::string append;
  app.add_option("--append", append, "Kernel command line");

  bool jit = false;
  app.add_flag("--jit", jit, "Enable JIT");

  bool debug = false;
  app.add_flag("--debug", debug, "Enable debug output");

  std::string monitor = "none";
  app.add_option("--monitor", monitor, "Monitor interface (stdio|none)")
      ->check(CLI::IsMember({"stdio", "none"}));

  bool version = false;
  app.add_flag("--version", version, "Show version");

  CLI11_PARSE(app, argc, argv);

  if (version) {
    fmt::print("HVM System Simulator v{}\n", "0.1.0");
    return 0;
  }

  fmt::print("HVM System Simulator Configuration:\n");
  fmt::print("  Machine:  {}\n", machine);
  fmt::print("  RAM:      {}\n", ram);
  fmt::print("  SMP:      {}\n", smp);
  fmt::print("  Display:  {}\n", display);
  fmt::print("  BIOS:     {}\n", bios.empty() ? "(none)" : bios);
  fmt::print("  Kernel:   {}\n", kernel.empty() ? "(none)" : kernel);
  fmt::print("  Initrd:   {}\n", initrd.empty() ? "(none)" : initrd);
  fmt::print("  DTB:      {}\n", dtb.empty() ? "(none)" : dtb);
  fmt::print("  Append:   {}\n", append.empty() ? "(none)" : append);
  fmt::print("  JIT:      {}\n", jit ? "yes" : "no");
  fmt::print("  Debug:    {}\n", debug ? "yes" : "no");
  fmt::print("  Monitor:  {}\n", monitor);
  for (const auto& d : drives) {
    fmt::print("  Drive:    {}\n", d);
  }

  return 0;
}
