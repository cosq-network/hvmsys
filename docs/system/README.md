# HVM System Design Book

This directory consolidates the former `docs/hvm/chip` and `docs/hvm/chips` documentation into a single book-style collection for the HVM CPU, SoC, board, firmware, and simulator work.

## Reading Order

### Part I: Foundation

1. [HVM CPU Silicon, Packaging, and Electrical Specifications](./01-hvm-cpu-silicon-packaging-electrical-specifications.md)
2. [HVM Green Compute Specification](./02-hvm-green-compute-specifications.md)

### Part II: SoC and Board Specifications

3. [HVM Mobile SoC and Board Detailed Specifications](./03-hvm-mobile-soc-board-detailed-specifications.md)
4. [HVM Mobile SoC and Board Production Specifications](./05-hvm-mobile-soc-board-production-specifications.md)
5. [HVM Desktop Motherboard Specifications](./06-hvm-desktop-motherboard-specifications.md)
6. [HVM Server Motherboard Specifications](./07-hvm-server-motherboard-specifications.md)
7. [HVM Robotics SoC and Board Specifications](./08-hvm-robotics-soc-board-specifications.md)

### Part III: Firmware and Simulation

8. [HVM Open-Source Firmware and BIOS Options](./09-hvm-open-source-firmware-bios-options.md)
9. [HVM Lightweight System Simulator Design](./10-hvm-lightweight-system-simulator-design.md)
10. [HVM System Architecture Comparison](./11-hvm-system-architecture-comparison.md)
11. [HVM System Memory Map and ABI](./12-hvm-memory-map-and-abi.md)

### Appendix

- [Legacy Chip Directory README](./00-legacy-chip-readme.md)
- [Platform Specifications README](./04-platform-specifications-readme.md)

## Profile Naming

| Profile | Package / Board Interface | Purpose |
| :--- | :--- | :--- |
| `HVM-M1` | BGA-1218 mobile SoC with LPDDR PoP option | Handhelds, wearables, and low-power edge devices |
| `HVM-D1` | LGA-1700-class desktop socket | Desktop PCs, workstations, and developer boards |
| `HVM-S1` | LGA-4096-class server socket | Servers, virtualization hosts, and HPC nodes |
| `HVM-R1` | BGA-256 or LQFP-144 rugged package | Robotics and deterministic real-time controllers |

## Canonical References

- `docs/hvm/hvm-spec.md`
- `docs/hvm/instructions.md`
- `docs/hvm/hvm_instruction_set.csv`
- `docs/hvm/hvm_register_set.csv`
