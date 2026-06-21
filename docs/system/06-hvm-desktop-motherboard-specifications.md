# HVM Desktop Motherboard Specifications

This document defines the `HVM-D1` desktop CPU and `HVM-MB-D1` reference motherboard. The platform targets developer workstations, gaming desktops, compact tower PCs, and local AI/graphics workstations.

The design inherits the HVM desktop CPU profile: 8 big HVM cores, 256-bit HVM-V vector width, HVM-C compressed instruction decode, HVM-ARC atomic reference management, `ICACHE.RNG`, optional HVM-L hardware loops, HVM-MEM pair load/store and prefetch hints, PCIe accelerator doorbells, and a socketed DDR5 board ecosystem.

## 1. Industry Reference Envelope

`HVM-MB-D1` is sized against contemporary enthusiast desktop platforms. AMD Ryzen 9 9950X provides a relevant desktop CPU reference with 16 cores, 32 threads, up to 5.7 GHz boost, 170 W default TDP, DDR5 memory, AM5 socket, and PCIe Gen 5 support. Intel Core Ultra 9 285K provides a second reference with FCLGA1851 packaging, PCIe 5.0/4.0, up to 24 PCIe lanes, and modern desktop platform management.

The HVM desktop platform should compete at the board level: socketed CPU, dual-channel DDR5, PCIe Gen 5 graphics, M.2 Gen 5 storage, USB4, 2.5 GbE or faster networking, and predictable firmware bring-up.

## 2. CPU Package Summary

| Item | `HVM-D1` Specification |
| :--- | :--- |
| CPU topology | 8 big HVM cores |
| ISA extensions | HVM-C, HVM-ARC, `ICACHE.RNG`, HVM-L, HVM-MEM, HVM-V 256-bit `VLEN`, HVM-A, HVM-Prof |
| Process target | TSMC N4P or equivalent 4 nm-class FinFET |
| Package | Socket HVM-S1, LGA-1700-class pin budget |
| Base / boost clock | 3.2 GHz base, up to 4.8 GHz boost target |
| Cache | 64 KB I + 64 KB D L1 per core, 512 KB L2 per core, 32 MB shared L3 |
| Memory | Dual-channel DDR5 UDIMM, ECC-capable where board and DIMMs support it |
| PCIe from CPU | 1x PCIe Gen 5 x16 GPU, 2x PCIe Gen 5 x4 NVMe |
| TDP | 65 W nominal, 125 W performance, 170 W workstation boost board limit |
| Thermal limit | 105 C maximum junction temperature |

## 3. Reference Motherboard Summary

| Item | `HVM-MB-D1` Specification |
| :--- | :--- |
| Form factors | ATX and micro-ATX variants |
| CPU socket | HVM-S1 LGA-1700-class socket |
| Memory slots | 4x DDR5 UDIMM slots, dual channel |
| Max memory | 192 GB-256 GB depending on DIMM density |
| Expansion | 1x PCIe Gen 5 x16, 1x PCIe Gen 4 x4, 1x PCIe Gen 3 x1 |
| Storage | 2x M.2 PCIe Gen 5 x4, 2x M.2 PCIe Gen 4 x4, 4x SATA 6 Gb/s |
| Rear I/O | USB4 Type-C, USB 3.2 Gen 2, HDMI/DP from iGPU, 2.5 GbE |
| Wireless | M.2 Key-E Wi-Fi 7 / Bluetooth module slot |
| Firmware | 32 MB SPI flash, HVM-SFI + coreboot/UEFI payload |
| Debug | POST code display, UART header, JTAG header, recovery SPI header |

## 4. Board Block Diagram

```
+----------------------+       +-----------------------+
| HVM-S1 socket        |=======| 4x DDR5 UDIMM slots   |
| HVM-D1 CPU           |       | dual-channel memory   |
+----------+-----------+       +-----------------------+
           |
           | PCIe Gen 5 x16
           v
+----------------------+       +-----------------------+
| GPU / accelerator    |       | 2x M.2 Gen 5 x4       |
| slot                 |       | direct CPU storage    |
+----------------------+       +-----------------------+
           |
           | HVM platform controller / chipset link
           v
+----------------------+       +-----------------------+
| USB4 / USB 3.2       |       | SATA / audio / LAN    |
| Wi-Fi / BT / GPIO    |       | TPM / SPI firmware    |
+----------------------+       +-----------------------+
```

## 5. Power Delivery

| Rail | Requirement |
| :--- | :--- |
| CPU VRM | 12+2 phase minimum for ATX, 8+2 phase minimum for micro-ATX |
| Smart power stages | 70 A minimum per CPU core phase |
| Input power | 24-pin ATX plus 8-pin EPS; workstation variant uses 8+8 EPS |
| `VDD_CORE` | 0.70 V-1.20 V DVFS, transient-aware load-line calibration |
| `VDD_SRAM` | 0.90 V fixed low-noise cache rail |
| `VDD_MEM_PHY` | DDR5 PHY rail isolated from core switching noise |
| Telemetry | PMBus or equivalent VRM telemetry exposed to firmware |

Thermal design must support:

- 65 W tower cooler baseline.
- 125 W sustained all-core workloads.
- 170 W short-duration workstation boost with firmware-enforced current limits.

## 6. Memory Subsystem

- DDR5 UDIMM dual-channel topology.
- Two DIMMs per channel maximum.
- Target DDR5-5600 at 1 DPC and DDR5-4400/4800 at 2 DPC, with board-specific tuning tables.
- ECC UDIMM support is recommended for developer workstations and required for validation boards used in compiler/runtime testing.
- Memory training data should be cached in SPI flash to reduce boot time after successful training.

## 7. PCIe, Storage, and Graphics

| Interface | Requirement |
| :--- | :--- |
| GPU slot | PCIe Gen 5 x16, 75 W slot power, reinforced mechanical connector |
| NVMe direct slots | 2x M.2 Key-M PCIe Gen 5 x4 from CPU |
| Chipset storage | 2x M.2 PCIe Gen 4 x4 plus 4x SATA 6 Gb/s |
| Resizable BAR | Required for HVM-A GPU shared-memory workflows |
| ATS/PRI | Required for accelerator SVM experimentation |
| Display | HDMI 2.1 and DisplayPort 1.4a from integrated graphics where SKU supports it |

## 8. Firmware and Runtime Support

- Boot ROM enters HVM-SFI firmware from SPI flash.
- coreboot or UEFI payload performs board initialization, memory training, PCIe enumeration, and ACPI/SMBIOS table publication.
- Firmware must expose HVM feature flags for HVM-C, HVM-ARC, HVM-L, HVM-MEM, HVM-V, HVM-A, HVM-Prof, and `ICACHE.RNG`.
- GPU doorbell MMIO regions must be mapped non-cacheable and marked with the appropriate privilege policy.
- Secure boot is optional on consumer boards but required on workstation validation boards.

## 9. PCB Layout Rules

- Board stackup: 10 layers minimum for micro-ATX, 12 layers preferred for ATX.
- PCIe Gen 5 differential impedance: 85 ohm +/- 10%.
- USB4 differential impedance: 90 ohm +/- 10%.
- DDR5 command/address fly-by routing with controlled impedance and per-byte-lane length matching.
- PCIe Gen 5 M.2 paths must use low-loss laminate on long runs; retimers are allowed on ATX boards if route length exceeds channel budget.
- Socket decoupling must place high-frequency capacitors on the back side directly under the HVM-S1 power ball fields.

## 10. Validation Checklist

- DDR5 memory training passes 1 DPC and 2 DPC matrices.
- PCIe Gen 5 x16 graphics link trains at full width and speed with Resizable BAR enabled.
- M.2 Gen 5 storage passes thermal throttling and signal integrity validation.
- HVM-ARC, HVM-L, HVM-MEM, HVM-Prof, and `ICACHE.RNG` tests pass under JIT-heavy workloads.
- HVM-V vector context save/restore passes OS scheduler stress tests.
- Board passes suspend/resume, wake-on-LAN, USB4 hot-plug, and SPI recovery flows.

## 11. CPU Production Requirements

The `HVM-D1` CPU is socketed, so production readiness depends on both silicon and package/socket discipline:

- Tape-out requires clean RTL lint, CDC/RDC, formal equivalence, STA, DRC, LVS, antenna, IR-drop, electromigration, ESD, and latch-up signoff.
- DFT must include scan compression, JTAG boundary scan, MBIST for cache/SRAM arrays, LBIST for core logic, PLL test modes, PCIe/DDR PHY loopback, and package-level continuity coverage.
- Speed binning must define base clock, boost clock, voltage-frequency curves, leakage limits, AVX/vector-equivalent guardbands for HVM-V, and per-bin TDP.
- Package design must include substrate stackup, land pattern, keepout zones, socket load specification, heatsink load limits, thermal resistance, and mechanical tolerances.
- Final test must validate HVM-C decode, HVM-ARC atomics, HVM-L hardware loops where enabled, HVM-MEM pair loads/stores and hints, HVM-V vector execution, HVM-Prof counters, `ICACHE.RNG`, DDR5 PHY margins, PCIe Gen 5 PHY margins, fuse programming, and debug lock.

## 12. Desktop Board Production Package

The `HVM-MB-D1` release package must include:

- Schematic, PCB source database, fabrication notes, stackup drawing, impedance rules, Gerber/ODB++/IPC-2581, drill files, netlist, assembly drawing, pick-and-place files, paste masks, and 3D mechanical model.
- Controlled BOM with manufacturer part numbers, approved alternates, lifecycle status, power/thermal derating, no-substitution parts, and firmware-loaded parts.
- Test collateral including ICT fixture files, boundary-scan vectors, power-rail margin scripts, DDR5 memory validation matrix, PCIe compliance checklist, and final functional test image.
- Manufacturing instructions for socket inspection, CPU retention hardware torque, VRM thermal pad placement, BIOS flash programming, MAC address programming, serial-number label placement, and packaging.

## 13. Compliance and Safety

| Area | Requirement |
| :--- | :--- |
| Electrical safety | IEC/UL 62368-1 planning for finished systems using the board |
| EMC | FCC Part 15 Class B / CE EMC pre-scan and final test on representative chassis |
| Environmental | RoHS, REACH, WEEE, conflict minerals, and halogen-free policy if required |
| Interfaces | USB-IF, PCI-SIG, HDMI/DisplayPort, SATA-IO, and Wi-Fi modular compliance as populated |
| Firmware security | Secure boot option, SPI write protection, TPM provisioning, rollback prevention, and recovery path |

## 14. EVT, DVT, and PVT Exit Criteria

| Gate | Exit Criteria |
| :--- | :--- |
| EVT | Board powers safely, CPU exits reset, SPI firmware boots, DDR5 trains, PCIe root ports enumerate, and debug access works |
| DVT | SI/PI, thermal, memory, PCIe, USB4, suspend/resume, OS stress, and EMC pre-scan pass across process and temperature spread |
| PVT | Factory programming, ICT, functional test, BIOS update, serialization, packaging, and pilot yield meet release thresholds |

## 15. Open Production Gaps

- Exact HVM-S1 socket mechanical drawing, load plate design, and heatsink keepout need to be frozen.
- DDR5 QVL, VRM controller model, power-stage model, clock generator, retimer, LAN controller, audio codec, and USB4 controller selections need controlled BOM approval.
- PCIe Gen 5 channel simulation and DDR5 timing simulation must be completed against the final PCB stackup.
- A formal BIOS setup menu, ACPI table set, and OS certification plan remain to be specified.

## 16. References

- AMD Ryzen 9 9950X specifications: https://www.amd.com/en/products/processors/desktops/ryzen/9000-series/amd-ryzen-9-9950x.html
- Intel Core Ultra 9 Processor 285K specifications: https://www.intel.com/content/www/us/en/products/sku/241060/intel-core-ultra-9-processor-285k-36m-cache-up-to-5-70-ghz/specifications.html
- HVM CPU baseline: [HVM CPU Silicon, Packaging, and Electrical Specifications](./01-hvm-cpu-silicon-packaging-electrical-specifications.md)
- HVM green-compute spec: [HVM Green Compute and Performance Specification](./02-hvm-green-compute-specifications.md)
