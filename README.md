# ESP32-S3 6-Axis Closed-Loop Stepper & Kinematics Motion Controller

[![Firmware](https://img.shields.io/badge/Firmware-v2.2.0--DH--CONFIRMED-238636.svg)](https://github.com/)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-388bfd.svg)](https://espressif.com/)
[![Framework](https://img.shields.io/badge/Framework-Arduino%20%2F%20PlatformIO-d29922.svg)](https://platformio.org/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

An industrial-grade, real-time **6-Axis Closed-Loop Stepper Controller** and **6-DOF Articulated Robotic Arm Motion Controller** powered by the **ESP32-S3**. Designed for NEMA stepper motors, TMC2209 ultra-silent drivers over dual multi-drop UART buses, and AS5600 magnetic absolute encoders multiplexed over I2C (PCA9548A).

---

## Key Features

- **Standard Denavit-Hartenberg (DH) Kinematics**:
  - Confirmed and physically verified Standard DH model ($d_1=139\text{mm}, a_2=138\text{mm}, a_3=88\text{mm}, d_4=126\text{mm}, d_{6,\text{tool}}=20\text{mm}$).
  - Analytical closed-form **Forward Kinematics (FK)** and **Inverse Kinematics (IK)** for sub-millisecond real-time Cartesian Tool Center Point (TCP) control.
- **Dual-Core FreeRTOS Architecture**:
  - **Core 0 (500Hz Sensor Task)**: Non-blocking sampling of 6x AS5600 absolute encoders through PCA9548A with exponential low-pass filtering and I2C bus auto-recovery.
  - **Core 1 (100Hz Motion Loop & Web Server)**: Real-time closed-loop position tracking, Schmitt deadband holding, trajectory execution, and Web REST API.
  - **Hardware Timers (`esp_timer`)**: Sub-microsecond step pulse generation with dynamic acceleration and deceleration ramping.
- **Precision Closed-Loop Control**:
  - **Schmitt-Trigger Deadband** (`Enter=0.3°`, `Exit=0.8°`) eliminating acoustic hunting and motor chatter.
  - **16-Point Look-Up Table (LUT)** non-linear magnetic calibration with sub-0.05° linear interpolation.
  - **Dual-Endstop Major Arc Homing** with stall detection and center alignment, plus instant software zeroing (`Set 0.00° Here`).
  - **Runaway Angle Protection**: Automatically detects reversed wiring or inverted direction and halts the axis safely.
- **Synchronized Multi-Axis Trajectory & Automation**:
  - **Time-Scaled Synchronized Arrival** ($T_{\text{sync}}$): Dynamic velocity scaling ensures all 6 joints finish their trajectory simultaneously.
  - **Trajectory Waypoint Player**: Program, record, and execute multi-pose automation sequences with loop and dwell timers.
- **Industrial Web Dashboard & REST API**:
  - **Self-Contained Web UI**: Zero external CDN dependencies; runs 100% offline in AP and STA Wi-Fi modes.
  - **High-Density Responsive Design**: Slate dark theme (`#0a0e17`), animated SVG vector dials, 6-axis live overview grid, 2D kinematic arm schematic, and live hardware telemetry.
  - **Full REST API & Serial CLI (G-code / ASCII)**: Complete programmatic control.

---

## Standard DH Parameters Table (Confirmed)

Transformation convention: $T_i = R_z(\theta_i) \cdot T_z(d_i) \cdot T_x(a_{i-1}) \cdot R_x(\alpha_{i-1})$

| Joint ($i$) | $a_{i-1}$ (mm) | $\alpha_{i-1}$ (deg) | $d_i$ (mm) | $\theta_i$ (DH convention) | Offset ($\theta_{\text{DH}} = \theta_{\text{enc}} + \text{offset}$) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **1** | $0.0$ | $0.0^\circ$ | $139.0$ | $\theta_1$ | $+0.0^\circ$ |
| **2** | $0.0$ | $-90.0^\circ$ | $0.0$ | $\theta_2$ | **$-90.0^\circ$** |
| **3** | $138.0$ | $0.0^\circ$ | $0.0$ | $\theta_3$ | $+0.0^\circ$ |
| **4** | $88.0$ | $-90.0^\circ$ | $126.0$ | $\theta_4$ | $+0.0^\circ$ |
| **5** | $0.0$ | $+90.0^\circ$ | $0.0$ | $\theta_5$ | $+0.0^\circ$ |
| **6** | $0.0$ | $-90.0^\circ$ | $0.0$ | $\theta_6$ | $+0.0^\circ$ |

- **Wrist Center**: Origin of Frame 4 ($J_4, J_5, J_6$ intersect at this physical point).
- **Physical Home Pose** ($\theta_{\text{enc}} = [0, 0, 0, 0, 0, 0]$):
  - Wrist Center $= (126.0, 0.0, 365.0)\text{ mm}$ ($139 + 138 + 88 = 365\text{mm}$).
  - Tool TCP ($D6\_TOOL = 20.0\text{mm}$) $= (126.0, 0.0, 385.0)\text{ mm}$.

---

## Hardware Pinout & Wiring

### ESP32-S3 Pin Assignment

> [!IMPORTANT]
> **GPIO 4 is strictly excluded** to prevent boot-loop strapping conflicts on ESP32-S3 DevKitC-1.

| Peripheral / Signal | ESP32-S3 GPIO | Description |
| :--- | :--- | :--- |
| **I2C SDA** | **GPIO 8** | PCA9548A Multiplexer (`0x70`) Data Line |
| **I2C SCL** | **GPIO 9** | PCA9548A Multiplexer (`0x70`) Clock Line (400kHz) |
| **UART 1 TX / RX** | **TX: GPIO 15, RX: GPIO 16** | Hardware `Serial1` for TMC2209 Motors 1–4 |
| **UART 2 TX / RX** | **TX: GPIO 17, RX: GPIO 18** | Hardware `Serial2` for TMC2209 Motors 5–6 |
| **STEP Motor 0 (Joint 1)** | **GPIO 1** | Base Yaw Stepper Pulse |
| **STEP Motor 1 (Joint 2)** | **GPIO 2** | Shoulder Pitch Stepper Pulse |
| **STEP Motor 2 (Joint 3)** | **GPIO 41** | Elbow Pitch Stepper Pulse |
| **STEP Motor 3 (Joint 4)** | **GPIO 42** | Wrist Roll Stepper Pulse |
| **STEP Motor 4 (Joint 5)** | **GPIO 21** | Wrist Pitch Stepper Pulse |
| **STEP Motor 5 (Joint 6)** | **GPIO 14** | Flange Roll Stepper Pulse |

### TMC2209 Multi-Drop UART Strapping

| Joint Instance | Hardware Port | MS1 Pin | MS2 Pin | 2-bit Address | Decimal Address |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Motor 0 (Joint 1)** | `Serial1` | GND | GND | `0b00` | 0 |
| **Motor 1 (Joint 2)** | `Serial1` | 3.3V | GND | `0b01` | 1 |
| **Motor 2 (Joint 3)** | `Serial1` | GND | 3.3V | `0b10` | 2 |
| **Motor 3 (Joint 4)** | `Serial1` | 3.3V | 3.3V | `0b11` | 3 |
| **Motor 4 (Joint 5)** | `Serial2` | GND | GND | `0b00` | 0 |
| **Motor 5 (Joint 6)** | `Serial2` | 3.3V | GND | `0b01` | 1 |

---

## Quick Start Guide

### 1. Build and Flash Firmware

```bash
# Build firmware
pio run

# Flash to ESP32-S3 over USB
pio run --target upload

# Open Serial Monitor (115200 baud)
pio device monitor
```

### 2. Connect to Web Dashboard

1. **Access Point (Default)**: Connect to Wi-Fi SSID `NEMA-6AXIS-CONTROLLER` (Password: `12345678`).
2. Open your browser to `http://192.168.4.1` or `http://nema.local`.
3. **Station Mode (Local LAN)**: In the Web UI, scan Wi-Fi networks, enter your credentials, and click **Lưu & Kết Nối**.

---

## Serial Command Line Interface (CLI)

| Command | Example | Description |
| :--- | :--- | :--- |
| `M<1-6> <angle>` | `M1 45.0` | Move Joint 1 to +45.00° target angle |
| `M<1-6> JOG <delta>` | `M2 JOG -5.0` | Jog Joint 2 by -5.00° |
| `M<1-6> STEP <steps> [spd]`| `M3 STEP 400 300` | Move 400 raw microsteps at 300us interval |
| `M<1-6> RUN CW` / `CCW` | `M1 RUN CW` | Spin motor continuously in CW direction |
| `M<1-6> STOP` | `M1 STOP` | Stop single joint |
| `M<1-6> HOME` | `M2 HOME` | Execute center homing with stall detection |
| `M<1-6> ZERO` | `M1 ZERO` | Set current position as 0.00° home reference |
| `M<1-6> CALIB` | `M1 CALIB` | Run 16-point LUT non-linear auto-calibration |
| `ALL <j1>..<j6> [time]` | `ALL 0 45 -30 90 0 0 2.5`| Coordinated 6-axis move finishing at 2.5s |
| `ALL HOME` / `G28` | `G28` | Run homing on all 6 axes |
| `ALL ZERO` | `ALL ZERO` | Zero all 6 axes at current pose |
| `IK <x> <y> <z> [r] [p] [y] [t]`| `IK 126 0 385 0 0 0 2.0`| Move TCP to Cartesian coordinate (mm) |
| `POSE` | `POSE` | Print current Cartesian Tool Center Point pose |
| `WP ADD <name>` | `WP ADD Pick1` | Save current joint angles as a Waypoint |
| `WP START [LOOP]` | `WP START LOOP` | Execute automated trajectory playback |
| `STOP` / `M112` | `STOP` | **EMERGENCY STOP ALL 6 AXES IMMEDIATELY** |
| `STATUS` / `M114` | `STATUS` | Print full system diagnostic summary |
| `TEST UART` | `TEST UART` | Query all 6 TMC2209 driver registers |

---

## License

This project is licensed under the MIT License.
