#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include <TMCStepper.h>
#include <esp_timer.h>
#include "config.h"

// Struct chứa chi tiết chẩn đoán thanh ghi phần cứng TMC2209
struct TMC2209Diag {
    bool uartOk;
    uint8_t driverVersion;
    bool overTemp;          // ot (Overtemperature shutdown)
    bool overTempWarning;   // otpw (Overtemperature pre-warning)
    bool shortToGndA;       // s2ga (Short to ground phase A)
    bool shortToGndB;       // s2gb (Short to ground phase B)
    bool openLoadA;         // ola (Open load phase A)
    bool openLoadB;         // olb (Open load phase B)
    bool standStill;        // stst (Motor at standstill)
    uint8_t csActual;       // cs_actual (Actual current scale 0..31)
    uint16_t sgResult;      // SG_RESULT (StallGuard load value 0..1023)
};

class Motor {
private:
    HardwareSerial* serialPort;
    SemaphoreHandle_t* uartMutex;
    TMC2209Stepper* driver;
    uint8_t stepPin;
    uint8_t dirPin;
    uint8_t address;
    const char* label;

    // Trạng thái vận hành bước (Được bảo vệ volatile và nguyên tử trong ISR)
    volatile bool running;
    volatile bool dirCW;
    int8_t lastShaftDir;                  // Bộ đệm lưu chiều quay UART đã gửi (-1: chưa khởi tạo)
    volatile uint32_t targetSpeedUs;      // Tốc độ đích mong muốn (us/step)
    volatile uint32_t currentSpeedUs;     // Tốc độ tức thời đang chạy (us/step)
    volatile uint32_t startSpeedUs;       // Tốc độ xuất phát (us/step)
    volatile uint32_t targetSteps;        // Tổng số bước của lệnh hiện tại
    volatile uint32_t stepsRemaining;     // Số bước còn lại
    volatile uint32_t stepCounter;        // Đếm số bước đã đi trong chu kỳ
    volatile uint32_t accelSteps;         // Số bước tăng tốc
    volatile uint32_t decelSteps;         // Số bước giảm tốc
    volatile bool continuousMode;         // Chế độ quay liên tục

    volatile uint16_t currentMa;
    volatile bool spreadCycleMode;
    volatile uint16_t microstepsVal;
    volatile uint8_t holdScale;
    volatile bool enabled;

    bool uartOk;
    uint8_t driverVersion;

    esp_timer_handle_t stepTimer;
    static void IRAM_ATTR onStepTimer(void* arg);

    static inline uint32_t calculateSCurveInterval(uint32_t currentStep, uint32_t totalSteps,
                                                   uint32_t startInterval, uint32_t targetInterval);

public:
    Motor(HardwareSerial* serial, float rSense, uint8_t uartAddress,
          uint8_t stepPinNum, uint8_t dirPinNum = 255, const char* motorLabel = "Motor");
    ~Motor();

    void setUartMutex(SemaphoreHandle_t* mutex) { uartMutex = mutex; }

    void begin(uint16_t initialCurrentMa = DEFAULT_NORMAL_CURRENT,
               uint16_t initialMicrosteps = DEFAULT_MICROSTEPS,
               bool initialSpreadCycle = true,
               uint8_t initialHoldScale = DEFAULT_HOLD_SCALE,
               uint8_t iholddelay = 10);

    // Điều khiển chuyển động
    void setDirection(bool cw);
    void run(bool cw, uint32_t steps);
    void runContinuous(bool cw);
    void stop();                          // Dừng khẩn cấp tức thời
    void enable(bool en = true);

    // Cấu hình tham số driver & tốc độ
    void setSpeed(uint32_t intervalUs);
    void setCurrent(uint16_t mA);
    void setHold(uint8_t scale);
    void setChopperMode(bool spreadCycle);
    void setMicrosteps(uint16_t ms);

    void update();

    // Chẩn đoán & kiểm tra giao tiếp UART
    bool testUART();
    bool isUartOK() const { return uartOk; }
    uint8_t getDriverVersion() const { return driverVersion; }
    TMC2209Diag getDriverStatus();

    // StallGuard4 / Sensorless Homing
    uint16_t getStallGuardResult();
    void setStallGuardThreshold(uint8_t threshold);
    void setupStallGuard(uint8_t threshold = 60, uint32_t tcoolthrs = 0xFFFFF);

    TMC2209Stepper* getDriver() { return driver; }

    // Getters
    bool isRunning() const { return running; }
    bool isEnabled() const { return enabled; }
    bool getDirCW() const { return dirCW; }
    uint32_t getStepsRemaining() const { return stepsRemaining; }
    uint32_t getTargetSteps() const { return targetSteps; }
    uint32_t getStepInterval() const { return targetSpeedUs; }
    uint32_t getCurrentInterval() const { return currentSpeedUs; }
    uint16_t getCurrent() const { return currentMa; }
    uint8_t getHoldScale() const { return holdScale; }
    bool getSpreadCycle() const { return spreadCycleMode; }
    uint16_t getMicrosteps() const { return microstepsVal; }
    uint8_t getAddress() const { return address; }
    uint8_t getStepPin() const { return stepPin; }
    uint8_t getDirPin() const { return dirPin; }
    const char* getLabel() const { return label; }

    String toJson() const;
};

#endif // MOTOR_H