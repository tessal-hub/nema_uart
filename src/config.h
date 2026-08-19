#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>

// ==============================================================================
// 1. SYSTEM SCALING
// ==============================================================================
#define NUM_MOTORS  6
#define NUM_SENSORS 6

// ==============================================================================
// 2. I2C & AS5600 ENCODER (PCA9548A MULTIPLEXER)
// ==============================================================================
#define SDA_PIN 8
#define SCL_PIN 9

#define PCA_ADDR       0x70
#define AS5600_ADDR    0x36
#define RAW_ANGLE_REG  0x0E

// Sensor Task configuration (Core 0, 500Hz)
#define SENSOR_TASK_PERIOD_MS        2
#define SENSOR_TASK_STACK_SIZE       4096
#define SENSOR_TASK_PRIORITY         2
#define SENSOR_TASK_CORE             0
#define SENSOR_I2C_MUTEX_TIMEOUT_MS  5

// ==============================================================================
// 3. TMC2209 STEPPER DRIVERS (DUAL HARDWARE UART & STEP/DIR PINS)
// ==============================================================================
#define R_SENSE 0.11f

// UART Bus 1: Drives Motors 0, 1, 2, 3 (Addresses 0b00, 0b01, 0b10, 0b11)
#define SERIAL_PORT_1 Serial1
#define TX_PIN_1      15
#define RX_PIN_1      16

// UART Bus 2: Drives Motors 4, 5 (Addresses 0b00, 0b01)
#define SERIAL_PORT_2 Serial2
#define TX_PIN_2      17
#define RX_PIN_2      18

// Dedicated STEP pins for all 6 axes (Excluding GPIO 4 to prevent ESP32-S3 boot issues)
#define STEP_PIN_0    1
#define STEP_PIN_1    2
#define STEP_PIN_2    41
#define STEP_PIN_3    42
#define STEP_PIN_4    21
#define STEP_PIN_5    14

// Optional dedicated DIR pins (Set to 255 if using UART-only direction control, or assign GPIO if wired)
#define DIR_PIN_0     255
#define DIR_PIN_1     255
#define DIR_PIN_2     255
#define DIR_PIN_3     255
#define DIR_PIN_4     255
#define DIR_PIN_5     255

// ==============================================================================
// 4. MOTION & CONTROL DEFAULTS
// ==============================================================================
#define DEFAULT_GEAR_RATIO        6.0f     // 6:1 gear ratio
#define DEFAULT_FULL_STEPS        200      // 1.8 degree stepper
#define DEFAULT_MICROSTEPS        16       // 1/16 microstepping
#define DEFAULT_NORMAL_CURRENT    800      // Normal running current (mA)
#define DEFAULT_HOMING_CURRENT    600      // Safe stall current for homing (mA)
#define DEFAULT_STEP_INTERVAL_US  400      // Base step interval (us)

// Schmitt-Trigger Deadband for closed-loop holding
#define DEFAULT_DEADBAND_ENTER    0.3f     // Enter holding window (degrees)
#define DEFAULT_DEADBAND_EXIT     0.8f     // Exit holding window on disturbance (degrees)
#define DEFAULT_ANGLE_TOLERANCE   0.5f     // Positioning target tolerance (degrees)

// Runaway safety threshold (stops motor if error grows by this amount in closed loop)
#define RUNAWAY_ERROR_THRESHOLD   5.0f     // degrees

// Motion Control Task configuration (Core 1, 100Hz)
#define MOTION_TASK_PERIOD_MS     10
#define MOTION_TASK_STACK_SIZE    4096
#define MOTION_TASK_PRIORITY      3
#define MOTION_TASK_CORE          1

#endif // CONFIG_H
