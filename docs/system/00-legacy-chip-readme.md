# HVM Chip Design Docs

This directory contains the design notes for a new HVM-based processor stack.

## Documents

- [HVM CPU Silicon, Packaging, and Electrical Specifications](./01-hvm-cpu-silicon-packaging-electrical-specifications.md)
- [HVM Green Compute and Performance Specification](./02-hvm-green-compute-specifications.md)
- [HVM Mobile SoC and Board Specifications](./03-hvm-mobile-soc-board-detailed-specifications.md)
- [HVM Lightweight System Simulator Design](./10-hvm-lightweight-system-simulator-design.md)

## Scope

- 64-bit RISC-style HVM processor family spanning mobile, desktop, server, and robotics profiles
- Silicon, package, socket, voltage, clock, memory, I/O, and board-level constraints
- Green-compute ISA extensions including HVM-C, HVM-ARC, HVM-L, HVM-MEM, HVM-V, HVM-A, HVM-Prof, HVM-Alloc, compact references, and `ICACHE.RNG`
- Mobile SoC packaging, PMIC, MIPI, UFS, RF, and compact board routing guidance

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
- Production-oriented expanded specs are maintained in this consolidated `docs/hvm/system/` collection.
