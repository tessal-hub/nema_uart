#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// AS5600 Register Map
#define AS5600_ZMCO_REG    0x00
#define AS5600_ZPOS_REG    0x01
#define AS5600_MPOS_REG    0x03
#define AS5600_MANG_REG    0x05
#define AS5600_CONF_REG    0x07   // 2 byte: 0x07 = high, 0x08 = low
#define AS5600_STATUS_REG  0x0B
#define AS5600_RAW_REG     0x0C
#define AS5600_ANGLE_REG   0x0E   // Hardware-filtered angle
#define AS5600_AGC_REG     0x1A
#define AS5600_MAG_REG     0x1B

// Hardware Filter & Power Configuration
#define AS5600_CONF_PM     0b00   // Power mode: NOM (Normal)
#define AS5600_CONF_HYST   0b10   // Hysteresis: 2LSB
#define AS5600_CONF_OUTS   0b00   // Output stage: analog
#define AS5600_CONF_PWMF   0b00   // PWM freq: off
#define AS5600_CONF_SF     0b00   // Slow filter: 16x (mạnh nhất)
#define AS5600_CONF_FTH    0b000  // Fast filter threshold: slow filter only

// Cấu trúc thông tin chẩn đoán phần cứng AS5600
struct AS5600Diag {
    uint8_t status;        // Reg 0x0B: STATUS
    bool magnetDetected;   // Bit 5: MD (Đã phát hiện nam châm)
    bool magnetTooLow;     // Bit 4: ML (Từ trường quá yếu / AGC max)
    bool magnetTooHigh;    // Bit 3: MH (Từ trường quá mạnh / AGC min)
    bool magnetOptimal;    // AGC và khoảng cách tối ưu
    uint8_t agc;           // Reg 0x1A: AGC (0..255)
    uint16_t magnitude;    // Reg 0x1B, 0x1C: CORDIC magnitude (0..4095)
    uint16_t rawAngle;     // Reg 0x0C, 0x0D: Raw angle (0..4095)
    uint16_t angleReg;     // Reg 0x0E, 0x0F: Hardware filtered angle (0..4095)
    uint16_t zpos;         // Reg 0x01, 0x02: Zero position
    uint16_t mpos;         // Reg 0x03, 0x04: Max position
    uint16_t mang;         // Reg 0x05, 0x06: Max angle
    uint8_t zmco;          // Reg 0x00: Burn count (0 = chua burn)
    bool readSuccess;      // Đọc thành công
};

class Sensor {
private:
    const float ALPHA = 0.2f;
    float filtered_angles[NUM_SENSORS];
    float last_raw_angles[NUM_SENSORS];
    float accumulated_angles[NUM_SENSORS];
    int32_t turn_counts[NUM_SENSORS];
    bool initialized[NUM_SENSORS];
    bool sensor_error[NUM_SENSORS];
    uint32_t read_fail_counts[NUM_SENSORS];

    SemaphoreHandle_t dataMutex;        // Bảo vệ mảng góc khi task ghi / main đọc
    SemaphoreHandle_t i2cMutex;         // Bảo vệ I2C bus khi đọc chẩn đoán từ task khác
    TaskHandle_t taskHandle;
    volatile bool taskRunning;

    void setPCAChannel(uint8_t channel);
    void disableAllPCAChannels();
    uint16_t readRaw();
    float filter(uint8_t ch, uint16_t raw);
    void scanOnce();                    // Quét toàn bộ NUM_SENSORS 1 lần
    void configureAS5600();             // Ghi CONF register
    void recoverI2CBus();               // Khôi phục I2C bus khi bị treo

    static void taskEntry(void* param);
    void taskLoop();

public:
    Sensor();
    ~Sensor();

    void begin(uint8_t coreID = SENSOR_TASK_CORE,
               uint8_t priority = SENSOR_TASK_PRIORITY,
               uint32_t period_ms = SENSOR_TASK_PERIOD_MS);

    float getAngle(uint8_t ch = 0);
    float getAccumulatedAngle(uint8_t ch = 0);
    int32_t getTurnCount(uint8_t ch = 0);
    void resetAccumulatedAngle(uint8_t ch = 0);

    bool isSensorOK(uint8_t ch = 0);
    AS5600Diag getDiagnostics(uint8_t ch = 0); // Đọc toàn bộ thanh ghi chẩn đoán
};

#endif // SENSOR_H