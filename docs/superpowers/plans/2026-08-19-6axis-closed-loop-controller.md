# Implementation Plan: 6-Axis Closed-Loop Stepper & Encoder Controller with IK Hook

- **Spec**: `docs/superpowers/specs/2026-08-19-6axis-closed-loop-controller-design.md`
- **Target Platform**: ESP32-S3 DevKitC-1 (PlatformIO / Arduino)

---

## Phase 1: Hardware Configuration & Multi-Channel Sensor Layer

### Task 1.1: Update `config.h`
- Define `NUM_MOTORS 6` and `NUM_SENSORS 6`.
- Configure dual UART pin definitions:
  - `SERIAL_PORT_1 Serial1`, `TX_PIN_1 15`, `RX_PIN_1 16`
  - `SERIAL_PORT_2 Serial2`, `TX_PIN_2 17`, `RX_PIN_2 18`
- Configure 6 STEP pin assignments:
  - `STEP_PIN_0 1`, `STEP_PIN_1 2`, `STEP_PIN_2 41`, `STEP_PIN_3 42`, `STEP_PIN_4 21`, `STEP_PIN_5 14`
  - Explicitly document exclusion of GPIO 4.
- Define PCA9548A multiplexer channel mapping (`PCA_CH_0` to `PCA_CH_5`).
- Define Schmitt-trigger deadband defaults: `DEFAULT_DEADBAND_ENTER 0.3f`, `DEFAULT_DEADBAND_EXIT 0.8f`.

### Task 1.2: Update `sensor.h` & `sensor.cpp`
- Scale internal arrays `filtered_angles[NUM_SENSORS]`, `initialized[NUM_SENSORS]`, `sensor_error[NUM_SENSORS]` to 6 channels.
- In `scanOnce()`: Loop across channels $0..5$, dynamically setting PCA9548A channel via I2C, reading angle registers, applying exponential filter ($\alpha=0.2$), and updating under `dataMutex`.
- In `getDiagnostics(uint8_t ch)`: Query AS5600 STATUS, AGC, MAGNITUDE, RAW_ANGLE for the specified channel with timeout protection.

**Verification**: Run `pio run` to verify clean compilation of sensor module.

---

## Phase 2: Thread-Safe Motor Driver Layer

### Task 2.1: Update `motor.h` & `motor.cpp`
- Implement thread-safe stop sequence in `stop()` and destructor:
  1. `esp_timer_stop(stepTimer)`
  2. `stepsRemaining = 0; targetSteps = 0; running = false;`
  3. `digitalWrite(stepPin, LOW);`
- Mark `stepsRemaining`, `targetSteps`, `running`, `dirCW`, `enabled` as `volatile`.
- Keep ISR callback strictly minimal (toggle STEP pin, decrement `stepsRemaining`, auto-stop timer when 0).
- Support hardware UART pointer (`HardwareSerial*`) and 2-bit multi-drop address (`0b00`–`0b11`).

**Verification**: Run `pio run` to verify compilation.

---

## Phase 3: Multi-Instance `MotionController` & NVS Namespacing

### Task 3.1: Update `motion_controller.h` & `motion_controller.cpp`
- Add `uint8_t axisId` (0–5) to constructor.
- Namespace NVS flash storage keys per axis (`mctrl_0` through `mctrl_5`) for 16-point LUT calibration, gear ratio, inversion, hold, speed, and current.
- Implement Schmitt-trigger deadband:
  - Enter holding state when $|\Delta \theta| \le \text{deadbandEnter}$ ($0.3^\circ$).
  - Only re-engage active stepping when disturbance exceeds $|\Delta \theta| > \text{deadbandExit}$ ($0.8^\circ$).
- Implement isolated manual control functions: `moveRawSteps()`, `runContinuous()`, `jog()`, `setDriverEnabled()`, `runCenterHoming()`, `runAutoCalibration()`.

**Verification**: Run `pio run` to verify compilation.

---

## Phase 4: Coordinator Layer with Synchronized Arrival & IK Extension Hook

### Task 4.1: Create `src/multi_axis_manager.h` & `src/multi_axis_manager.cpp`
- Maintain arrays `Motor* motors[NUM_MOTORS]` and `MotionController* controllers[NUM_MOTORS]`.
- Implement `setTargetAnglesSync(const float targets[6], float moveTimeSec, bool syncArrival)`:
  - Compute $\Delta \theta_i = |\theta_i^* - \theta_i|$ for all 6 axes.
  - Calculate $T_{\min} = \max_i (\Delta \theta_i / \omega_{\max, i})$.
  - Feasibility clamping: $T_{\text{actual}} = \max(T_{\text{requested}}, T_{\min})$.
  - Compute scaled velocity per joint $\omega_i = \Delta \theta_i / T_{\text{actual}}$ and set step intervals.
  - Command all 6 controllers simultaneously.
- Implement `emergencyStopAll()`: Stops all 6 timers and drivers instantly.
- Create dedicated `MotionControlTask` pinned to Core 1 at 100Hz (`vTaskDelayUntil`) to isolate motion loop from Web Server HTTP latency.
- Create `KinematicsHook` abstract class with `CartesianPose` $[X, Y, Z, \text{Roll}, \text{Pitch}, \text{Yaw}]$ and `IKSolverParams`.

**Verification**: Run `pio run` to verify compilation.

---

## Phase 5: Web UI & REST/Serial API Integration

### Task 5.1: Redesign `src/web_ui.h`
- Build modern responsive 6-axis dashboard:
  - **Top Bar**: Wi-Fi status, Global Homing state, Emergency Stop ALL.
  - **6-Axis Live Overview Card**: Real-time table showing J1–J6 angles, targets, errors, and driver states.
  - **Joint Control Panel (Tabs M1 to M6)**:
    - Live SVG Dial Gauge showing target and current angle for selected joint.
    - Angle targeting & jogging ($\pm 1^\circ, \pm 10^\circ$).
    - Manual Step Controls: Quick step buttons ($\pm 50, \pm 200, \pm 1600, \pm 3200$), custom step count, Continuous CCW/CW run, and Driver Enable switch.
    - Homing & 16-point Auto-Calibration triggers.
    - Speed & Current sliders.
  - **Synchronized 6-Axis Move Card**: 6 inputs for $[\theta_1..\theta_6]$ + "Sync Move" execution button.

### Task 5.2: Update `src/web_server_manager.h` & `src/web_server_manager.cpp`
- Implement parameterized REST endpoints:
  - `/api/status`: JSON array of all 6 axes + system telemetry.
  - `/api/motor/goto`, `/api/motor/jog`, `/api/motor/step`, `/api/motor/run`, `/api/motor/enable`, `/api/motor/home`, `/api/motor/calib`, `/api/motor/settings` with `axis` argument.
  - `/api/all/goto` with `angles` and optional `time`.
  - `/api/all/stop`.

### Task 5.3: Update `src/main.cpp`
- Initialize `Serial1` (TX: 15, RX: 16) and `Serial2` (TX: 17, RX: 18).
- Instantiate 6 `Motor` instances with their assigned UART buses and addresses.
- Instantiate `Sensor`, 6 `MotionController` instances, `MultiAxisManager`, and `WebServerManager`.
- Start `SensorScanTask` on Core 0 and `MotionControlTask` on Core 1.
- Implement Serial CLI commands (`M1..M6 <angle>`, `M<1-6> STEP <steps>`, `M<1-6> RUN <CW/CCW>`, `M<1-6> HOME`, `ALL <a1>..<a6>`, `STOP`).

**Verification**: Run `pio run` to perform a full build.

---

## Phase 6: Full System Build & Firmware Verification

- Perform full compilation with PlatformIO.
- Verify flash and RAM usage on ESP32-S3.
- Commit all changes to git repository.
