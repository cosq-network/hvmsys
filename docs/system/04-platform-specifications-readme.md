# HVM Chip and Board Specifications

This directory contains platform-specific HVM silicon and board specification manuals derived from:

- [HVM CPU Silicon, Packaging, and Electrical Specifications](./01-hvm-cpu-silicon-packaging-electrical-specifications.md)
- [HVM Green Compute and Performance Specification](./02-hvm-green-compute-specifications.md)

The documents split the HVM hardware family into deployable reference platforms:

- [HVM Mobile SoC and Board Production Specifications](./05-hvm-mobile-soc-board-production-specifications.md)
- [HVM Desktop Motherboard Specifications](./06-hvm-desktop-motherboard-specifications.md)
- [HVM Server Motherboard Specifications](./07-hvm-server-motherboard-specifications.md)
- [HVM Robotics SoC and Board Specifications](./08-hvm-robotics-soc-board-specifications.md)
- [HVM Open-Source Firmware and BIOS Options](./09-hvm-open-source-firmware-bios-options.md)

## Common HVM Platform Assumptions

All platforms use the HVM 64-bit RISC execution model, HVM-C compressed instruction support, HVM-ARC retain/release acceleration, fine-grained `ICACHE.RNG` invalidation, HVM-L/HVM-MEM low-overhead loop and memory primitives where the core profile supports them, and the HVM-V vector extension where the thermal envelope allows it. HVM-Alloc, compact object references, HVM-Cap, HVM-Prof, and HVM-NZ remain profile-gated runtime extensions until simulator, RTL, and workload data justify promotion.

| Profile | Primary Form | CPU Package | Memory Class | Target Power | Comparable Industry Class |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `HVM-M1` | Mobile SoC + compact board | BGA / PoP | LPDDR5X | 3 W-15 W | Qualcomm Snapdragon 8 Elite, Apple M-series mobile-class SoCs |
| `HVM-D1` | Desktop motherboard | LGA socket | DDR5 UDIMM | 65 W-170 W | Intel Core Ultra desktop, AMD Ryzen 9000 desktop platforms |
| `HVM-S1` | Server motherboard | LGA / dual socket | DDR5 ECC RDIMM | 250 W-800 W | AMD EPYC 9005 and enterprise SP5 server boards |
| `HVM-R1` | Robotics SoM + carrier board | Rugged BGA / LQFP control island | LPDDR5X + ECC SRAM | 8 W-130 W | NVIDIA Jetson AGX Thor / Orin robotics modules |

## Production Release Gate

The platform documents are production-intent specifications, not a complete foundry or contract-manufacturer handoff by themselves. A CPU, SoC, module, or board may not enter production until the following release package exists and is version controlled.

| Area | Required Release Artifacts |
| :--- | :--- |
| Silicon design | Frozen RTL, lint/CDC/RDC reports, synthesis constraints, timing signoff, power intent, scan/DFT insertion reports, BIST coverage, formal equivalence, security review, and errata log |
| Physical implementation | Floorplan, GDSII/OASIS, LEF/DEF, SPEF, Liberty views, IR-drop/EM signoff, DRC/LVS clean reports, antenna checks, ESD/latch-up signoff, package pin map, and foundry PDK version lock |
| Wafer and package test | Probe test vectors, scan patterns, MBIST/LBIST patterns, speed bin limits, fuse map, wafer sort limits, final-test limits, ATE program version, package assembly drawing, and qualification lot plan |
| Package qualification | JEDEC-style moisture sensitivity, thermal cycling, high-temperature operating life, unbiased/bias humidity, mechanical shock/drop, solder-ball shear, warpage, and board-level reliability reports |
| Board fabrication | Schematic, PCB layout database, Gerber/ODB++/IPC-2581, drill files, stackup drawing, impedance coupon requirements, fabrication notes, assembly drawing, pick-and-place, paste mask, approved vendor list, and controlled BOM |
| Board test | ICT fixture, boundary-scan chain, bed-of-nails access map, flying-probe fallback, functional test image, power-rail margin scripts, firmware flashing procedure, MAC/serial provisioning, and pass/fail limits |
| Firmware and software | Boot ROM hash, secure boot key policy, first-stage bootloader, board support package, device tree/ACPI tables, BMC image where applicable, recovery image, manufacturing test firmware, and signed release manifest |
| Compliance | RoHS, REACH, WEEE where applicable, FCC/CE/UKCA/India WPC/BIS as market requires, IEC/UL safety, EMC pre-scan and final reports, radio modular certifications, and export-control classification |
| Lifecycle | Revision history, PCN/EOL policy, approved substitutions, traceability rules, RMA failure-analysis flow, field firmware update policy, secure key revocation plan, and production yield dashboard |

## Engineering Build Gates

| Gate | Exit Criteria |
| :--- | :--- |
| Architecture Review | ISA, memory model, package, board power, and I/O budgets are frozen for prototype work |
| EVT | First silicon or FPGA/emulation plus first boards boot firmware, train memory, enumerate I/O, and pass smoke tests |
| DVT | Electrical, thermal, SI/PI, reliability, compliance pre-scan, and OS/runtime stress tests pass on representative units |
| PVT | Factory line, programming, calibration, test fixtures, yield limits, traceability, packaging, and logistics pass pilot production |
| MP | Production change control is active, all release artifacts are signed, and quality dashboards meet launch thresholds |

## Industry Baseline References

These manuals use current public product specifications as reference points for realistic board I/O, power, memory, and mechanical envelopes:

- Qualcomm Snapdragon 8 Elite Mobile Platform: https://www.qualcomm.com/smartphones/products/8-series/snapdragon-8-elite-mobile-platform
- Apple M4 chip announcement: https://www.apple.com/newsroom/2024/05/apple-introduces-m4-chip/
- AMD Ryzen 9 9950X specifications: https://www.amd.com/en/products/processors/desktops/ryzen/9000-series/amd-ryzen-9-9950x.html
- Intel Core Ultra 9 Processor 285K specifications: https://www.intel.com/content/www/us/en/products/sku/241060/intel-core-ultra-9-processor-285k-36m-cache-up-to-5-70-ghz/specifications.html
- AMD EPYC 9005 Series processors: https://www.amd.com/en/products/processors/server/epyc/9005-series.html
- ASUS Pro WS WRX90E-SAGE SE motherboard: https://www.asus.com/motherboards-components/motherboards/workstation/pro-ws-wrx90e-sage-se/techspec/
- Supermicro H13SSL-NT server motherboard: https://www.supermicro.com/en/products/motherboard/h13ssl-nt
- NVIDIA Jetson Thor robotics platform: https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-thor/
