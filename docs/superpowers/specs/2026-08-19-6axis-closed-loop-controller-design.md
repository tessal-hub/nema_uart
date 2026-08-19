# Design Document: 6-Axis Closed-Loop Stepper & Encoder Controller with IK Architecture

- **Date**: 2026-08-19
- **Target Platform**: ESP32-S3 DevKitC-1 (Arduino Framework / PlatformIO)
- **Actuators & Sensors**: 6x NEMA Stepper Motors (TMC2209 Drivers) + 6x AS5600 Magnetic Absolute Encoders (PCA9548A I2C Multiplexer)

---

## 1. System Overview & Objectives

The goal is to scale the existing single-axis closed-loop controller into a modular, high-precision **6-axis motion control system** capable of:
1. **Independent Closed-Loop Axis Control**: Each joint maintains independent angle tracking, homing against endstops, 16-point non-linear calibration look-up tables (LUT), gear ratio compensation, and Schmitt-trigger position holding.
2. **Dedicated Individual Manual Mode**: Direct control per motor for manual jogging, discrete raw stepping, continuous spinning, and driver enable/free-shaft toggle.
3. **Synchronized Multi-Axis Trajectory**: Coordinated movement across all 6 axes where all joints arrive at their target angles simultaneously via dynamic velocity scaling ($T_{\text{sync}}$).
4. **Inverse Kinematics (IK) Extension Hook**: A dedicated coordinator-level interface ready to accept Cartesian Tool Center Point (TCP) targets $(X, Y, Z, \text{Roll}, \text{Pitch}, \text{Yaw})$ and resolve them to joint angles with solver parameters (iterations, tolerance).
5. **Modern Web UI & Serial CLI**: A responsive dashboard featuring multi-axis overview gauges, per-joint manual control tabs, Wi-Fi provisioning, and emergency stop.

---

## 2. Layered Architecture

```
+-----------------------------------------------------------------------------------+
|                                 APPLICATION LAYER                                 |
|  - Web UI Dashboard (6-Axis Live Overview + M1-M6 Independent Manual Control Tabs)|
|  - Serial Command Line Interface (CLI)                                            |
|  - REST API (/api/motor/..., /api/all/..., /api/wifi/...)                         |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                        COORDINATOR & KINEMATICS LAYER                             |
|  - MultiAxisManager: Time-scaled synchronized trajectory generation               |
|  - Inverse Kinematics Hook: [X,Y,Z,Rx,Ry,Rz] -> [J1..J6] Joint Angle Solver       |
|  - Global Emergency Stop & State Aggregation                                      |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                        JOINT-LEVEL CLOSED-LOOP CONTROL                            |
|  - MotionController[6]: PID/Schmitt deadband loop, 16-point LUT calib, homing     |
|  - Sensor Manager: Core 0 500Hz Task scanning PCA9548A channels 0..5              |
|  - Motor[6]: Hardware esp_timer step generation + TMC2209 UART telemetry         |
+-----------------------------------------------------------------------------------+
```

---

## 3. Hardware Pinout, Communication & Strapping

### 3.1 GPIO Pin Assignment (ESP32-S3)
* **Design Constraint**: GPIO 4 is strictly excluded (causes boot loops on this ESP32-S3 DevKitC-1 variant).
* **JTAG Note**: GPIO 41 and 42 are used as STEP pins; if onboard USB-JTAG debugging is active, alternate GPIOs (e.g. GPIO 10, 11) can be substituted in `config.h`.

| Signal / Peripheral | ESP32-S3 GPIO | Notes |
| :--- | :--- | :--- |
| **I2C SDA** | **GPIO 8** | Validated on PCA9548A (0x70) |
| **I2C SCL** | **GPIO 9** | Validated on PCA9548A (0x70) |
| **TMC2209 UART Bus 1 (TX/RX)** | **TX: GPIO 15, RX: GPIO 16** | Hardware `Serial1` for Motors 1–4 |
| **TMC2209 UART Bus 2 (TX/RX)** | **TX: GPIO 17, RX: GPIO 18** | Hardware `Serial2` for Motors 5–6 |
| **STEP Pin Motor 0 (J1)** | **GPIO 1** | Dedicated output |
| **STEP Pin Motor 1 (J2)** | **GPIO 2** | Dedicated output |
| **STEP Pin Motor 2 (J3)** | **GPIO 41** | Dedicated output (Alternative: GPIO 10) |
| **STEP Pin Motor 3 (J4)** | **GPIO 42** | Dedicated output (Alternative: GPIO 11) |
| **STEP Pin Motor 4 (J5)** | **GPIO 21** | Dedicated output |
| **STEP Pin Motor 5 (J6)** | **GPIO 14** | Dedicated output |

### 3.2 TMC2209 Multi-Drop UART Strapping Table
TMC2209 drivers share a single-wire UART bus per port (TX and RX tied via 1k resistor). Each driver's address is physically configured via MS1 and MS2 pins:

| Driver Instance | Hardware Port | MS1 Pin | MS2 Pin | 2-bit UART Address | Decimal Address |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Motor 0 (Joint 1)** | `Serial1` | GND | GND | `0b00` | 0 |
| **Motor 1 (Joint 2)** | `Serial1` | VCC (3.3V) | GND | `0b01` | 1 |
| **Motor 2 (Joint 3)** | `Serial1` | GND | VCC (3.3V) | `0b10` | 2 |
| **Motor 3 (Joint 4)** | `Serial1` | VCC (3.3V) | VCC (3.3V) | `0b11` | 3 |
| **Motor 4 (Joint 5)** | `Serial2` | GND | GND | `0b00` | 0 |
| **Motor 5 (Joint 6)** | `Serial2` | VCC (3.3V) | GND | `0b01` | 1 |

### 3.3 AS5600 I2C Multiplexer Channel Mapping (PCA9548A @ 0x70)
* Joint 1 Encoder $\rightarrow$ PCA Channel 0
* Joint 2 Encoder $\rightarrow$ PCA Channel 1
* Joint 3 Encoder $\rightarrow$ PCA Channel 2
* Joint 4 Encoder $\rightarrow$ PCA Channel 3
* Joint 5 Encoder $\rightarrow$ PCA Channel 4
* Joint 6 Encoder $\rightarrow$ PCA Channel 5

---

## 4. Concurrency & RTOS Execution Model

```
+-----------------------------------------------------------------------------+
|                                ESP32-S3 CORE 0                              |
|  - SensorScanTask (Priority 2, 500Hz / 2ms period)                          |
|    Sequentially switches PCA9548A channels 0..5, reads raw angles, applies |
|    exponential smoothing filter (ALPHA=0.2), updates shared filtered_angles |
|    under dataMutex.                                                         |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                ESP32-S3 CORE 1                              |
|  - MotionControlTask (Priority 3, 100Hz / 10ms fixed via vTaskDelayUntil)  |
|    Runs closed-loop position tracking, calculates angle error, evaluates    |
|    Schmitt-trigger deadband, and sets motor step counts.                     |
|                                                                             |
|  - Background Arduino loop() (Priority 1)                                   |
|    Executes WebServerManager (handles REST API & Web UI) and Serial CLI.     |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                           HARDWARE ISR / esp_timer                          |
|  - 6x independent microsecond step timers                                   |
|  - ISR Rule: Only toggles STEP pin and decrements volatile stepsRemaining.  |
|  - Thread-Safe Stop Sequence: Disable esp_timer -> Clear stepsRemaining = 0 |
|    -> Reset state flags.                                                    |
+-----------------------------------------------------------------------------+
```

---

## 5. Control Algorithms & Multi-Axis Synchronization

### 5.1 Schmitt-Trigger Deadband (`MotionController`)
To prevent hunting, acoustic resonance, and excessive motor current chatter:
* **`DEADBAND_ENTER` ($0.3^\circ$)**: When the joint error $|\Delta \theta| \le 0.3^\circ$, the controller decelerates to a stop and enters closed-loop holding.
* **`DEADBAND_EXIT` ($0.8^\circ$)**: Correction is only reactivated if an external disturbance forces $|\Delta \theta| > 0.8^\circ$.

### 5.2 Time-Scaled Synchronized Arrival (`MultiAxisManager`)
When commanding a multi-joint move to $[ \theta_0^*, \theta_1^*, \theta_2^*, \theta_3^*, \theta_4^*, \theta_5^* ]$:
1. For each joint $i$, compute angular displacement: $\Delta \theta_i = |\theta_i^* - \theta_i|$.
2. Determine minimum feasible physical time for each joint:
   $$t_{\min, i} = \frac{\Delta \theta_i}{\omega_{\max, i}}$$
3. Find the longest axis time:
   $$T_{\min} = \max_{i \in [0..5]} (t_{\min, i})$$
4. Enforce feasibility clamping against caller-requested duration $T_{\text{requested}}$:
   $$T_{\text{actual}} = \max(T_{\text{requested}}, T_{\min})$$
5. Scale each joint's speed so all 6 finish at $T_{\text{actual}}$:
   $$\omega_i = \frac{\Delta \theta_i}{T_{\text{actual}}}$$
   $$\text{stepIntervalUs}_i = \frac{10^6}{\omega_i \times \text{stepsPerDegree}_i}$$

### 5.3 Inverse Kinematics (IK) Extension Hook
`MultiAxisManager` provides a dedicated class and solver interface for 6-DOF robotics:
```cpp
struct CartesianPose {
    float x, y, z;       // Position in mm
    float roll, pitch, yaw; // Orientation in degrees
};

struct IKSolverParams {
    uint16_t maxIterations;
    float toleranceMm;
    float dampingFactor;
};

class KinematicsHook {
public:
    virtual bool solveIK(const CartesianPose& targetPose, 
                         const float currentJoints[6], 
                         float outJoints[6], 
                         const IKSolverParams& params) = 0;
};
```

---

## 6. NVS Flash Storage Schema

Each axis stores its parameters in independent NVS namespaces:
* Namespaces: `mctrl_0` through `mctrl_5`
  * `calib`: 16-point `CalibData` struct (sensor angles vs. actual angles).
  * `gear_ratio`: Float (default 6.0).
  * `invert`: Boolean (direction invert).
  * `hold`: Boolean (closed-loop hold enable).
  * `speed`: `uint32_t` (base step interval in $\mu\text{s}$).
  * `current`: `uint16_t` (RMS current in mA).
* Namespace: `wifi`
  * `ssid`: String
  * `pass`: String

---

## 7. Web UI & API Specifications

### 7.1 Web Dashboard Layout
* **Header**: Live Wi-Fi status badge (AP/STA IP), Global homing status, Emergency Stop ALL.
* **6-Axis Overview Panel**: Live table/cards with J1–J6 angles, target angles, error, and sensor health.
* **Active Joint Controller (Tabs: Joint 1 to Joint 6)**:
  * High-resolution Dial Gauge with target and actual angle needles.
  * Manual Step Controls: Quick step buttons ($\pm 50, \pm 200, \pm 1600, \pm 3200$), custom step count input, continuous spin CCW/CW, and Driver Enable toggle (Free shaft).
  * Angle Targeting & Jogging ($\pm 1^\circ, \pm 10^\circ$).
  * Homing & 16-point Auto-Calibration LUT triggers.
  * TMC2209 speed and current RMS sliders.
* **Coordinated Move Panel**: 6 inputs for $[\theta_1..\theta_6]$ with "Sync Move" execution button.

### 7.2 REST API Endpoints

| Method | Endpoint | Query Parameters | Description |
| :--- | :--- | :--- | :--- |
| `GET` | `/api/status` | — | Returns JSON status of all 6 axes and system telemetry |
| `POST` | `/api/motor/goto` | `axis` (0–5), `angle` (float) | Moves single joint to target angle |
| `POST` | `/api/motor/jog` | `axis` (0–5), `delta` (float) | Jogs single joint by delta angle |
| `POST` | `/api/motor/step` | `axis` (0–5), `dir` (cw/ccw), `steps` (int), `speed` (opt) | Moves raw steps on chosen motor |
| `POST` | `/api/motor/run` | `axis` (0–5), `dir` (cw/ccw), `speed` (opt) | Runs motor continuously |
| `POST` | `/api/motor/enable` | `axis` (0–5), `en` (0/1) | Enables or disables TMC2209 driver |
| `POST` | `/api/motor/home` | `axis` (0–5) | Initiates center-homing for single joint |
| `POST` | `/api/motor/calib` | `axis` (0–5) | Runs 16-point auto calibration on single joint |
| `POST` | `/api/motor/settings` | `axis`, `hold`, `invert`, `speed`, `curr` | Updates and saves joint settings to NVS |
| `POST` | `/api/all/goto` | `angles` (comma-separated 6 floats), `time` (opt float) | Coordinated synchronized move across all 6 axes |
| `POST` | `/api/all/stop` | — | Emergency stop on all 6 axes |

### 7.3 Serial CLI Commands
* `M<1-6> <angle>`: Go to angle (e.g. `M1 45.0`)
* `M<1-6> STEP <steps>`: Step raw pulses (e.g. `M2 STEP 400`, `M2 STEP -400`)
* `M<1-6> RUN CW` / `M<1-6> RUN CCW`: Continuous run
* `M<1-6> STOP` / `M<1-6> ENABLE` / `M<1-6> FREE`
* `M<1-6> HOME` / `M<1-6> CALIB`
* `ALL <a1> <a2> <a3> <a4> <a5> <a6>`: Coordinated move (e.g. `ALL 0 45 -30 90 0 0`)
* `STOP` / `EMERGENCY STOP`: Global immediate stop

---

## 8. Verification & Acceptance Criteria
1. **Multi-Drop UART Telemetry**: All 6 TMC2209 drivers respond to `version()` and configure RMS current without collisions.
2. **500Hz 6-Axis Sensor Scan**: `SensorScanTask` reads all 6 channels of PCA9548A within 2ms without I2C timeouts.
3. **Manual Control Isolation**: Operating Motor $N$ in manual step/continuous mode does not affect or block any other motor.
4. **Schmitt Deadband Stability**: When held at position, motors remain quiet without micro-stepping vibration or hunting.
5. **Synchronized Arrival**: A coordinated multi-axis move with unequal angles (e.g. J1=$90^\circ$, J3=$5^\circ$) finishes simultaneously without path warping.
