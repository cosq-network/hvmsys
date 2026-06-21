# HVM Mobile SoC and Board Specifications Manual (Specification Only)

# HVM Mobile SoC and Board Specifications Manual

This document details the hardware, packaging, system design, and board layout specifications for the **HVM Mobile SoC (`HVM-M1`)** and the **HVM-MB-Mobile v1.0 Reference Board**. The target envelope is optimized for smartphones, wearable computing, handheld gaming devices, and ultra-low-power edge nodes.

---

## 1. System-on-Chip (SoC) Packaging & Stacking (PoP)

To minimize the PCB trace length, reduce parasitic inductance, and shrink the physical motherboard layout, the HVM Mobile SoC uses a **Package-on-Package (PoP)** memory-stacking configuration.

```
       +---------------------------------------------+
       | LPDDR5/LPDDR5X Memory Die Stack (Top Package)|
       +---------------------------------------------+
                            ||   SAC305 Balls (0.3mm Pitch)
       +---------------------------------------------+
       |     HVM Mobile SoC Die (Bottom Package)     |
       +---------------------------------------------+
                            ||   SAC305 Balls (0.4mm Pitch)
       +---------------------------------------------+
       |        Motherboard PCB (High-Density)       |
       +---------------------------------------------+
```

### 1.1 Bottom Package Specifications (SoC Die)
* **Package Profile**: BGA-1218 (Ball Grid Array).
* **Grid Layout**: 34 x 36 matrix, depopulated center core area.
* **Pitch**: 0.4 mm ball spacing.
* **Body Size**: 15.0 mm x 15.0 mm x 0.8 mm thickness.
* **Underfill Requirement**: High-reliability epoxy underfill to prevent thermal solder fatigue under cyclic loading.

### 1.2 Top Package Specifications (LPDDR5/LPDDR5X Stacking)
* **Package Profile**: BGA-496.
* **Grid Layout**: Dual-channel perimeter ball matrix.
* **Pitch**: 0.3 mm ball spacing.
* **Memory Stacking**: 4x die stack of LPDDR5/LPDDR5X (yielding up to 16 GB capacity for the baseline mobile reference; higher densities are board- and vendor-dependent).

---

## 2. Power Management Infrastructure (PMIC Integration)

The mobile board utilizes a dedicated dynamic Power Management Integrated Circuit (PMIC) matching the high-efficiency sleep/wake states of the HVM core.

### 2.1 Voltage Regulation Rails

| Voltage Rail | Board Regulator Type | Nominal Voltage | Ripple Limit | Max Output Current | Function |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **`VDD_CORE`** | Dynamic Buck Converter | 0.80 V (Nominal) | $\le 10$ mV | 25 A | Core execution pipelines. Scales dynamically from 0.50 V (deep sleep) to 1.10 V (burst turbo). |
| **`VDD_GPU`** | Dynamic Buck Converter | 0.85 V (Nominal) | $\le 15$ mV | 18 A | Integrated lightweight Vulkan graphics engine. |
| **`VDD_MEM_VDD2`**| LDO Regulator | 1.05 V | $\le 5$ mV | 6 A | Memory I/O interfaces and PoP routing. |
| **`VDD_ANA`** | Isolated Ultra-low-noise LDO | 1.80 V | $\le 1$ mV | 1 A | On-die Analog PLLs, ADC monitors, and thermal sensors. |
| **`VDD_IO`** | Standard LDO | 1.80 V | $\pm$ 5% | 2 A | General-purpose peripherals, camera control, and SD-card rails. |

### 2.2 Low-Power States & Wake Pipeline
* **Deep Sleep (C5 State)**: Clock-gating shuts down PLLs, and the core voltage rail (`VDD_CORE`) drops to 0.50V. Current draw drops to $\le 50\ \mu\text{A}$ leakage current.
* **Wake Latency**: Transitions from Deep Sleep to active execution (2.4 GHz) in $\le 120\ \mu\text{s}$, triggered via external PMIC GPIO line (mapped to touchscreen or accelerometer interrupts).

---

## 3. High-Speed Interface & Layout Routing

The HVM Mobile board operates with high-speed serial buses that require strict PCB microstrip layer routing to prevent signal degradation.

```
                  +--------------------------------+
                  |         HVM Mobile SoC         |
                  +--------------------------------+
                        //              \\
        MIPI-CSI Lanes //                \\ MIPI-DSI Lanes
                      //                  \\
     +-------------------+              +-------------------+
     | Dual Camera       |              | Low-Power OLED    |
     | Sensors (4-Lane)  |              | Display (4-Lane)  |
     +-------------------+              +-------------------+
```

### 3.1 MIPI Display Serial Interface (MIPI-DSI)
* **Lane Layout**: 4-lane differential data + 1-lane differential clock.
* **Data Rate**: Supports D-PHY v2.5 up to 2.5 Gbps per lane, enabling driving OLED displays up to $2560 \times 1600$ at 120 Hz.
* **PCB Trace Matching**: Differential impedance must be maintained at $100\ \Omega \pm 10\%$. Intra-pair trace length skew must be matched to within $\pm 0.5$ mm to avoid phase misalignment.

### 3.2 MIPI Camera Serial Interface (MIPI-CSI)
* **Configuration**: 2x independent ports, each featuring 4 data lanes + 1 clock lane.
* **Shielding Rules**: Routed on Layer 3 (Stripline) sandwiched between two solid Ground plane layers (Layer 2 and Layer 4) to protect sensitive analog camera inputs from RF transmitter interference.

### 3.3 Storage Configuration (UFS 4.0)
* **Interface**: 2-lane Universal Flash Storage (UFS) 4.0.
* **Transmission Rate**: Up to 23.2 Gbps per lane.
* **Routing Rule**: Point-to-point routing directly to soldered flash storage die on board with zero vias on data signals.

---

## 4. RF & Wireless Layout Guide (5G, WiFi 6E, BT 5.3)

The reference board integrates a multi-mode wireless modem via a PCIe Gen 4.0 x1 lane.

* **RF Shielding**: The entire RF section (Modem chip, front-end modules, and power amplifiers) must be enclosed inside a dedicated solder-down metal shield can (material: Nickel Silver, thickness: 0.2 mm) to prevent EMI coupling to memory lines.
* **Antenna Feed lines**: Coaxial-like coplanar waveguide with ground layout (CPW-G) routed at $50\ \Omega \pm 5\%$ impedance target. Feed lines must be kept clear of high-speed digital buses by a minimum physical distance of $3.5$ mm.
