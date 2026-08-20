#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>

// ==============================================================================
// 1. FIRMWARE METADATA & SYSTEM SCALING
// ==============================================================================
#define FW_NAME           "NEMA-6AXIS-PRO"
#define FW_VERSION        "v2.2.0-DH-CONFIRMED"
#define FW_BUILD_DATE     "2026-08-20"

#define NUM_MOTORS        6
#define NUM_SENSORS       6
#define MAX_WAYPOINTS     32

// ==============================================================================
// 2. I2C & AS5600 MAGNETIC ENCODER (PCA9548A MULTIPLEXER)
// ==============================================================================
#define SDA_PIN           8
#define SCL_PIN           9
#define I2C_FREQUENCY     400000     // 400kHz Fast I2C Bus

#define PCA_ADDR          0x70       // PCA9548A I2C address
#define AS5600_ADDR       0x36       // AS5600 I2C address
#define RAW_ANGLE_REG     0x0E

// Sensor Task configuration (Core 0, 500Hz)
#define SENSOR_TASK_PERIOD_MS        2
#define SENSOR_TASK_STACK_SIZE       4096
#define SENSOR_TASK_PRIORITY         2
#define SENSOR_TASK_CORE             0
#define SENSOR_I2C_MUTEX_TIMEOUT_MS  10

// AS5600 AGC Thresholds for Magnetic Field Health
#define AS5600_AGC_MIN_HEALTHY       25      // Below 25: Magnet too close (<0.5mm)
#define AS5600_AGC_MAX_HEALTHY       230     // Above 230: Magnet too far (>2.5mm)

// ==============================================================================
// 3. TMC2209 STEPPER DRIVERS (DUAL HARDWARE UART & STEP/DIR PINS)
// ==============================================================================
#define R_SENSE           0.11f      // Current sense resistor (Ohms)
#define TMC_UART_BAUD     115200     // Hardware UART Baud Rate

// UART Bus 1: Drives Motors 0, 1, 2, 3 (Addresses 0b00, 0b01, 0b10, 0b11)
#define SERIAL_PORT_1     Serial1
#define TX_PIN_1          15
#define RX_PIN_1          16

// UART Bus 2: Drives Motors 4, 5 (Addresses 0b00, 0b01)
#define SERIAL_PORT_2     Serial2
#define TX_PIN_2          17
#define RX_PIN_2          18

// Dedicated STEP pins for all 6 axes (Excluding GPIO 4 to prevent ESP32-S3 boot issues)
#define STEP_PIN_0        1
#define STEP_PIN_1        2
#define STEP_PIN_2        41
#define STEP_PIN_3        42
#define STEP_PIN_4        21
#define STEP_PIN_5        14

// Optional dedicated DIR pins (Set to 255 if using UART-only direction control, or assign GPIO if wired)
#define DIR_PIN_0         255
#define DIR_PIN_1         255
#define DIR_PIN_2         255
#define DIR_PIN_3         255
#define DIR_PIN_4         255
#define DIR_PIN_5         255

// ==============================================================================
// 4. MOTION & CLOSED-LOOP CONTROL DEFAULTS
// ==============================================================================
#define DEFAULT_GEAR_RATIO        6.0f     // 6:1 gear ratio default
#define DEFAULT_FULL_STEPS        200      // 1.8 degree stepper (200 steps/rev)
#define DEFAULT_MICROSTEPS        16       // 1/16 microstepping
#define DEFAULT_NORMAL_CURRENT    800      // Normal running current (mA)
#define DEFAULT_HOMING_CURRENT    550      // Safe stall current for homing (mA)
#define DEFAULT_HOLD_SCALE        8        // TMC2209 ihold scale (0..31)

// Speed & Acceleration Timing (in microseconds per step pulse)
#define DEFAULT_STEP_INTERVAL_US  400      // Target step interval (us) -> 2500 steps/sec
#define MIN_STEP_INTERVAL_US      120      // Max speed limit (us) -> ~8333 steps/sec
#define MAX_STEP_INTERVAL_US      2500     // Starting / Creep speed interval (us)
#define DEFAULT_ACCEL_RATE        12       // Microseconds interval delta per acceleration step

// Schmitt-Trigger Deadband for closed-loop holding
#define DEFAULT_DEADBAND_ENTER    0.3f     // Enter holding window (degrees)
#define DEFAULT_DEADBAND_EXIT     0.8f     // Exit holding window on disturbance (degrees)
#define DEFAULT_ANGLE_TOLERANCE   0.5f     // Positioning target tolerance (degrees)

// Runaway safety threshold (stops motor if error grows by this amount in closed loop)
#define RUNAWAY_ERROR_THRESHOLD   5.0f     // degrees

// Motion Control Task configuration (Core 1, 100Hz)
#define MOTION_TASK_PERIOD_MS     10
#define MOTION_TASK_STACK_SIZE    5120
#define MOTION_TASK_PRIORITY      3
#define MOTION_TASK_CORE          1

// ==============================================================================
// 5. CONFIRMED STANDARD DH PARAMETERS (mm & degrees)
// ==============================================================================
// Standard DH Table:
// Joint 1: a0 = 0.0,   alpha0 = 0.0,   d1 = 139.0, theta_offset = 0.0
// Joint 2: a1 = 0.0,   alpha1 = -90.0, d2 = 0.0,   theta_offset = -90.0
// Joint 3: a2 = 138.0, alpha2 = 0.0,   d3 = 0.0,   theta_offset = 0.0
// Joint 4: a3 = 88.0,  alpha3 = -90.0, d4 = 126.0, theta_offset = 0.0
// Joint 5: a4 = 0.0,   alpha4 = +90.0, d5 = 0.0,   theta_offset = 0.0
// Joint 6: a5 = 0.0,   alpha5 = -90.0, d6 = 0.0,   theta_offset = 0.0
#define DH_D1_MM                  139.0f   // Joint 1 base height (mm)
#define DH_A2_MM                  138.0f   // Joint 3 upper arm offset a_prev (mm)
#define DH_A3_MM                  88.0f    // Joint 4 forearm offset a_prev (mm)
#define DH_D4_MM                  126.0f   // Joint 4 link offset d4 (mm)
#define DH_D6_TOOL_MM             20.0f    // TCP tool offset past wrist center (mm)

#define DH_THETA2_OFFSET_DEG      -90.0f   // Joint 2 offset from encoder home

// Joint Soft Angle Limits (Degrees relative to Calibrated Home)
#define J1_MIN_LIMIT  -170.0f
#define J1_MAX_LIMIT  +170.0f
#define J2_MIN_LIMIT  -90.0f
#define J2_MAX_LIMIT  +120.0f
#define J3_MIN_LIMIT  -135.0f
#define J3_MAX_LIMIT  +135.0f
#define J4_MIN_LIMIT  -180.0f
#define J4_MAX_LIMIT  +180.0f
#define J5_MIN_LIMIT  -120.0f
#define J5_MAX_LIMIT  +120.0f
#define J6_MIN_LIMIT  -360.0f
#define J6_MAX_LIMIT  +360.0f

// ==============================================================================
// 6. NETWORKING & SYSTEM CONFIGURATION
// ==============================================================================
#define DEFAULT_AP_SSID           "NEMA-6AXIS-CONTROLLER"
#define DEFAULT_AP_PASS           "12345678"
#define DEFAULT_MDNS_HOST         "nema"
#define WEB_SERVER_PORT           80

#endif // CONFIG_H
