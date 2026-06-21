# HVM CPU Silicon, Packaging, and Electrical Specifications Manual (Specification Only)

# HVM CPU Silicon, Packaging, and Electrical Specifications Manual

This document defines the physical hardware, silicon-level fabrication, packaging, LGA/BGA pinout configurations, and electrical specifications for the **Hoo Virtual Machine (HVM)** CPU family. These specifications are designed to enable tap-out and manufacturing using industry-standard silicon foundry processes and PCB fabrication technologies.

---

## 1. Architectural Profiles & Multi-Core Topologies

The HVM CPU architecture is manufactured across four standard profiles optimized for distinct compute envelopes, ranging from low-power battery-operated devices to hyper-scale datacenter deployments and real-time robotic controllers.

```
                  +-----------------------------------+
                  |      HVM Core Architecture        |
                  |                                   |
                  |  +-----------------------------+  |
                  |  |  64-Bit RISC Execution Unit |  |
                  |  +-----------------------------+  |
                  |  |    HVM-V VLA SIMD Vector    |  |
                  |  +-----------------------------+  |
                  |  |   HVM-ARC Refcount Unit     |  |
                  |  +-----------------------------+  |
                  +-----------------------------------+
                                    ||
     +-----------------+------------+------------+-----------------+
     |                 |                         |                 |
     v                 v                         v                 v
+----------+      +----------+              +----------+      +----------+
|  Mobile  |      | Desktop  |              |  Server  |      | Robotic  |
| (HVM-M1) |      | (HVM-D1) |              | (HVM-S1) |      | (HVM-R1) |
+----------+      +----------+              +----------+      +----------+
```

### 1.1 Profile Matrix

| Feature | Mobile (`HVM-M1`) | Desktop (`HVM-D1`) | Server (`HVM-S1`) | Robotic / RT (`HVM-R1`) |
| :--- | :--- | :--- | :--- | :--- |
| **Core Topology** | 2 Big (VLA) + 4 Little cores | 8 Big cores (VLA enabled) | 64-128 Big cores per socket; dual-socket capable | 2 Deterministic RT + 2 App cores |
| **Vector Width** | 128-bit `VLEN` | 256-bit `VLEN` | 512-bit `VLEN` (up to 2048) | 128-bit `VLEN` (deterministic execution) |
| **Package Type** | BGA-1218 (ball grid array) | LGA-1700 (socketed) | LGA-4096 (multi-socketed) | LQFP-144 or BGA-256 (ruggedized) |
| **Memory Interface**| LPDDR5/LPDDR5X (PoP / soldered) | Dual-channel DDR5 DIMM | 8-channel DDR5 RDIMM (ECC) | Soldered LPDDR5/LPDDR5X / SRAM |
| **Typical TDP** | 3W – 15W | 65W – 125W | 250W – 800W | 2W – 8W |
| **Primary I/O** | MIPI-DSI, MIPI-CSI, USB 4 | PCIe Gen 5.0, USB 4, SATA | 128 PCIe Gen 5.0 lanes, BMC | CAN-FD, PWM, SPI, I2C, UART |
| **Deployment** | Handhelds, Wearables, Edge | Workstations, Desktop PCs | Cloud Virtualization, HPC | Robotic Arms, Motor Controllers |

---

## 2. Silicon Fabrication & Cache Architecture

All HVM CPU profiles are fabricated on advanced commercial processes to balance high frequency, low leakage currents, and silicon area efficiency.

### 2.1 Silicon Foundry Target
* **Process Node**: TSMC 4nm N4P FinFET CMOS technology.
* **Gate Structure**: High-k Metal Gate (HKMG) with strained silicon channels.
* **Metal Layup**: 13 copper layers (low-k Intermetal Dielectric) + 1 aluminum redistribution layer (RDL).

### 2.2 Core Cache Topology
* **MOESI Protocol**: Hardware-enforced cache coherency across all cores.
* **L1 Cache (Per Core)**:
  - Instruction Cache (I-Cache): 64 KB, 4-way set-associative.
  - Data Cache (D-Cache): 64 KB, 8-way set-associative, write-back.
* **L2 Cache (Per Core)**:
  - Private, 512 KB, 8-way set-associative, unified Instruction/Data.
* **L3 Cache (Shared Cluster)**:
  - Baseline desktop cluster: 24 MB, 16-way set-associative. Mobile variants scale this down for leakage and server variants replicate the cluster-level L3 across chiplets. Connected via a high-speed coherent L2/L3 Ring Bus or server mesh operating at system bus frequency (`CLK_SYS` = 1.6 GHz), yielding up to 512 GB/s bisection bandwidth per baseline cluster.
* **Snoop Control Unit (SCU)**: Directory-based hardware unit to intercept cache line requests and route them directly between L2 caches, bypassing L3/DRAM reads.

---

## 3. Package Configurations & Pinout Mappings

### 3.1 Socket HVM-D1 (LGA-1700) - Desktop Profile
The desktop socketed processor maps to a standard LGA-1700 pad layout. The electrical pinout breakdown is structured to prevent electromagnetic interference (EMI) on high-speed DDR5 and PCIe Gen 5.0 traces.

```
       LGA-1700 Pin Matrix Structure (Bottom View Representation)
     +-------------------------------------------------------------+
     | VSS  VDD  VSS  VDD  VSS  VDD  VSS  VDD  VSS  VDD  VSS  VDD  |
     | VSS [ DDR5 Address / Command ] VSS [ DDR5 DQ Data Lanes ]  |
     | VSS [ PCIe Gen 5 TX Lanes    ] VSS [ PCIe Gen 5 RX Lanes ]  |
     | VSS  GPIO  UART  SPI  I2C  IPI  JTAG  TRNG  VDD_SRAM  VSS   |
     +-------------------------------------------------------------+
```

#### Detailed LGA-1700 Pin Assignment Breakdown

| Pin Count | Group Label | Pin Type / Signal | Target Voltage | Description |
| :---: | :--- | :--- | :---: | :--- |
| **650** | `VSS` | Ground Plane | 0V | Centralized ground returns. Distributed uniformly across high-speed signals for return-path shielding. |
| **320** | `VDD_Core` / `VDD_SRAM` | CPU Core and L1/L2/L3 SRAM Cache Power | 0.70V – 1.15V / 0.90V | Dynamic Voltage and Frequency Scaling (DVFS) rails. Power pins are interleaved with `VSS` to limit parasitics. |
| **280** | `DDR5_DQ_DQS` / `DDR5_CTRL` | DDR5 Memory Interface Lines | 1.1V (PHY) / 1.8V (CMD) | High-speed differential pairs (DQ/DQS) for Channel A and Channel B, plus command and address lines. |
| **220** | `PCIE_TX_RX` / `PCIE_REF` | PCIe Gen 5.0 Lines | 1.0V (PHY) | 16x Lane lanes (PCIe Gen 5.0) + 4x Lane auxiliary lanes for M.2 SSD storage and clock lines. |
| **80** | `LSP_IO` | Low-Speed Peripherals | 1.8V / 3.3V | Includes GPIO pins, UART, SMBus, SPI channels, and the Inter-Processor Interrupt (IPI) lanes. |
| **150** | `ANCL_PWR` / `THERM` | Auxiliary Rails & Monitors | Various | Phase-locked loop (PLL) power, dynamic thermal sensor links, JTAG test interfaces, and startup configurations. |

---

### 3.2 Socket HVM-S1 (LGA-4096) - Server Profile
Designed for multi-socket enterprise configurations. The 4096-pin matrix expands DDR5 memory channels to 8 channels and provides high-speed coherent inter-socket links.

#### LGA-4096 Pin Interface Table

| Signal Block | Pin Count | Direction | Impedance | Description |
| :--- | :---: | :---: | :---: | :--- |
| **VSS (Ground)** | 1500 | Power | 0 $\Omega$ | Massive ground return matrix to minimize bounce during simultaneous switching. |
| **VDD_Core** | 850 | Power | - | Distributed VDD core power rails to support 250W-500W active draw per socket, depending on SKU and cooling envelope. |
| **DDR5 Channels (A-H)**| 1120 | Bi-dir | 40 $\Omega$ | 8-Channel independent DDR5 RDIMM lanes (Data, Clocks, ECC, Command/Address). |
| **PCIe Gen 5.0 (x128 lanes)** | 512 | Differential | 85 $\Omega$ | 128 PCIe lanes implemented as transmit/receive differential-pair groups for expansion slots, storage sleds, and accelerator links. |
| **HVM-Link Coherent** | 64 | Differential | 85 $\Omega$ | High-speed cache-coherent socket-to-socket interconnect lines (MOESI-compliant). |
| **System Management / BMC** | 50 | Input/Output| 50 $\Omega$ | Mapped lines to ASPEED AST2600 BMC, including I2C, SPI, UART, and IPMI interrupt pins. |

---

### 3.3 BGA-1218 - Mobile Profile
The mobile BGA package is optimized for z-height minimization, signal parasitics control, and ultra-high-density System-on-Module (SoM) packaging.

* **Pitch Size**: 0.4 mm ball pitch.
* **Ball Material**: Lead-free SAC305 (Sn96.5/Ag3.0/Cu0.5) solder balls.
* **Memory Architecture**: Supporting Package-on-Package (PoP) placement where LPDDR5/LPDDR5X dies are stacked directly on top of the HVM SoC substrate to save PCB footprint.

---

### 3.4 BGA-256 / LQFP-144 - Robotic & Industrial Profile (`HVM-R1`)
For robotic arm joint controllers, autonomous mobile robots (AMR), and factory automation units, the CPU is packaged in ruggedized, vibration-resistant packages.

#### BGA-256 Pin Mapping Matrix for Robotics

```
      A       B       C       D       E       F       G       H       J       K       L       M       N       P       R       T
  +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
1 | VSS   | CAN0_H| CAN0_L| PWM_0 | PWM_1 | PWM_2 | PWM_3 | GPIO0 | GPIO1 | SPI_CS| SPI_CLK| SPI_MISO| SPI_MOSI| ADC_0 | ADC_1 | VSS   | 1
2 | VDD_IO| CAN1_H| CAN1_L| PWM_4 | PWM_5 | PWM_6 | PWM_7 | GPIO2 | GPIO3 | I2C_SDA| I2C_SCL| UART_TX| UART_RX| ADC_2 | ADC_3 | VDD_IO| 2
3 | VDD_C | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VSS   | VDD_C | 3
...
```

* **LQFP-144 Package (Low-profile Quad Flat Pack)**: Features gull-wing pins spaced at 0.5 mm pitch. Recommended for high-vibration applications (e.g. robotic end-effectors) where visual solder joint inspection is required.
* **CAN-FD Interfaces**: Integrated physical controller layers on-die to interface with industrial actuator networks with zero CPU latency.
* **Redundant Lockstep Layout**: Physical silicon divides the deterministic real-time core pair (`Core0` and `Core1`) to run in **Dual-Core Lockstep (DCLS)** mode, comparing outputs at the execution boundary each clock cycle to detect transient hardware faults instantly.

---

## 4. Voltage Domains & Clock Specifications

### 4.1 DC Electrical Characteristics (Voltage Domains)

The board power regulators must supply clean, noise-isolated power rails. The dynamic voltage ranges are defined below:

| Voltage Rail | Nominal Voltage | Tolerance | Dynamic Range (DVFS) | Max Current (Desktop) | Description |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **`VDD_Core`** | 1.00 V | $\pm$ 3.0% | 0.70 V – 1.15 V | 120 A | Primary power to RISC executing pipelines and vector registers. |
| **`VDD_SRAM`** | 0.90 V | $\pm$ 2.0% | Stable (Static) | 15 A | Cache memory cell arrays (L1, L2, L3). Kept constant to prevent bit-flip soft errors. |
| **`VDD_IO`** | 1.80 V | $\pm$ 5.0% | Stable (Static) | 8 A | General-purpose inputs/outputs, SPI, UART, and JTAG boundaries. |
| **`VDD_MEM_PHY`**| 1.10 V | $\pm$ 3.0% | Stable (Static) | 22 A | Power to high-speed DDR5 memory controllers and PHY transceiver lines. |
| **`VDD_ADC`** | 3.30 V | $\pm$ 1.0% | Stable (Static) | 0.5 A | Clean analog power rail to internal ADC sensors (Robotic/RT variant). |

---

### 4.2 AC Electrical Characteristics (Clock Domains)

To control dynamic power consumption ($P \propto f$), clock trees are managed through multiple Phase-Locked Loops (PLLs) with dynamic clock gating (DCG) down to the module level.

```
                  +--------------------------------+
                  |   Master Ref Crystal (50 MHz)  |
                  +--------------------------------+
                                  ||
                                  v
                  +--------------------------------+
                  |    Renesas 9FGV1006 Clock Gen  |
                  +--------------------------------+
                                  ||
     +-----------------+----------+----------+-----------------+
     | (CLK_CORE)      | (CLK_SYS)           | (CLK_MEM)       | (CLK_RT)
     v                 v                     v                 v
+----------+      +----------+          +----------+      +----------+
| HVM CPU  |      | Coherent |          | DDR5 PHY |      | Real-Time|
| pipeline |      | Ring/Mesh|          | Interf.  |      | Interface|
| (4.2 GHz)|      | (1.6 GHz)|          | (3.2 GHz)|      | (200 MHz)|
+----------+      +----------+          +----------+      +----------+
```

1. **Core Clock Domain (`CLK_CORE`)**:
   - Frequency: 2.4 GHz (base) to 4.2 GHz (boost).
   - Serves integer, vector pipelines, and branch prediction units.
2. **System Ring Clock Domain (`CLK_SYS`)**:
   - Frequency: 1.6 GHz (fixed).
   - Serves Snoop Control Unit, L3 coherent cache ring network, and PCIe interface layers.
3. **Memory Controller Clock Domain (`CLK_MEM`)**:
   - Frequency: 3.2 GHz (driving DDR5-6400).
4. **Real-time Interface Clock (`CLK_RT`)**:
   - Frequency: 200 MHz (robotic controller variant).
   - Dedicated low-jitter clock tree for motor-feedback loops, CAN-FD message controllers, and ADC/DAC samplers.

---

## 5. Physical PCB Manufacturing Layout Rules

To ensure signal integrity and minimize electromagnetic interference (EMI) on high-speed paths (DDR5, PCIe Gen 5.0), motherboard layouts must strictly conform to these routing constraints.

### 5.1 10-Layer Stackup Layup Specification
Manufacturing requires an impedance-controlled stackup using High-Tg FR4 material (e.g. IT-180A, Tg $\ge 170$ °C) or low-loss Megtron 6 laminates.

```
   Layer No.  | Type          | Thickness | Impedance Target     | Purpose
  ------------+---------------+-----------+----------------------+-------------------------------
   Layer 1    | Signal (Top)  | 0.5 oz Cu | 50 Ohm (S), 85 Ohm D | Microstrip High-Speed Signal
   Layer 2    | Ground Plane  | 1.0 oz Cu | Reference GND        | High-Speed Return Path
   Layer 3    | Signal (Int)  | 1.0 oz Cu | 50 Ohm (S), 85 Ohm D | Stripline Routing High-Speed
   Layer 4    | Power Plane   | 1.0 oz Cu | -                    | VDD_Core / VDD_IO Distribution
   Layer 5    | Ground Plane  | 1.0 oz Cu | Reference GND        | Shielding Core Power Planes
   Layer 6    | Ground Plane  | 1.0 oz Cu | Reference GND        | Shielding SRAM Power Planes
   Layer 7    | Power Plane   | 1.0 oz Cu | -                    | VDD_SRAM / VDD_MEM PHY Power
   Layer 8    | Signal (Int)  | 1.0 oz Cu | 50 Ohm (S)           | Low-Speed Signals / UART
   Layer 9    | Ground Plane  | 1.0 oz Cu | Reference GND        | Low-Speed Return Path
   Layer 10   | Signal (Bot)  | 0.5 oz Cu | 50 Ohm (S), 90 Ohm D | Microstrip Low-Speed / USB
```
*(S) = Single-ended Signal, (D) = Differential Pair*

### 5.2 DDR5 Fly-by Topology Trace Constraints
* **Trace Topology**: Fly-by routing layout for address, command, and control lines, terminating with $40\ \Omega$ pull-up resistors at the end of the line.
* **Trace Trace Width & Spacing**: 4 mil width, minimum 8 mil spacing to minimize crosstalk.
* **Length Matching**:
  - Intra-pair (differential clocks, data strobes): Match within $\pm 2$ mils.
  - Data lanes to Strobes (DQ to DQS): Match within $\pm 10$ mils across the entire bus length.
* **Via Budget**: A maximum of 2 signal vias per trace is permitted on memory signals. Unused via stubs must be back-drilled to prevent reflections at speeds above 4800 MT/s.

---

## 6. Functional Safety & Real-Time Controller Interfaces (Robotics)

For robotic applications, the CPU integrates dedicated controller peripherals on-die, communicating via high-priority registers.

```
       HVM-R1 Robotic Controller Integration
      +-----------------------------------------+
      |               HVM CPU Core              |
      |                                         |
      | +-----------+             +-----------+ |
      | | RT Core 0 |<-Compare->  | RT Core 1 | |
      | +-----+-----+             +-----+-----+ |
      |       |                         |       |
      |       +------------+------------+       |
      |                    |                    |
      |                    v                    |
      |      Lockstep Checker (ASIL-D)          |
      +-----------------------------------------+
                           ||
               Peripheral Registers Bridge
                           ||
      +----------+    +----------+    +---------+
      |  CAN-FD  |    | PWM Gen  |    | ADC /   |
      | (10Mbps) |    | (16-bit) |    | Resolver|
      +----------+    +----------+    +---------+
```

### 6.1 Lockstep (DCLS) Core Coordination
The robotic variant implements dual-core lockstep. The redundant CPU cores receive the same inputs with a 2-cycle phase delay (to mitigate common-cause clock faults). A hardware logic comparator checks the output address and data bus lines:
* **Fault Detection**: In the event of a mismatch, execution is halted, outputs are placed in a high-impedance safe state (shutting off power to robotic actuators), and a non-maskable interrupt (NMI) is asserted to the supervisor core.

### 6.2 Low-Latency PWM Generation
* **PWM Resolution**: 16-bit registers with complementary output support.
* **Safety Gating**: Physical fault input pins connected directly to the PWM logic can force the outputs low within 50 nanoseconds, bypassing software layers to prevent robotic joint over-travel.

### 6.3 CAN-FD Bus Controller
* **Data Rate**: Supports ISO 11898-1:2015 protocol up to 10 Mbps payload phase.
* **Buffer Management**: 64 message buffers stored in SRAM with ECC protection, avoiding CPU cache pollution during high-speed fieldbus communications.
