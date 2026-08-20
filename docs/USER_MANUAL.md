# ESP32-S3 6-Axis Motion Controller: User & Integration Manual

## 1. Safety Guidelines & Operating Rules

Before powering the motion control system, verify the following:
1. **Supply Voltage**: Ensure the stepper motor DC power supply (12V–24V) is stable and shared ground with the ESP32-S3 logic board.
2. **Current Rating**: Configure TMC2209 RMS current appropriately for your NEMA stepper motor model (typically 600mA–1000mA for NEMA 17, 400mA–700mA for NEMA 14). Do not exceed motor thermal ratings.
3. **Emergency Stop**: Keep the Web UI emergency stop button or Serial terminal `STOP` command accessible during initial tuning.
4. **Mechanical Endstops**: Ensure physical hard stops or mechanical limits are within the reach of the robotic joints.

---

## 2. Web UI Dashboard Walkthrough

### 2.1 Live 6-Axis Overview Panel
- Displays all 6 axes simultaneously with live joint angle (0.1° resolution), target angle, error offset, sensor health status, and motion state (`IDLE`, `HOLD`, `QUAY`).
- **Click any card** to immediately switch to that specific joint's detail tuning tab.

### 2.2 Vector SVG Dial Gauge
- **Outer dial circle**: 360° degree markers with primary tick lines at 0° (Home / Blue), +90°, -90°, 180°.
- **Dashed Cyan Pointer**: Target commanded angle.
- **Solid Green Pointer**: Measured AS5600 encoder angle in real-time.
- **Center Readout**: Large high-contrast tabular monospace degree readout and error status.

### 2.3 Manual Direct Step & Jog Controls
- **Driver Enable Toggle**: Turn off to release the motor holding torque, allowing the axis shaft to be freely rotated by hand without resistance. Turn on to re-energize the coils.
- **Discrete Step Buttons**: Pulse exact step counts (`±50`, `±200`, `±1600`, `±3200 steps`) with smooth acceleration ramping.
- **Continuous Run**: Spin continuously CW or CCW for mechanism testing and mechanical run-in.

### 2.4 Homing & 16-Point Auto-Calibration
- **Homing Cung Lớn (Center Homing)**: Moves the joint to find the left hard stop, backs off, moves to find the right hard stop, calculates the major arc span, and centers the joint at `0.00°`.
- **Đặt Gốc (Zero Here)**: Immediately sets the current physical position as `0.00°` home reference without moving the motor.
- **Auto Calib LUT**: Executes a 16-station automated routine around 360° to build a non-linear calibration map in NVS flash, compensating for any magnet eccentricity or sensor non-linearity.

### 2.5 6-Axis Synchronized Move (`$T_{sync}$`)
- Allows commanding all 6 joints to independent target angles $[ \theta_1, \theta_2, \theta_3, \theta_4, \theta_5, \theta_6 ]$.
- Enter an optional execution duration in seconds, or leave as `0` for auto-calculated minimum feasible time.
- The coordinator scales each motor's velocity so all axes start together and complete their displacement at the exact same instant.

### 2.6 6-DOF Cartesian IK Controller
- Controls the Tool Center Point (TCP) in millimeters $(X, Y, Z)$ and end-effector orientation in degrees $(\text{Roll}, \text{Pitch}, \text{Yaw})$.
- Features a live 2D geometric schematic preview of the arm configuration.
- Includes quick pose presets:
  - **Home**: `[0°, 0°, 0°, 0°, 0°, 0°]`
  - **Ready / Standby**: `[0°, -30°, 45°, 0°, 30°, 0°]`
  - **Reach / Inspect**: `[0°, 30°, 30°, 0°, -45°, 0°]`
  - **Pick Position**: `[45°, 45°, 20°, 0°, -65°, 0°]`
  - **Place Position**: `[-45°, 45°, 20°, 0°, -65°, 0°]`

### 2.7 Automated Trajectory Waypoints Sequencer
- Record custom waypoints by clicking **Lưu Vị Trí Hiện Tại** (captures current joint angles and Cartesian pose).
- Set custom transition move duration and dwell delay (ms) per point.
- Click **Bắt Đầu Chạy** to play through the sequence sequentially with optional infinite loop.

---

## 3. Tuning & Diagnostics

### 3.1 Resolving Inverted Direction (Runaway Protection)
If a joint begins moving and stops after 300ms with a **Runaway Direction Warning**:
1. This indicates the motor stepping direction opposes the AS5600 encoder rotation.
2. In the Joint Settings card, simply toggle the **Đảo Chiều Motor (Invert)** switch to `ON`.
3. The setting is instantly persisted to NVS flash memory.

### 3.2 Closed-Loop Schmitt Deadband Tuning
- **`deadbandEnter` (0.3°)**: The threshold where the controller considers the joint arrived at the target angle and enters holding mode.
- **`deadbandExit` (0.8°)**: The threshold that must be exceeded by external mechanical torque/disturbance before the controller reactivates stepping pulses.
- This hysteresis window completely prevents microstep hunting and motor resonance while maintaining firm holding stiffness.

### 3.3 AS5600 Magnet Health & Air Gap
Open the **Chẩn Đoán Phần Cứng** tab to inspect the magnetic field parameters:
- **AGC Value**: Optimal range is between **30 and 220** (nominal ~128).
  - If AGC $< 25$: Magnet is too close ($< 0.5\text{mm}$).
  - If AGC $> 230$: Magnet is too far ($> 2.5\text{mm}$) or magnetic field is weak.
- **Magnitude**: CORDIC magnitude should typically register between 1000 and 3000.
- **Magnet Optimal**: Displays a green checkmark when air gap and field strength are in the optimal linear operating zone.
