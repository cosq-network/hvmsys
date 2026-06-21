Phase	Time	What It Delivers
P0 Skeleton	Week 1-2	Build system, CLI stubs, CSV→opcode generator, CI (already designed)
P1 CPU Interpreter	Week 3-6	HvmCpuState, all 114 instructions decoded & executed, precise exceptions, gtest
P2 Memory+MMU+Devices	Week 7-9	Physical RAM/ROM/MMIO regions, HVM-39 3-level page table walker, TLB, HLIC (local interrupts), HPIC (platform interrupts), UART, timer, boot ROM
P3 Block Layer + Tools	Week 10-12	BlockDevice interface, Raw sparse, HSD native format (header/metadata/clusters/refcounts/snapshots), clean-room QCOW2, VHDX; hvmimg create/info/check/convert/snapshot
P4 Board Profiles	Week 13-15	4 MachineProfiles: hvm-mobile (2+4 cores, LPDDR5, UFS, MIPI stubs, PMIC, framebuffer), hvm-desktop (8 cores, DDR5 DIMM, PCIe RC, NVMe, USB, NIC), hvm-server (scalable SMP 16-128, DDR5 ECC RDIMM, BMC stub, RAS), hvm-robot (2 RT + app cores, lockstep, CAN-FD, PWM, ADC, QEI, watchdog); device tree generation
P5 Firmware	Week 16-18	HVM Boot ROM contract, HVM-SFI firmware ABI (timer/console/IPI/reset), U-Boot boot path, coreboot + EDK II payload for desktop/server, direct kernel boot (--kernel --initrd --dtb --append), memory map + ABI from specs
P6 Display/Input/Net	Week 19-20	SDL3 framebuffer + Dear ImGui debug HUD, USB HCI stub (keyboard/mouse), user-mode NIC
P7 hvmdisk	Week 21	hvmdisk create --size N --filesystem ext4/fat32/exfat --output disk.img
P8 LLVM ORC JIT	Week 22-25	JIT block cache, HVM basic blocks → LLVM IR lowering, MMIO/privileged ops via runtime helpers, ICACHE.RNG → JIT invalidation, differential interpreter-vs-JIT gtest
P9 Robotics + Polish	Week 26-27	CAN-FD message RAM + interrupts, PWM timing + ADC scripted samples + QEI encoder, lockstep mismatch injection + safe-state latch, fault scripting (TOML/JSON), VM snapshot save/load, fuzz testing