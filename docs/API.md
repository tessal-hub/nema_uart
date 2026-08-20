# REST API & Serial CLI Reference

## 1. REST API Specification

Base URL: `http://<ESP32_IP>/` or `http://nema.local/`

### 1.1 System & Telemetry Endpoints

#### `GET /api/status`
Returns complete JSON state of all 6 axes, Cartesian TCP pose, system uptime, and driver diagnostics.

**Response Schema (JSON)**:
```json
{
  "fw_name": "NEMA-6AXIS-PRO",
  "fw_version": "v2.2.0-DH-CONFIRMED",
  "uptime_sec": 142,
  "free_heap": 218450,
  "wifi_connected": true,
  "wifi_ssid": "Workshop-WiFi",
  "wifi_rssi": -55,
  "wifi_ip": "192.168.1.150",
  "ap_ip": "192.168.4.1",
  "all_homed": true,
  "all_calibrated": true,
  "any_running": false,
  "seq_running": false,
  "seq_idx": 0,
  "seq_count": 3,
  "tcp": {
    "x": 126.0,
    "y": 0.0,
    "z": 385.0,
    "roll": 0.0,
    "pitch": 0.0,
    "yaw": 0.0
  },
  "axes": [
    {
      "axis": 0,
      "currentAngle": 0.0,
      "targetAngle": 0.0,
      "error": 0.0,
      "rawAngle": 182.4,
      "absAngle": 182.4,
      "accumAngle": 0.0,
      "turns": 0,
      "isHomed": true,
      "isCalibrated": true,
      "isRunning": false,
      "driver_enabled": true,
      "inDeadband": true,
      "runaway_error": false,
      "uart_ok": true,
      "driver_version": 33,
      "over_temp": false,
      "over_temp_warn": false,
      "short_gnd_a": false,
      "short_gnd_b": false,
      "open_load_a": false,
      "open_load_b": false,
      "sg_result": 45,
      "totalStroke": 350.0,
      "limitLeft": -175.0,
      "limitRight": 175.0,
      "as5600_ok": true,
      "magnet_optimal": true,
      "agc": 128,
      "magnitude": 2150,
      "speed": 400,
      "currentSpeed": 400,
      "current": 800,
      "gearRatio": 6.0,
      "closedLoopHold": true,
      "dirInvert": false
    }
  ]
}
```

---

### 1.2 Single Joint Control Endpoints

| Endpoint | Method | Parameters | Description |
| :--- | :--- | :--- | :--- |
| `/api/motor/goto` | `POST` | `axis` (0–5), `angle` (float) | Target angle closed-loop positioning |
| `/api/motor/jog` | `POST` | `axis` (0–5), `delta` (float) | Jog relative delta angle |
| `/api/motor/step` | `POST` | `axis` (0–5), `dir` (`cw`/`ccw`), `steps` (int), `speed` (opt us) | S-curve stepped pulse move |
| `/api/motor/run` | `POST` | `axis` (0–5), `dir` (`cw`/`ccw`), `speed` (opt us) | Smooth continuous rotation |
| `/api/motor/stop` | `POST` | `axis` (0–5) | Immediate joint stop |
| `/api/motor/enable` | `POST` | `axis` (0–5), `en` (`1`/`0`) | Free shaft toggle (1=Power ON, 0=Free) |
| `/api/motor/home` | `POST` | `axis` (0–5) | Center homing routine |
| `/api/motor/zero` | `POST` | `axis` (0–5) | Instant zeroing at current position |
| `/api/motor/calib` | `POST` | `axis` (0–5) | 16-point LUT auto-calibration |
| `/api/motor/calib_clear` | `POST` | `axis` (0–5) | Erase calibration map |
| `/api/motor/settings` | `POST` | `axis`, `hold`, `invert`, `speed`, `curr`, `gear`, `lim_min`, `lim_max` | Persist joint config to NVS |

---

### 1.3 Multi-Axis & Kinematics Endpoints

| Endpoint | Method | Parameters | Description |
| :--- | :--- | :--- | :--- |
| `/api/all/goto` | `POST` | `angles` (comma-separated 6 floats), `time` (opt float, s) | Synchronized multi-axis arrival move ($T_{\text{sync}}$) |
| `/api/all/stop` | `POST` | — | Emergency stop all 6 axes immediately |
| `/api/all/home` | `POST` | — | Sequential center homing on all 6 axes |
| `/api/all/zero` | `POST` | — | Set `[0,0,0,0,0,0]` reference across all joints |
| `/api/all/enable` | `POST` | `en` (`1`/`0`) | Global driver power toggle |
| `/api/ik/goto` | `POST` | `x`, `y`, `z`, `roll`, `pitch`, `yaw`, `time` | Cartesian Tool Center Point (TCP) move |
| `/api/ik/pose` | `GET` | — | Get current Cartesian TCP coordinates |

---

### 1.4 Waypoint Sequencer Endpoints

| Endpoint | Method | Parameters | Description |
| :--- | :--- | :--- | :--- |
| `/api/waypoint/list` | `GET` | — | Return JSON array of all saved Waypoints |
| `/api/waypoint/add` | `POST` | `name`, `time`, `dwell`, `joints` (opt) | Add new waypoint to sequence |
| `/api/waypoint/clear` | `POST` | — | Clear all waypoints from memory |
| `/api/waypoint/start` | `POST` | `loop` (`1`/`0`) | Start automated trajectory sequence |
| `/api/waypoint/pause` | `POST` | — | Pause sequence |
| `/api/waypoint/stop` | `POST` | — | Halt sequence and stop all motion |

---

## 2. Industrial Serial CLI & G-code Reference

Baud Rate: **115200**, Line Ending: **Newline** (`\n` or `\r\n`).

### G-code Standard Commands

| Command | Example | Description |
| :--- | :--- | :--- |
| `G0` | `G0 X150 Y50 Z300 T2.0` | Rapid Cartesian positioning to target |
| `G1` | `G1 X150 Y50 Z300 F50` | Linear Cartesian move at feedrate (mm/s) |
| `G28` | `G28` | Home all 6 axes sequentially |
| `M17` | `M17` | Enable all stepper drivers (Power ON) |
| `M18` / `M84` | `M18` | Disable stepper drivers (Free shaft) |
| `M112` | `M112` | **EMERGENCY STOP ALL 6 AXES IMMEDIATELY** |
| `M114` | `M114` | Query current position and system status |

### ASCII / Robot CLI Commands

| Command | Example | Description |
| :--- | :--- | :--- |
| `M<1-6> <angle>` | `M1 45.0` | Move single joint to target angle |
| `M<1-6> JOG <delta>` | `M2 JOG -5.0` | Jog single joint by relative delta angle |
| `M<1-6> STEP <steps> [spd]`| `M3 STEP 400 300` | Step microsteps with S-curve ramp |
| `M<1-6> RUN CW` / `CCW` | `M1 RUN CW` | Spin motor continuously |
| `M<1-6> STOP` | `M1 STOP` | Stop single joint |
| `M<1-6> HOME` | `M1 HOME` | Start center homing on joint |
| `M<1-6> ZERO` | `M1 ZERO` | Instant zeroing at current position |
| `M<1-6> CALIB` | `M1 CALIB` | Run 16-point LUT auto-calibration |
| `M<1-6> INVERT <0/1>` | `M1 INVERT 1` | Set direction invert flag in NVS |
| `M<1-6> HOLD <0/1>` | `M1 HOLD 1` | Enable/disable Schmitt deadband holding |
| `M<1-6> SPEED <us>` | `M1 SPEED 350` | Set motor base step interval |
| `M<1-6> CURR <mA>` | `M1 CURR 900` | Set TMC2209 RMS running current |
| `ALL <j1>..<j6> [time]` | `ALL 0 45 -30 90 0 0 2.5` | Coordinated 6-axis arrival move |
| `IK <x> <y> <z> [r] [p] [y] [t]`| `IK 126 0 385 0 0 0 2.0` | Move TCP to Cartesian coordinate (mm) |
| `POSE` | `POSE` | Print current Cartesian Tool Center Point pose |
| `WP ADD <name>` | `WP ADD Pick1` | Save current joint angles as a Waypoint |
| `WP LIST` | `WP LIST` | Display all saved waypoints |
| `WP START [LOOP]` | `WP START LOOP` | Execute automated trajectory playback |
| `TEST UART` | `TEST UART` | Query all 6 TMC2209 driver registers |
| `REBOOT` | `REBOOT` | Restart the ESP32 microcontroller |
| `HELP` / `?` | `HELP` | Display interactive CLI help menu |
