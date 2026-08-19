#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include <TMCStepper.h>
#include <esp_timer.h>

class Motor {
private:
    TMC2209Stepper* driver;
    uint8_t stepPin;
    uint8_t dirPin;
    uint8_t address;
    const char* label;

    volatile bool running;
    volatile bool dirCW;
    volatile uint32_t stepIntervalUs;
    volatile uint32_t targetSteps;
    volatile uint32_t stepsRemaining;
    volatile uint16_t currentMa;
    volatile bool spreadCycleMode;
    volatile uint16_t microstepsVal;
    volatile uint8_t holdScale;
    volatile bool enabled;

    bool uartOk;
    uint8_t driverVersion;

    esp_timer_handle_t stepTimer;
    static void IRAM_ATTR onStepTimer(void* arg);

public:
    Motor(HardwareSerial* serialPort, float rSense, uint8_t uartAddress,
          uint8_t stepPinNum, uint8_t dirPinNum = 255, const char* motorLabel = "Motor");
    ~Motor();

    void begin(uint16_t initialCurrentMa = 700, uint16_t initialMicrosteps = 16,
               bool initialSpreadCycle = true, uint8_t initialHoldScale = 8,
               uint8_t iholddelay = 10);

    void run(bool cw, uint32_t steps);
    void stop();
    void enable(bool en = true);

    void setSpeed(uint32_t intervalUs);
    void setCurrent(uint16_t mA);
    void setHold(uint8_t scale);
    void setChopperMode(bool spreadCycle); // true = SpreadCycle, false = StealthChop
    void setMicrosteps(uint16_t ms);

    void update();

    // --- Diagnostic & UART verification ---
    bool testUART();
    bool isUartOK() const { return uartOk; }
    uint8_t getDriverVersion() const { return driverVersion; }

    // --- StallGuard4 / Sensorless Homing ---
    uint16_t getStallGuardResult();
    void setStallGuardThreshold(uint8_t threshold);
    void setupStallGuard(uint8_t threshold = 60, uint32_t tcoolthrs = 0xFFFFF);

    TMC2209Stepper* getDriver() { return driver; }

    // --- Getters cho status/JSON ---
    bool isRunning() const { return running; }
    bool isEnabled() const { return enabled; }
    bool getDirCW() const { return dirCW; }
    uint32_t getStepsRemaining() const { return stepsRemaining; }
    uint32_t getTargetSteps() const { return targetSteps; }
    uint32_t getStepInterval() const { return stepIntervalUs; }
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