# HVM Server Motherboard Specifications

This document defines the `HVM-S1` server CPU family and `HVM-MB-S1` reference server motherboard. The platform targets virtualization hosts, cloud nodes, high-density storage servers, compiler farms, AI CPU front ends, and power-aware datacenter deployments.

The design inherits the HVM server profile: high-core-count sockets, HVM-V 512-bit or wider vectors, HVM-ARC, HVM-C, `ICACHE.RNG`, HVM-L/HVM-MEM runtime-kernel acceleration, HVM-Prof power-aware counters, optional HVM-Alloc and compact references for managed services, HVM-A accelerator doorbells, ECC memory, BMC management, and multi-socket coherency.

## 1. Industry Reference Envelope

`HVM-MB-S1` is sized against current AMD EPYC 9005-class server platforms and SP5 motherboards. AMD EPYC 9005 public material establishes the current high-end server class with very high core counts, DDR5 memory, PCIe Gen 5 I/O, and 2P configurations. Supermicro H13SSL-NT is a practical board reference with single-socket SP5, EPYC 9004/9005 support, 12 DDR5 RDIMM slots, PCIe 5.0 slots, dual 10GBase-T LAN, AST2600 BMC graphics, TPM header, and ATX dimensions. Workstation-class boards such as ASUS Pro WS WRX90E-SAGE SE show that seven physical PCIe x16-class slots and multiple PCIe 5.0 M.2 paths are commercially realistic when the socket lane budget allows it.

## 2. CPU Package Summary

| Item | `HVM-S1` Specification |
| :--- | :--- |
| CPU topology | 64-128 big HVM cores per socket, chiplet-based |
| ISA extensions | HVM-C, HVM-ARC, `ICACHE.RNG`, HVM-L, HVM-MEM, HVM-V 512-bit `VLEN`, HVM-Prof, optional HVM-Alloc, optional compact references, optional 1024-bit server vector SKU |
| Package | HVM-S2 LGA-4096-class server socket |
| Socket count | 1P and 2P reference boards |
| Cache | 64 KB I + 64 KB D L1 per core, 1 MB L2 per core, 256 MB-512 MB shared/chiplet L3 |
| Memory | 8-12 channel DDR5 ECC RDIMM per socket |
| PCIe | 96-128 PCIe Gen 5 lanes per socket |
| Coherent link | HVM-Link for 2P cache-coherent operation |
| TDP | 250 W-500 W per socket, board support up to 800 W combined in 2P |
| Management | BMC-attached power, thermal, crash, and console telemetry |

## 3. Reference Motherboard Classes

| Board | Form Factor | Socketing | Purpose |
| :--- | :--- | :--- | :--- |
| `HVM-MB-S1-ATX` | ATX / EEB | 1P | Development, edge server, storage |
| `HVM-MB-S1-OCP` | OCP 1U/2U sled | 1P | Datacenter deployment |
| `HVM-MB-S2-EEB` | EEB / proprietary server | 2P | High-memory virtualization and HPC |

## 4. Single-Socket Board Specification

| Feature | Requirement |
| :--- | :--- |
| CPU socket | 1x HVM-S2 LGA-4096-class socket |
| Memory slots | 12x DDR5 ECC RDIMM slots, 1 DPC preferred |
| Max memory | 3 TB with 256 GB RDIMMs; higher with future MRDIMM/CXL memory expansion |
| PCIe slots | 3x PCIe Gen 5 x16, 2x PCIe Gen 5 x8 |
| Storage | 2x M.2 PCIe Gen 5 x4, 8x SATA, 2x MCIO or SlimSAS x8 |
| Networking | 2x 10GBase-T onboard, OCP NIC 3.0 option |
| Management | ASPEED AST2600-class BMC, dedicated 1 GbE management port |
| Firmware | 64 MB redundant SPI flash, OpenBMC, HVM-SFI/coreboot server payload |
| Security | TPM 2.0 header, secure boot fuses, measured boot log |

## 5. Dual-Socket Board Specification

| Feature | Requirement |
| :--- | :--- |
| CPU sockets | 2x HVM-S2 sockets |
| Memory slots | 16-24 DDR5 ECC RDIMM slots |
| Coherency | HVM-Link x4 bundles between sockets |
| PCIe routing | 128+ total PCIe Gen 5 lanes after socket-link reservation |
| Accelerator support | 4x PCIe Gen 5 x16 double-width slots or equivalent OAM/CXL risers |
| Storage | 4x U.2/U.3 NVMe Gen 5 x4 front bays minimum |
| Networking | Dual 100 GbE QSFP28 or OCP NIC 3.0 x16 |
| Power | Redundant server PSUs, 12 V busbar or high-current cabled feed |
| Cooling | 1U high-pressure airflow or 2U passive heatsinks; liquid cooling option above 500 W/socket |

## 6. Memory Architecture

- DDR5 ECC RDIMM is mandatory.
- Advanced ECC, memory scrubbing, patrol scrub, demand scrub, and page retirement are required.
- Memory mirroring and lockstep modes are required on 2P boards.
- `HVM-S1` firmware must expose NUMA topology and per-socket memory latency to the OS.
- CXL Type-3 memory expansion should be supported on selected PCIe Gen 5 x16 slots.

## 7. Accelerator and I/O Architecture

| Interface | Requirement |
| :--- | :--- |
| PCIe Gen 5 | 85 ohm differential, retimer support for riser/backplane paths |
| ATS/PRI | Required for HVM-A shared virtual memory with GPUs and DPUs |
| Resizable BAR | Required on accelerator slots |
| CXL | CXL.io and CXL.mem capable slots recommended |
| NIC | 10 GbE onboard minimum, 100/200 GbE through OCP or PCIe for datacenter SKUs |
| Storage | NVMe hot-plug, VMD-like domain grouping, surprise-removal handling |

## 8. Power Delivery and Thermal

| Subsystem | Requirement |
| :--- | :--- |
| CPU VRM | 16+ phase per socket minimum for 400 W-class CPUs |
| Current telemetry | PMBus mandatory |
| Power capping | BMC-enforced socket and board-level caps |
| Fan control | BMC closed-loop fan curves with CPU, DIMM, VRM, and inlet sensors |
| Airflow | Front-to-back server airflow, no consumer radial cooler assumptions |
| Hot spots | DIMM and VRM heatsinks required in 1U/2U chassis |

The board must keep CPU junction temperature at or below 95 C sustained in datacenter ambient conditions and preserve a hard firmware shutdown threshold below the silicon maximum.

## 9. Firmware, BMC, and Observability

- OpenBMC-compatible BMC image with Redfish, IPMI, serial-over-LAN, remote firmware update, and crash dump capture.
- Host firmware publishes ACPI, SMBIOS, NUMA, RAS, PCIe AER, and CXL tables.
- BMC logs VRM telemetry, DIMM thermal warnings, correctable ECC rate, PCIe AER events, and watchdog resets.
- HVM runtime counters for HVM-ARC operations, HVM-L loop use, HVM-MEM pair/hint use, allocation fast-path and slow-path counts, compact-reference decode faults, `ICACHE.RNG` invalidations, vector context switches, sleep/throttle residency, and accelerator doorbells must be exposed through performance-monitoring registers.

## 10. PCB and Signal Integrity Rules

- 14-18 layer PCB stackup for single-socket; 18-24 layers for dual-socket.
- Low-loss laminate for PCIe Gen 5, HVM-Link, and CXL paths.
- DDR5 RDIMM routing uses fly-by topology with per-byte-lane matching and strict via budgets.
- Use retimers/redrivers for any slot, riser, or backplane path exceeding PCIe Gen 5 insertion-loss budget.
- BMC and host SPI flash should be physically separable for recovery and manufacturing programming.

## 11. Validation Checklist

- 24-hour memory stress with ECC injection and patrol scrub enabled.
- PCIe Gen 5 link training across all slots, risers, and NVMe bays.
- 2P coherency stress with HVM-Link fault injection.
- BMC power-cycle, firmware recovery, serial-over-LAN, and Redfish update tests.
- HVM-V vector workloads run under virtualization with lazy context switching.
- HVM-ARC atomic stress passes under high core counts and NUMA migration.
- HVM-L, HVM-MEM, HVM-Prof, and optional HVM-Alloc/compact-reference workloads pass interpreter/JIT/silicon validation before production enablement.
- Accelerator SVM tests validate ATS/PRI, Resizable BAR, and HVM-A doorbell ordering.

## 12. Server Silicon Production Requirements

Server CPUs have a higher RAS and lifecycle bar than desktop parts. The `HVM-S1` production release must include:

- Chiplet, interposer/substrate, and package co-design signoff covering HVM-Link, DDR5, PCIe, CXL, power delivery, and thermal gradients.
- Scan, JTAG, MBIST, LBIST, SRAM repair, redundancy fuse, per-chiplet fuse, and package continuity coverage reports.
- RAS design documentation for ECC, cache parity, poisoned data propagation, machine-check architecture, PCIe AER, memory patrol scrub, page retirement, clock fault handling, and thermal throttling.
- Per-socket and 2P timing/coherency validation, including HVM-Link retry, lane degradation, error injection, and firmware recovery.
- Speed bin and power bin tables with deterministic behavior under board-level power caps and datacenter inlet temperature limits.
- ATE programs for wafer sort, known-good-die classification, package final test, high-temperature test, burn-in, and outgoing quality screening.

## 13. Server Board Production Package

The server board cannot enter PVT until the contract manufacturer has:

- Full fabrication and assembly package: schematic, PCB database, ODB++/IPC-2581, drill, impedance, stackup, assembly drawings, pick-and-place, paste, controlled BOM, AVL, and mechanical chassis fit files.
- ICT and boundary-scan coverage for CPU socket pins, DIMM slots, PCIe slots, BMC, SPI flash, CPLD, VRM telemetry, fan headers, and management LAN.
- Manufacturing firmware for host boot, BMC provisioning, DIMM training, PCIe lane margining, MAC programming, UUID/serial provisioning, TPM ownership, and FRU EEPROM programming.
- Riser, backplane, cable, and chassis compatibility matrix with insertion-loss budget and retimer firmware versions.
- Factory data capture for yield, failing test step, BMC logs, VRM telemetry, DIMM population, CPU stepping, and firmware version.

## 14. Compliance, RAS, and Datacenter Requirements

| Area | Requirement |
| :--- | :--- |
| Safety and EMC | IEC/UL 62368-1, FCC/CE Class A, conducted/radiated emissions, ESD, EFT, and surge tests as system-integrated |
| Environmental | RoHS, REACH, WEEE, data-center material declarations, and regional recycling requirements |
| Manageability | Redfish, IPMI, serial-over-LAN, secure BMC update, audit logs, signed firmware, and recovery image |
| RAS | ECC injection, PCIe AER, CXL error handling, CPU machine checks, thermal trip, watchdog reset, and crash dump capture |
| Security | Secure boot, measured boot, TPM 2.0, BMC credential rotation, debug lockdown, key revocation, and supply-chain traceability |

## 15. EVT, DVT, and PVT Exit Criteria

| Gate | Exit Criteria |
| :--- | :--- |
| EVT | One socket boots, BMC controls power, DDR5 trains, PCIe enumerates, remote console works, and thermal sensors report |
| DVT | Full DIMM population, all slots, hot-plug storage, 2P coherency, RAS injection, thermal stress, and compliance pre-scan pass |
| PVT | Pilot line meets yield target, factory tests are stable, FRU/serial provisioning works, BMC update/recovery passes, and packaging survives shipping tests |

## 16. Open Production Gaps

- HVM-Link electrical specification, connector/socket pin allocation, retry protocol, and compliance test plan must be finalized.
- Final DDR5 channel count, DIMM topology, and RDIMM/MRDIMM support matrix must be frozen before layout.
- CXL support level needs to be split into required and optional SKUs with firmware table requirements.
- BMC hardware root of trust, CPLD responsibilities, and firmware signing flow need complete ownership assignment.
- Chassis airflow, heatsink vendor, riser vendor, and retimer firmware need qualification as a combined platform.

## 17. References

- AMD EPYC 9005 Series processors: https://www.amd.com/en/products/processors/server/epyc/9005-series.html
- Supermicro H13SSL-NT server motherboard: https://www.supermicro.com/en/products/motherboard/h13ssl-nt
- ASUS Pro WS WRX90E-SAGE SE motherboard: https://www.asus.com/motherboards-components/motherboards/workstation/pro-ws-wrx90e-sage-se/techspec/
- HVM CPU baseline: [HVM CPU Silicon, Packaging, and Electrical Specifications](./01-hvm-cpu-silicon-packaging-electrical-specifications.md)
- HVM green-compute spec: [HVM Green Compute and Performance Specification](./02-hvm-green-compute-specifications.md)
