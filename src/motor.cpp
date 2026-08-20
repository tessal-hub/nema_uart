#include "motor.h"
#include "driver/gpio.h"

Motor::Motor(HardwareSerial* serial, float rSense, uint8_t uartAddress,
             uint8_t stepPinNum, uint8_t dirPinNum, const char* motorLabel)
    : serialPort(serial),
      driver(nullptr),
      stepPin(stepPinNum),
      dirPin(dirPinNum),
      address(uartAddress),
      label(motorLabel),
      running(false),
      dirCW(true),
      targetSpeedUs(DEFAULT_STEP_INTERVAL_US),
      currentSpeedUs(MAX_STEP_INTERVAL_US),
      startSpeedUs(MAX_STEP_INTERVAL_US),
      targetSteps(0),
      stepsRemaining(0),
      stepCounter(0),
      accelSteps(60),
      decelSteps(60),
      continuousMode(false),
      currentMa(DEFAULT_NORMAL_CURRENT),
      spreadCycleMode(true),
      microstepsVal(DEFAULT_MICROSTEPS),
      holdScale(DEFAULT_HOLD_SCALE),
      enabled(true),
      uartOk(false),
      driverVersion(0),
      stepTimer(nullptr)
{
    driver = new TMC2209Stepper(serial, rSense, uartAddress);
}

Motor::~Motor() {
    if (stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
        esp_timer_delete(stepTimer);
        stepTimer = nullptr;
    }
    delete driver;
}

// Hàm nội suy S-Curve Smoothstep: S(x) = 3*x^2 - 2*x^3
inline uint32_t Motor::calculateSCurveInterval(uint32_t currentStep, uint32_t totalSteps,
                                               uint32_t startInterval, uint32_t targetInterval) {
    if (totalSteps == 0 || currentStep >= totalSteps) return targetInterval;
    if (startInterval == targetInterval) return targetInterval;

    float x = (float)currentStep / (float)totalSteps;
    // Cubic Smoothstep S-curve
    float s = x * x * (3.0f - 2.0f * x);

    // Tính toán theo miền vận tốc (v = 1/T)
    float vStart = 1000000.0f / (float)startInterval;
    float vTarget = 1000000.0f / (float)targetInterval;
    float vCurrent = vStart + s * (vTarget - vStart);

    if (vCurrent <= 1.0f) return MAX_STEP_INTERVAL_US;
    uint32_t interval = (uint32_t)(1000000.0f / vCurrent);
    if (interval < MIN_STEP_INTERVAL_US) interval = MIN_STEP_INTERVAL_US;
    if (interval > MAX_STEP_INTERVAL_US) interval = MAX_STEP_INTERVAL_US;
    return interval;
}

void IRAM_ATTR Motor::onStepTimer(void* arg) {
    Motor* self = static_cast<Motor*>(arg);
    if (!self->running) return;

    // 1. Tạo xung bước phần cứng tối ưu
    gpio_set_level((gpio_num_t)self->stepPin, 1);
    delayMicroseconds(1);
    gpio_set_level((gpio_num_t)self->stepPin, 0);

    // 2. Xử lý S-Curve Ramping
    if (!self->continuousMode && self->stepsRemaining > 0 && self->stepsRemaining != 0xFFFFFFFF) {
        self->stepCounter++;
        self->stepsRemaining--;

        // Pha tăng tốc S-Curve (Acceleration)
        if (self->stepCounter < self->accelSteps) {
            self->currentSpeedUs = calculateSCurveInterval(self->stepCounter, self->accelSteps,
                                                           self->startSpeedUs, self->targetSpeedUs);
        }
        // Pha giảm tốc S-Curve (Deceleration)
        else if (self->stepsRemaining <= self->decelSteps) {
            self->currentSpeedUs = calculateSCurveInterval(self->stepsRemaining, self->decelSteps,
                                                           self->startSpeedUs, self->targetSpeedUs);
        }
        // Pha chạy đều (Cruise)
        else {
            self->currentSpeedUs = self->targetSpeedUs;
        }

        // Kiểm tra hoàn thành bước
        if (self->stepsRemaining == 0) {
            self->running = false;
            esp_timer_stop(self->stepTimer);
            return;
        }
    } else if (self->continuousMode) {
        // Chế độ quay liên tục với S-Curve tăng tốc mượt
        if (self->stepCounter < self->accelSteps) {
            self->stepCounter++;
            self->currentSpeedUs = calculateSCurveInterval(self->stepCounter, self->accelSteps,
                                                           self->startSpeedUs, self->targetSpeedUs);
        } else {
            self->currentSpeedUs = self->targetSpeedUs;
        }
    }

    // Cập nhật chu kỳ ngắt timer nếu tốc độ thay đổi
    if (self->stepTimer != nullptr && self->running) {
        uint32_t interval = self->currentSpeedUs;
        if (interval < MIN_STEP_INTERVAL_US) interval = MIN_STEP_INTERVAL_US;
        if (interval > MAX_STEP_INTERVAL_US) interval = MAX_STEP_INTERVAL_US;
        esp_timer_restart(self->stepTimer, interval);
    }
}

bool Motor::testUART() {
    if (serialPort != nullptr) {
        while (serialPort->available()) {
            serialPort->read();
        }
    }
    uint8_t v = driver->version();
    driverVersion = v;
    uartOk = (v == 0x21); // TMC2209 chuẩn trả về 0x21
    return uartOk;
}

TMC2209Diag Motor::getDriverStatus() {
    TMC2209Diag diag = {0};
    diag.uartOk = testUART();
    diag.driverVersion = driverVersion;

    if (diag.uartOk) {
        uint32_t drvStatus = driver->DRV_STATUS();
        diag.overTemp        = (drvStatus & (1UL << 1)) != 0;
        diag.overTempWarning = (drvStatus & (1UL << 0)) != 0;
        diag.shortToGndA     = (drvStatus & (1UL << 2)) != 0;
        diag.shortToGndB     = (drvStatus & (1UL << 3)) != 0;
        diag.openLoadA       = (drvStatus & (1UL << 4)) != 0;
        diag.openLoadB       = (drvStatus & (1UL << 5)) != 0;
        diag.standStill      = (drvStatus & (1UL << 31)) != 0;
        diag.csActual        = (drvStatus >> 16) & 0x1F;
        diag.sgResult        = driver->SG_RESULT();
    }
    return diag;
}

void Motor::begin(uint16_t initialCurrentMa, uint16_t initialMicrosteps,
                   bool initialSpreadCycle, uint8_t initialHoldScale, uint8_t iholddelay) {
    pinMode(stepPin, OUTPUT);
    digitalWrite(stepPin, LOW);

    if (dirPin != 255) {
        pinMode(dirPin, OUTPUT);
        digitalWrite(dirPin, LOW);
    }

    if (stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
        esp_timer_delete(stepTimer);
        stepTimer = nullptr;
    }

    esp_timer_create_args_t timerArgs = {
        .callback = &Motor::onStepTimer,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "motor_step_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&timerArgs, &stepTimer);

    driver->begin();
    driver->toff(4);
    enabled = true;
    driver->pdn_disable(true);
    driver->I_scale_analog(false);
    driver->mstep_reg_select(true);

    currentMa = initialCurrentMa;
    driver->rms_current(currentMa);

    microstepsVal = initialMicrosteps;
    driver->microsteps(microstepsVal);

    setChopperMode(initialSpreadCycle);

    holdScale = initialHoldScale;
    driver->ihold(holdScale);
    driver->iholddelay(iholddelay);

    // Initial direction
    driver->shaft(false);

    // Test UART
    testUART();
    if (uartOk) {
        Serial.printf("  >> [TMC2209 OK] %s (Addr %u, STEP Pin %d): UART Ket noi tot! Version: 0x%02X\n",
                      label, address, stepPin, driverVersion);
    } else {
        Serial.printf("  >> [TMC2209 CANH BAO] %s (Addr %u, STEP Pin %d): KHONG PHAN HOI UART! Version: 0x%02X\n",
                      label, address, stepPin, driverVersion);
    }
}

void Motor::run(bool cw, uint32_t steps) {
    if (!enabled) {
        enable(true);
    }
    dirCW = cw;

    // 1. Hardware DIR pin control (nếu có chân DIR vật lý)
    if (dirPin != 255) {
        digitalWrite(dirPin, cw ? HIGH : LOW);
    }

    // 2. UART register direction control
    driver->shaft(!cw);

    continuousMode = false;
    targetSteps = steps;
    stepsRemaining = steps;
    stepCounter = 0;

    // Tính toán số bước dốc tăng giảm tốc S-Curve
    if (steps > 150) {
        accelSteps = steps / 4;
        if (accelSteps > 100) accelSteps = 100;
        decelSteps = accelSteps;
    } else {
        accelSteps = steps / 2;
        decelSteps = steps - accelSteps;
    }

    startSpeedUs = targetSpeedUs + 600;
    if (startSpeedUs > MAX_STEP_INTERVAL_US) startSpeedUs = MAX_STEP_INTERVAL_US;
    currentSpeedUs = startSpeedUs;

    running = true;

    if (stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
        esp_timer_start_periodic(stepTimer, currentSpeedUs);
    }
}

void Motor::runContinuous(bool cw) {
    if (!enabled) {
        enable(true);
    }
    dirCW = cw;

    if (dirPin != 255) {
        digitalWrite(dirPin, cw ? HIGH : LOW);
    }
    driver->shaft(!cw);

    continuousMode = true;
    targetSteps = 0xFFFFFFFF;
    stepsRemaining = 0xFFFFFFFF;
    stepCounter = 0;
    accelSteps = 120;
    decelSteps = 0;

    startSpeedUs = targetSpeedUs + 600;
    if (startSpeedUs > MAX_STEP_INTERVAL_US) startSpeedUs = MAX_STEP_INTERVAL_US;
    currentSpeedUs = startSpeedUs;

    running = true;

    if (stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
        esp_timer_start_periodic(stepTimer, currentSpeedUs);
    }
}

void Motor::stop() {
    if (stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
    }
    running = false;
    continuousMode = false;
    stepsRemaining = 0;
    targetSteps = 0;
    stepCounter = 0;
    gpio_set_level((gpio_num_t)stepPin, 0);
}

void Motor::enable(bool en) {
    enabled = en;
    if (en) {
        driver->toff(4);
    } else {
        stop();
        driver->toff(0);
    }
}

void Motor::setSpeed(uint32_t intervalUs) {
    if (intervalUs < MIN_STEP_INTERVAL_US) intervalUs = MIN_STEP_INTERVAL_US;
    if (intervalUs > MAX_STEP_INTERVAL_US) intervalUs = MAX_STEP_INTERVAL_US;
    targetSpeedUs = intervalUs;
    if (!running) {
        currentSpeedUs = intervalUs;
    }
}

void Motor::setCurrent(uint16_t mA) {
    currentMa = mA;
    driver->rms_current(currentMa);
}

void Motor::setHold(uint8_t scale) {
    holdScale = scale;
    driver->ihold(holdScale);
}

void Motor::setChopperMode(bool spreadCycle) {
    spreadCycleMode = spreadCycle;
    driver->en_spreadCycle(spreadCycleMode);
    if (spreadCycleMode) {
        driver->pwm_autoscale(false);
        driver->pwm_autograd(false);
    } else {
        driver->pwm_autoscale(true);
        driver->pwm_autograd(true);
    }
}

void Motor::setMicrosteps(uint16_t ms) {
    microstepsVal = ms;
    driver->microsteps(ms);
}

void Motor::update() {
    // Pulse generation is handled at microsecond precision by esp_timer.
}

uint16_t Motor::getStallGuardResult() {
    return driver->SG_RESULT();
}

void Motor::setStallGuardThreshold(uint8_t threshold) {
    driver->SGTHRS(threshold);
}

void Motor::setupStallGuard(uint8_t threshold, uint32_t tcoolthrs) {
    setChopperMode(false);
    driver->pwm_autoscale(true);
    driver->pwm_autograd(true);
    driver->TCOOLTHRS(tcoolthrs);
    driver->SGTHRS(threshold);
    driver->semin(0);
    driver->semax(0);
}

String Motor::toJson() const {
    String j = "{";
    j += "\"running\":" + String(running ? "true" : "false") + ",";
    j += "\"dir\":\"" + String(dirCW ? "cw" : "ccw") + "\",";
    j += "\"stepsRemaining\":" + String(stepsRemaining) + ",";
    j += "\"targetSteps\":" + String(targetSteps) + ",";
    j += "\"stepIntervalUs\":" + String(targetSpeedUs) + ",";
    j += "\"currentSpeedUs\":" + String(currentSpeedUs) + ",";
    j += "\"currentMa\":" + String(currentMa) + ",";
    j += "\"holdScale\":" + String(holdScale) + ",";
    j += "\"spreadCycle\":" + String(spreadCycleMode ? "true" : "false") + ",";
    j += "\"microsteps\":" + String(microstepsVal) + ",";
    j += "\"uartOk\":" + String(uartOk ? "true" : "false") + ",";
    j += "\"version\":" + String(driverVersion);
    j += "}";
    return j;
}