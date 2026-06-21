# HVM Robotics SoC and Board Specifications

This document defines the `HVM-R1` robotics SoC and `HVM-RB-R1` reference robotics carrier board. The profile targets autonomous mobile robots, humanoid controllers, industrial arms, motor drives, machine vision nodes, and safety-critical embedded controllers.

The design combines deterministic HVM real-time cores with application cores, HVM-C compressed code density, HVM-L bounded hardware loops for RT firmware, HVM-ARC runtime acceleration on application cores, HVM-MEM pair memory operations, optional HVM-V vectors, accelerator doorbells, lockstep safety logic, CAN-FD, PWM, ADC, encoder, and industrial Ethernet connectivity.

## 1. Industry Reference Envelope

`HVM-R1` is sized against robotics modules such as NVIDIA Jetson AGX Thor and Jetson-class carrier boards. NVIDIA's public Jetson Thor material lists Blackwell GPU acceleration, Arm Neoverse CPU cores, 128 GB LPDDR5X on the high-end module, 273 GB/s memory bandwidth, 40 W-130 W module power ranges, CAN headers on the developer kit, and 25 GbE-class networking options. The HVM robotics design uses that module class as an AI/perception reference while adding deterministic lockstep motor-control behavior from the HVM CPU specification.

## 2. SoC Summary

| Item | `HVM-R1` Specification |
| :--- | :--- |
| CPU topology | 2 deterministic real-time cores + 2-8 application cores |
| Safety mode | Dual-Core Lockstep (DCLS) for RT core pair |
| ISA extensions | HVM-C, HVM-L deterministic RT subset, HVM-ARC, HVM-MEM, `ICACHE.RNG`; HVM-V 128-bit on application cores |
| Process target | 6 nm-12 nm automotive/industrial-qualified node or N4P for AI-heavy module |
| Package | BGA-699/BGA-1024 robotics module or BGA-256/LQFP-144 control SKU |
| Memory | 8 GB-64 GB LPDDR5X plus on-die ECC SRAM for RT islands |
| AI accelerator | Optional 50-250 TOPS edge AI block or external GPU module via PCIe |
| Industrial I/O | CAN-FD, EtherCAT/TSN Ethernet, PWM, SPI, I2C, UART, ADC, QEI |
| Power | 8 W-30 W control/perception module; 40 W-130 W high-AI module |
| Temperature | -40 C to 85 C industrial operating range target |

## 3. Safety and Real-Time Architecture

```
+----------------------------------------------------+
| HVM-R1 SoC                                         |
|                                                    |
|  +-------------------+   compare   +-------------+ |
|  | RT Core 0         |<----------->| RT Core 1   | |
|  | deterministic     |             | lockstep    | |
|  +---------+---------+             +------+------+ |
|            |                              |        |
|            +--------------+---------------+        |
|                           v                        |
|              Safety island and fault manager       |
|                           |                        |
|        +------------------+------------------+     |
|        | PWM | CAN-FD | ADC | QEI | GPIO     |     |
|        +-------------------------------------+     |
|                                                    |
|  +-------------------+     +-------------------+   |
|  | App HVM cores     |     | AI/GPU/ISP block  |   |
|  +-------------------+     +-------------------+   |
+----------------------------------------------------+
```

Safety requirements:

- RT cores execute in lockstep with a 2-cycle phase delay to reduce common-cause clock fault sensitivity.
- Fault comparator checks retire-stage address, data, control-flow, and peripheral-write signatures.
- On mismatch, PWM outputs enter high-impedance or safe-low state within 50 ns.
- Safety island raises a non-maskable interrupt to the application cluster and latches a tamper/fault register for postmortem logs.
- ECC is required on RT SRAM and CAN message RAM.

## 4. Real-Time Peripheral Set

| Peripheral | Requirement |
| :--- | :--- |
| CAN-FD | 4-8 controllers, ISO 11898-1:2015, up to 10 Mbps data phase |
| Ethernet | 2x GbE TSN, optional 10/25 GbE on AI module |
| PWM | 16-32 channels, 16-bit or better, complementary outputs, dead-time insertion |
| ADC | 12-16 bit, 2 MSPS aggregate minimum, resolver/current-sense modes |
| QEI / encoder | 8 channels quadrature encoder input, timestamp capture |
| SPI | 4 controllers, one boot-capable |
| I2C / I3C | 4 controllers for sensors and PMIC |
| UART | 6 ports, including isolated debug console |
| GPIO | 96+ lines with interrupt, debounce, and safety state controls |
| Time sync | IEEE 1588 PTP hardware timestamping |

## 5. Board Classes

| Board | Purpose | Mechanical Target |
| :--- | :--- | :--- |
| `HVM-RB-R1-Control` | Motor-control and safety island validation | 100 mm x 80 mm industrial carrier |
| `HVM-RB-R1-Perception` | Robotics AI and sensor fusion | 100 mm x 87 mm module-class board |
| `HVM-RB-R1-Carrier` | Production robot controller carrier | Custom, vibration-isolated, connectorized |

## 6. Module and Carrier Board Specification

| Feature | Requirement |
| :--- | :--- |
| Module connector | High-density board-to-board or 699+ pin mezzanine |
| Carrier input power | 9 V-36 V DC, reverse polarity and surge protection |
| Functional safety supply | Independent safety island regulator and watchdog supervisor |
| Memory | LPDDR5X on module, ECC SRAM on SoC |
| Storage | 1x M.2 NVMe, 1x eMMC/UFS boot storage |
| Camera | 4x MIPI-CSI 4-lane or GMSL/FPD-Link bridge headers |
| Networking | 2x GbE TSN, 1x 10/25 GbE optional, isolated PHYs for factory networks |
| Fieldbus | CAN-FD, RS-485, EtherCAT through MAC/PHY option |
| Motor control | Isolated PWM outputs, ADC current feedback, encoder inputs |
| Debug | Isolated UART, JTAG, SWD-style safety island debug, recovery strap pins |

## 7. Power and Thermal

| Rail | Requirement |
| :--- | :--- |
| `VDD_RT` | Fixed low-noise rail for deterministic RT cores |
| `VDD_APP` | DVFS rail for application cores |
| `VDD_AI_GPU` | Independent rail for accelerator and perception workloads |
| `VDD_SAFETY` | Independent always-on safety island supply |
| `VDD_IO_FIELD` | Isolated or protected field I/O supply |

Power modes:

- Safety monitor only: below 500 mW.
- Real-time control active: 2 W-8 W.
- Perception and planning active: 15 W-30 W.
- High-AI robotics module: 40 W-130 W depending on accelerator SKU.

Thermal requirements:

- Passive heatsink for 15 W industrial control use.
- Heat spreader plus active airflow for 30 W perception use.
- Heat pipe or cold plate option for 70 W-130 W AI modules.

## 8. Robotics Firmware and Runtime

- Boot ROM validates safety firmware before application firmware.
- RT firmware starts first and owns safe-state defaults for PWM and field outputs.
- Application firmware boots Linux or an RTOS partition using HVM-SFI supervisor services.
- Device tree must describe safety island ownership, RT timers, fieldbus interrupts, and DMA isolation domains.
- HVM-ARC is enabled on application cores; RT cores may restrict dynamic allocation and use ARC only in bounded regions.
- HVM-L is enabled on RT cores for bounded counted loops; unbounded vector, division, allocation fast-path, and compact-reference operations are prohibited in hard real-time regions unless a board-specific worst-case execution-time contract exists.
- HVM-MEM pair load/store operations may be used in RT firmware only where alignment and timing are bounded.
- HVM-V is enabled for perception, filtering, and sensor-fusion kernels on application cores.

## 9. Layout and Reliability Rules

- Separate noisy motor power, field I/O, and compute power planes.
- Use digital isolation for external CAN/RS-485 where the robot power domain is not shared.
- Keep ADC and current-sense traces away from PWM gate-drive loops.
- Add TVS protection on all off-board connectors.
- Use locking connectors and board retention for vibration.
- Conformal coating is recommended for industrial humidity and dust environments.
- Safety fault pins must route directly to PWM gate-disable logic without software dependency.

## 10. Validation Checklist

- Lockstep mismatch injection triggers safe state within the specified latency.
- PWM fault input disables outputs within 50 ns at connector pins.
- CAN-FD passes bus saturation with ECC-protected message buffers.
- PTP timestamping passes sub-microsecond synchronization tests.
- HVM-V perception kernels run concurrently with RT motor loops without deadline misses.
- HVM-L deterministic-loop tests and HVM-MEM bounded-memory tests pass under lockstep simulation and hardware validation.
- Power-loss and brownout tests preserve safe output state.
- Thermal cycling validates solder joints and connector retention.
- EMI pre-scan passes with motors switching under realistic cable loads.

## 11. Robotics Silicon and Module Production Requirements

The `HVM-R1` platform must be released as a safety-relevant industrial product, not only as a compute module:

- Silicon signoff must include RTL lint, CDC/RDC, formal equivalence, low-power checks, STA, DRC/LVS, IR-drop, EM, ESD, latch-up, scan, MBIST, LBIST, and safety mechanism coverage.
- Safety island DFT must allow lockstep comparator test, PWM kill-path test, ADC self-test, watchdog test, clock monitor test, voltage monitor test, ECC injection, and CAN message RAM fault injection.
- Package qualification must cover industrial temperature, vibration, mechanical shock, thermal cycling, humidity, solder fatigue, and connector retention.
- Safety documentation must include hazard analysis, failure mode effects analysis, diagnostic coverage estimates, safe-state definition, fault reaction timing, safety manual, and errata.
- Security lifecycle must define production debug lock, RMA unlock, secure boot root keys, firmware rollback prevention, device identity, and field update policy.

## 12. Carrier Board Production Package

The robotics carrier board release must include:

- Schematic, PCB database, Gerber/ODB++/IPC-2581, drill files, stackup, impedance plan, assembly drawing, pick-and-place, paste masks, conformal coating mask, controlled BOM, and mechanical enclosure model.
- Isolation boundary drawings, creepage/clearance rules, surge/ESD protection ratings, connector pinout, cable harness drawings, and grounding strategy.
- ICT/boundary-scan fixture files, motor-output safety test fixture, CAN/RS-485/Ethernet loopback fixtures, ADC calibration fixture, and final functional test firmware.
- Factory provisioning flow for serial number, device certificate, safety firmware version, calibration constants, MAC addresses, and production debug lock.

## 13. Functional Safety, Compliance, and Reliability

| Area | Requirement |
| :--- | :--- |
| Functional safety | IEC 61508 / ISO 13849 planning for industrial use; ISO 26262 planning if automotive robots are targeted |
| EMC and immunity | IEC 61000-4 ESD/EFT/surge/immunity, CISPR 11/32 emissions, and motor-load EMI testing |
| Industrial environment | -40 C to 85 C operation, vibration, shock, humidity, conformal coating, and connector retention |
| Fieldbus | CAN-FD interoperability, EtherCAT/TSN conformance where populated, IEEE 1588 timestamp validation |
| Security | Secure boot, signed safety firmware, debug lock, key revocation, and authenticated field update |

## 14. EVT, DVT, and PVT Exit Criteria

| Gate | Exit Criteria |
| :--- | :--- |
| EVT | Board powers safely, RT cores boot first, PWM defaults safe, app cores boot, and fieldbus loopback passes |
| DVT | Lockstep injection, brownout, watchdog, thermal, vibration, EMC pre-scan, motor-load, and deadline tests pass |
| PVT | Factory calibration, safety test fixtures, serialization, conformal coating, enclosure assembly, and pilot yield pass |

## 15. Open Production Gaps

- The target safety integrity level must be selected before final silicon and board safety analysis.
- Exact isolated transceivers, gate drivers, ADC front end, Ethernet PHYs, and connector system must be chosen for the production BOM.
- High-AI module SKUs need a separate thermal design and accelerator software support matrix.
- Motor-load EMC testing depends on representative cable lengths, drive stages, and enclosure grounding.
- A safety manual and field-update policy must be completed before customer evaluation units.

## 16. References

- NVIDIA Jetson Thor robotics platform: https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-thor/
- HVM CPU baseline: [HVM CPU Silicon, Packaging, and Electrical Specifications](./01-hvm-cpu-silicon-packaging-electrical-specifications.md)
- HVM green-compute proposal: [HVM Green Compute and Performance Proposal](./02-hvm-green-compute-performance-proposal.md)
