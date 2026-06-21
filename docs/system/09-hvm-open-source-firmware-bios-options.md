# HVM Open-Source Firmware and BIOS Options

This document evaluates open-source firmware, BIOS, bootloader, BMC, and root-of-trust projects that can be used with the HVM CPU, SoC, and board family.

The key constraint is that HVM is a new 64-bit RISC ISA. No existing firmware stack will boot HVM silicon without a CPU architecture port, early assembly runtime, linker scripts, exception code, timer/interrupt support, and board-specific drivers. The projects below are still useful because they provide proven boot flows, firmware services, UEFI payloads, BMC management, secure update models, and manufacturing workflows.

## 1. Recommended Firmware Stack by HVM Platform

| Platform | Recommended Stack | Rationale |
| :--- | :--- | :--- |
| `HVM-M1` mobile SoC | HVM Boot ROM -> HVM FSBL -> U-Boot SPL/U-Boot proper -> Linux/Android-style boot | U-Boot already supports staged SPL/TPL flows, FIT images, device tree, storage, networking, firmware update patterns, and many embedded SoC bring-up paths. |
| `HVM-D1` desktop board | HVM Boot ROM -> coreboot or U-Boot -> EDK II UEFI payload -> OS | Desktop users and OS installers expect UEFI. coreboot can do minimal hardware init and hand off to EDK II; U-Boot can also provide a UEFI subsystem but needs an HVM architecture port. |
| `HVM-S1` server board | HVM Boot ROM -> coreboot or LinuxBoot path -> EDK II/UEFI or LinuxBoot payload; OpenBMC on BMC | Server platforms need remote management, Redfish/IPMI, crash capture, firmware update, and measured boot. OpenBMC is the right BMC stack; LinuxBoot is attractive for datacenter control if flash space and OS policy allow it. |
| `HVM-R1` robotics SoC/board | HVM Boot ROM -> HVM safety FSBL -> U-Boot for app cores; Zephyr or a safety RTOS profile on the safety island | Robotics needs deterministic safe-state behavior before Linux boots. Use Zephyr for safety island/controller firmware where certification scope permits, and U-Boot for application-core Linux bring-up. |
| All platforms | OpenTitan-class root of trust or discrete TPM/HSM | OpenTitan is relevant for secure boot, key storage, measured boot, and auditable hardware root-of-trust designs. |

## 2. Project Assessment

### 2.1 U-Boot

U-Boot is the best first target for HVM SoC bring-up. Its documentation describes TPL/SPL/U-Boot stages where early loaders initialize RAM and load U-Boot proper; SPL can also launch EDK II, Linux, Arm TF-A BL31, or RISC-V OpenSBI. This maps well to mobile, robotics, and early desktop/server prototypes.

Recommended HVM usage:

- Port a new `arch/hvm` with reset entry, exception vectors, cache/MMU setup, timer, interrupt controller, and relocation support.
- Add HVM board device trees for `HVM-M1`, `HVM-D1`, `HVM-S1`, and `HVM-R1`.
- Use FIT images for signed kernel, initrd, device tree, and recovery images.
- Use U-Boot's UEFI subsystem only as a bridge; for full PC-class UEFI, prefer EDK II as a payload once stable.

Production fit:

- Strong for mobile, robotics, factory test, and recovery firmware.
- Good for early desktop/server bring-up.
- Needs HVM architecture work before it can execute natively.

Source: https://docs.u-boot.org/en/latest/

### 2.2 coreboot

coreboot is a strong candidate for `HVM-D1` and `HVM-S1` once the CPU and chipset initialization sequence is stable. Its design goal is to do the minimum hardware initialization required and then pass control to a payload. The official docs list common payloads including SeaBIOS, EDK II, GRUB2, and depthcharge.

Recommended HVM usage:

- Add an HVM CPU architecture port and SoC support package.
- Implement HVM memory training hooks, PCIe root-complex init, interrupt-controller setup, ACPI or device-tree handoff, and flash layout.
- Use EDK II as the payload for UEFI desktop/server boot.
- Use LinuxBoot or u-root payloads for server SKUs where datacenter operators want Linux-based firmware policy.

Production fit:

- Strong for desktop/server if the HVM silicon team owns low-level init.
- Less attractive for mobile/robotics where U-Boot and direct device tree flows are simpler.
- Not a drop-in: coreboot support is board/chipset-specific and requires significant porting.

Source: https://doc.coreboot.org/

### 2.3 EDK II / TianoCore

EDK II is the main open-source UEFI implementation. It is a cross-platform firmware development environment for UEFI and PI specifications, released under a BSD+Patent license.

Recommended HVM usage:

- Use EDK II as a payload for desktop and server boards that need standard UEFI services.
- Add HVM CPU architectural libraries, PEI/DXE platform packages, timer, interrupt, MMU, cache, PCI, SMBIOS, ACPI, secure boot, and capsule update support.
- For prototypes, run EDK II after coreboot or U-Boot has already initialized DRAM.

Production fit:

- Best fit for `HVM-D1` and `HVM-S1` OS compatibility.
- Useful for manufacturing diagnostics through UEFI shell and UEFI apps.
- Heavyweight for mobile and robotics unless those products specifically require UEFI.

Source: https://github.com/tianocore/tianocore.github.io/wiki/EDK-II

### 2.4 OpenSBI

OpenSBI is a RISC-V implementation of the Supervisor Binary Interface. It is not directly reusable for HVM unless HVM intentionally adopts an SBI-like machine/supervisor firmware boundary. Its model is still valuable: machine-mode firmware exposes stable services to supervisors, hypervisors, and OS kernels.

Recommended HVM usage:

- Do not treat OpenSBI as drop-in firmware for HVM.
- Reuse the architectural idea by defining `HVM-SBI` or `HVM Supervisor Firmware Interface` for timers, IPIs, hart/core start, shutdown, console, PMU, and platform reset.
- Consider a clean-room HVM firmware layer with OpenSBI-like APIs so Linux, BSD, hypervisors, and U-Boot can share a stable supervisor interface.

Production fit:

- Direct fit only if HVM becomes RISC-V-compatible, which current HVM docs do not state.
- Strong conceptual template for HVM supervisor firmware.

Source: https://github.com/riscv-software-src/opensbi

### 2.5 OpenBMC

OpenBMC is the recommended BMC firmware for `HVM-S1` server boards. The OpenBMC project describes itself as an open-source BMC firmware stack for heterogeneous enterprise, HPC, telco, and cloud-scale data-center systems.

Recommended HVM usage:

- Use an ASPEED AST2600-class or equivalent BMC running OpenBMC.
- Integrate Redfish, IPMI where needed, serial-over-LAN, virtual media, host power sequencing, sensor telemetry, firmware update, watchdogs, crash logs, and FRU EEPROM provisioning.
- Connect BMC to HVM host through LPC/eSPI-equivalent, UART, I2C/SMBus, SPI recovery, GPIO power controls, and mailbox registers.

Production fit:

- Strong for server.
- Useful for high-end robotics test rigs, but usually too heavy for production mobile/robotics products.

Source: https://openbmc.org

### 2.6 LinuxBoot

LinuxBoot replaces much of traditional UEFI DXE policy with a Linux kernel and userland. Its upstream repository says it enables Linux to replace server firmware and lists supported server mainboards and QEMU targets.

Recommended HVM usage:

- Consider for `HVM-S1` datacenter SKUs after basic HVM Linux support is mature.
- Use coreboot or U-Boot for early hardware init, then LinuxBoot/u-root for network boot, attestation, storage discovery, and operator policy.
- Keep EDK II available for compatibility SKUs.

Production fit:

- Good for hyperscale/server deployments that can standardize on Linux-based boot policy.
- Not ideal for broad desktop compatibility.
- Needs working HVM Linux very early in boot.

Source: https://github.com/linuxboot/linuxboot

### 2.7 Zephyr

Zephyr is an open-source RTOS for resource-constrained embedded systems. Its documentation lists support for multiple architectures, including Arm, RISC-V, x86, OpenRISC, SPARC, and others, plus services such as power management, device model, devicetree, networking, Bluetooth, filesystems, shell, and firmware update primitives.

Recommended HVM usage:

- Use Zephyr for `HVM-R1` safety island firmware, board controller firmware, EC-like functions, factory fixtures, and small always-on controllers.
- Port HVM as a Zephyr architecture only if RT cores are meant to run Zephyr directly.
- Otherwise use Zephyr on a companion microcontroller that supervises HVM power, safety, fieldbus, and watchdog behavior.

Production fit:

- Strong for robotics control, board management, and manufacturing fixtures.
- Not a BIOS/UEFI replacement for desktop/server.
- Functional-safety certification still requires a controlled safety process around the selected Zephyr subset.

Source: https://docs.zephyrproject.org/latest/introduction/index.html

### 2.8 OpenTitan

OpenTitan is an open-source silicon root-of-trust project. Its public site describes commercial-grade IP blocks and security-certified hardware root-of-trust designs, Apache 2.0 licensing, production silicon use, and compliance/security certification goals.

Recommended HVM usage:

- Use as a design reference or integrated/discrete root-of-trust block for secure boot, key storage, attestation, firmware measurement, lifecycle state, debug unlock, and recovery.
- Tie OpenTitan-class RoT into HVM Boot ROM, SPI flash verification, BMC measured boot, and manufacturing key provisioning.
- Keep it separate from general application firmware ownership.

Production fit:

- Strong for all HVM profiles, especially server, desktop workstation, and robotics safety/security SKUs.
- Requires silicon/IP integration planning, threat modeling, key ceremonies, and lifecycle-state definition.

Source: https://opentitan.org/

### 2.9 Trusted Firmware-A

Trusted Firmware-A is valuable only if an HVM product includes Arm application cores or an Arm-based management/security island. TF-A provides Arm secure-world boot/runtime firmware for Armv7-A and Armv8-A, including EL3 secure monitor services and Arm interface standards such as PSCI, TBBR, SMC Calling Convention, SCMI, and PSA firmware update.

Recommended HVM usage:

- Do not use TF-A for native HVM cores unless HVM intentionally implements Arm exception levels and secure monitor semantics, which it currently does not.
- Use TF-A only for companion Arm subsystems inside a mixed-architecture SoC.
- Reuse its architectural separation ideas when designing HVM secure monitor firmware.

Production fit:

- Conditional. Useful for Arm companion controllers; not a native HVM BIOS.

Source: https://trustedfirmware-a.readthedocs.io/en/latest/

### 2.10 oreboot and Slim Bootloader

oreboot is a Rust-focused downstream fork of coreboot that currently targets LinuxBoot payloads. It is promising for experimental secure firmware, but it is less mature as a production base for a new custom ISA than coreboot or U-Boot.

Slim Bootloader is an Intel-maintained open-source boot firmware optimized for Intel x86 architecture. Its design is relevant, but it is not a natural base for HVM unless HVM wants to replicate its staged design concepts.

Recommended HVM usage:

- Track oreboot as a long-term research option for minimal Rust firmware.
- Do not base first HVM production firmware on oreboot unless the team commits to upstreaming a full HVM architecture and board support.
- Treat Slim Bootloader as a design reference, not a porting target, because its official FAQ frames it around Intel x86 architecture.

Sources:

- https://github.com/oreboot/oreboot
- https://slimbootloader.github.io/

## 3. Proposed HVM Boot Architecture

```
Power-on / reset
      |
      v
HVM mask Boot ROM
      |
      | verifies FSBL using fused RoT key
      v
HVM FSBL
      |
      | initializes clocks, PMIC/VRM, SRAM, DRAM training, fuses, debug policy
      v
Platform firmware
      |
      +-- Mobile / robotics: U-Boot SPL -> U-Boot proper -> Linux/RTOS
      |
      +-- Desktop: coreboot or U-Boot -> EDK II payload -> OS installer/runtime
      |
      +-- Server: coreboot/U-Boot -> EDK II or LinuxBoot -> OS/hypervisor
      |
      +-- Server BMC side: BMC Boot ROM -> U-Boot -> OpenBMC Linux
```

## 4. HVM Porting Work Required

Any selected open-source firmware stack needs these HVM-specific pieces:

- Toolchain: HVM GCC/LLVM backend or stable cross-compiler, assembler, linker, objcopy, debug symbols, and firmware linker scripts.
- Reset and exception model: reset vector, privilege levels, trap entry/exit, interrupt masking, timer interrupts, fault reporting, and debug exception behavior.
- Memory model: cache enable/disable, MMU/page table setup, memory barriers, DMA coherency, I-cache invalidation using `ICACHE.RNG`, and device-memory attributes.
- SMP bring-up: secondary-core release, IPIs, core-local timers, parking loops, low-power states, and hotplug rules.
- Device model: UART, timer, interrupt controller, GPIO, SPI, I2C, PCIe, USB, DDR/LPDDR PHY, storage, watchdog, RTC, thermal sensors, and reset controller.
- Platform description: devicetree for U-Boot/Linux/Zephyr and ACPI/SMBIOS tables for UEFI desktop/server.
- Secure boot: Boot ROM contract, image format, signature scheme, rollback counters, fuse lifecycle, debug lock, recovery mode, and RMA unlock.
- Manufacturing: serial provisioning, MAC/UUID/FRU data, factory test commands, boundary scan hooks, board revision detection, and firmware update capsules.

## 5. Recommendation

Start with U-Boot for first silicon and FPGA/emulator bring-up because it gives the shortest path to clocks, DRAM, UART, storage, device tree, FIT images, and Linux boot. In parallel, define an HVM supervisor firmware interface inspired by OpenSBI, but do not directly depend on OpenSBI unless HVM becomes RISC-V-compatible.

For production desktop and server boards, add coreboot plus EDK II as the compatibility stack. For servers, use OpenBMC on the BMC from day one. For robotics, keep the safety island firmware independent and small, with Zephyr as the strongest open-source candidate if its certification implications are acceptable. For all products, evaluate OpenTitan-class root-of-trust integration before freezing the SoC security architecture.
