#include "motor.h"
#include "driver/gpio.h"

Motor::Motor(HardwareSerial* serial, float rSense, uint8_t uartAddress,
             uint8_t stepPinNum, uint8_t dirPinNum, const char* motorLabel)
    : serialPort(serial),
      uartMutex(nullptr),
      driver(nullptr),
      stepPin(stepPinNum),
      dirPin(dirPinNum),
      address(uartAddress),
      label(motorLabel),
      running(false),
      dirCW(true),
      lastShaftDir(-1),
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
    delete driver;  // TMC2209Stepper has no virtual dtor; safe because we know the concrete type
#pragma GCC diagnostic pop
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

    // 1. Generate step pulse HIGH (TMC2209 min pulse ≥1µs; the ISR overhead itself
    //    provides the required pulse width before the CLEAR gpio below executes)
    gpio_set_level((gpio_num_t)self->stepPin, 1);

    // 2. Compute the next interval BEFORE clearing the pin so the GPIO HIGH time
    //    is at least the instruction-execution time (~200ns on 240MHz core).
    //    Then determine new interval via S-Curve.
    uint32_t nextInterval = self->targetSpeedUs;

    if (!self->continuousMode && self->stepsRemaining > 0 && self->stepsRemaining != 0xFFFFFFFF) {
        self->stepCounter++;
        self->stepsRemaining--;

        // Acceleration phase
        if (self->stepCounter < self->accelSteps) {
            nextInterval = calculateSCurveInterval(self->stepCounter, self->accelSteps,
                                                   self->startSpeedUs, self->targetSpeedUs);
        }
        // Deceleration phase
        else if (self->stepsRemaining <= self->decelSteps) {
            nextInterval = calculateSCurveInterval(self->stepsRemaining, self->decelSteps,
                                                   self->startSpeedUs, self->targetSpeedUs);
        }

        self->currentSpeedUs = nextInterval;

        // Terminate when all steps done
        if (self->stepsRemaining == 0) {
            gpio_set_level((gpio_num_t)self->stepPin, 0);
            self->running = false;
            return;          // Timer is one-shot; do NOT reschedule
        }
    } else if (self->continuousMode) {
        // Continuous mode — accelerate then cruise
        if (self->stepCounter < self->accelSteps) {
            self->stepCounter++;
            nextInterval = calculateSCurveInterval(self->stepCounter, self->accelSteps,
                                                   self->startSpeedUs, self->targetSpeedUs);
        }
        self->currentSpeedUs = nextInterval;
    }

    // 3. Clamp interval
    if (nextInterval < MIN_STEP_INTERVAL_US) nextInterval = MIN_STEP_INTERVAL_US;
    if (nextInterval > MAX_STEP_INTERVAL_US) nextInterval = MAX_STEP_INTERVAL_US;

    // 4. Re-arm timer for the next step (one-shot)
    esp_timer_start_once(self->stepTimer, nextInterval);

    // 5. Clear step pin — done AFTER re-arming so scheduling latency doesn't
    //    eat into pulse width. The pin will be HIGH for at least the time it took
    //    to execute steps 2-4 (~1–2µs at 240 MHz), satisfying TMC2209's 100ns min.
    gpio_set_level((gpio_num_t)self->stepPin, 0);
}

bool Motor::testUART() {
    bool ok = false;
    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (serialPort != nullptr) {
                while (serialPort->available()) serialPort->read();
            }
            uint8_t v = driver->version();
            driverVersion = v;
            uartOk = (v == 0x21); // TMC2209 chuẩn trả về 0x21
            ok = uartOk;
            xSemaphoreGive(*uartMutex);
        }
    } else {
        if (serialPort != nullptr) {
            while (serialPort->available()) serialPort->read();
        }
        uint8_t v = driver->version();
        driverVersion = v;
        uartOk = (v == 0x21);
        ok = uartOk;
    }
    return ok;
}

TMC2209Diag Motor::getDriverStatus() {
    TMC2209Diag diag = {};

    auto doRead = [&]() {
        if (serialPort) { while (serialPort->available()) serialPort->read(); }
        uint8_t v = driver->version();
        driverVersion = v;
        uartOk = (v == 0x21);
        diag.uartOk = uartOk;
        diag.driverVersion = driverVersion;

        if (uartOk) {
            if (serialPort) { while (serialPort->available()) serialPort->read(); }
            uint32_t drvStatus = driver->DRV_STATUS();
            diag.overTemp        = (drvStatus & (1UL << 1)) != 0;
            diag.overTempWarning = (drvStatus & (1UL << 0)) != 0;
            diag.shortToGndA     = (drvStatus & (1UL << 2)) != 0;
            diag.shortToGndB     = (drvStatus & (1UL << 3)) != 0;
            diag.openLoadA       = (drvStatus & (1UL << 4)) != 0;
            diag.openLoadB       = (drvStatus & (1UL << 5)) != 0;
            diag.standStill      = (drvStatus & (1UL << 31)) != 0;
            diag.csActual        = (drvStatus >> 16) & 0x1F;
            if (serialPort) { while (serialPort->available()) serialPort->read(); }
            diag.sgResult        = driver->SG_RESULT();
        }
    };

    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            doRead();
            xSemaphoreGive(*uartMutex);
        }
    } else {
        doRead();
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
        .dispatch_method = ESP_TIMER_TASK,  // Note: ESP_TIMER_ISR requires IDF 5.2+; using TASK for IDF 5.1 compat.
        .name = "motor_step",
        .skip_unhandled_events = true
    };
    esp_err_t ret = esp_timer_create(&timerArgs, &stepTimer);
    if (ret != ESP_OK) {
        Serial.printf("  >> [ERROR] %s: esp_timer_create failed: %d\n", label, ret);
        stepTimer = nullptr;
    }

    if (uartMutex != nullptr && *uartMutex != nullptr) {
        xSemaphoreTake(*uartMutex, portMAX_DELAY);
    }
    if (serialPort != nullptr) {
        while (serialPort->available()) serialPort->read();
    }

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

    spreadCycleMode = initialSpreadCycle;
    driver->en_spreadCycle(spreadCycleMode);
    if (spreadCycleMode) {
        driver->pwm_autoscale(false);
        driver->pwm_autograd(false);
    } else {
        driver->pwm_autoscale(true);
        driver->pwm_autograd(true);
    }

    holdScale = initialHoldScale;
    driver->ihold(holdScale);
    driver->iholddelay(iholddelay);

    // Initial direction: force cache invalidation so the first setDirection() always sends UART
    driver->shaft(false);
    lastShaftDir = -1;

    if (uartMutex != nullptr && *uartMutex != nullptr) {
        xSemaphoreGive(*uartMutex);
    }

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

void Motor::setDirection(bool cw) {
    dirCW = cw;

    // 1. Hardware DIR pin control (0ms delay GPIO switch)
    if (dirPin != 255) {
        gpio_set_level((gpio_num_t)dirPin, cw ? 1 : 0);
    }

    // 2. UART register direction control - CHỈ gửi UART khi chiều thực sự THAY ĐỔI
    if (lastShaftDir != (int8_t)cw) {
        if (uartMutex != nullptr && *uartMutex != nullptr) {
            if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (serialPort != nullptr) {
                    while (serialPort->available()) serialPort->read();
                }
                driver->shaft(cw);
                lastShaftDir = (int8_t)cw;
                xSemaphoreGive(*uartMutex);
            }
        } else {
            if (serialPort != nullptr) {
                while (serialPort->available()) serialPort->read();
            }
            driver->shaft(cw);
            lastShaftDir = (int8_t)cw;
        }
    }
}

void Motor::run(bool cw, uint32_t steps) {
    if (!enabled) {
        enable(true);
    }
    setDirection(cw);

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
        esp_timer_start_once(stepTimer, currentSpeedUs);
    }
}

void Motor::runContinuous(bool cw) {
    if (!enabled) {
        enable(true);
    }
    setDirection(cw);

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
        esp_timer_start_once(stepTimer, currentSpeedUs);
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
        if (uartMutex != nullptr && *uartMutex != nullptr) {
            if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
                driver->toff(4);
                xSemaphoreGive(*uartMutex);
            }
        } else {
            driver->toff(4);
        }
    } else {
        stop();
        if (uartMutex != nullptr && *uartMutex != nullptr) {
            if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
                driver->toff(0);
                xSemaphoreGive(*uartMutex);
            }
        } else {
            driver->toff(0);
        }
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
    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
            driver->rms_current(currentMa);
            xSemaphoreGive(*uartMutex);
        }
    } else {
        driver->rms_current(currentMa);
    }
}

void Motor::setHold(uint8_t scale) {
    holdScale = scale;
    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
            driver->ihold(holdScale);
            xSemaphoreGive(*uartMutex);
        }
    } else {
        driver->ihold(holdScale);
    }
}

void Motor::setChopperMode(bool spreadCycle) {
    spreadCycleMode = spreadCycle;
    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
            driver->en_spreadCycle(spreadCycleMode);
            if (spreadCycleMode) {
                driver->pwm_autoscale(false);
                driver->pwm_autograd(false);
            } else {
                driver->pwm_autoscale(true);
                driver->pwm_autograd(true);
            }
            xSemaphoreGive(*uartMutex);
        }
    } else {
        driver->en_spreadCycle(spreadCycleMode);
        if (spreadCycleMode) {
            driver->pwm_autoscale(false);
            driver->pwm_autograd(false);
        } else {
            driver->pwm_autoscale(true);
            driver->pwm_autograd(true);
        }
    }
}

void Motor::setMicrosteps(uint16_t ms) {
    microstepsVal = ms;
    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
            driver->microsteps(ms);
            xSemaphoreGive(*uartMutex);
        }
    } else {
        driver->microsteps(ms);
    }
}

void Motor::update() {
    // Pulse generation is handled with hardware precision by esp_timer interrupt.
}

uint16_t Motor::getStallGuardResult() {
    uint16_t res = 0;
    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
            res = driver->SG_RESULT();
            xSemaphoreGive(*uartMutex);
        }
    } else {
        res = driver->SG_RESULT();
    }
    return res;
}

void Motor::setStallGuardThreshold(uint8_t threshold) {
    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
            driver->SGTHRS(threshold);
            xSemaphoreGive(*uartMutex);
        }
    } else {
        driver->SGTHRS(threshold);
    }
}

void Motor::setupStallGuard(uint8_t threshold, uint32_t tcoolthrs) {
    setChopperMode(false);
    if (uartMutex != nullptr && *uartMutex != nullptr) {
        if (xSemaphoreTake(*uartMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (serialPort != nullptr) { while (serialPort->available()) serialPort->read(); }
            driver->pwm_autoscale(true);
            driver->pwm_autograd(true);
            driver->TCOOLTHRS(tcoolthrs);
            driver->SGTHRS(threshold);
            driver->semin(0);
            driver->semax(0);
            xSemaphoreGive(*uartMutex);
        }
    } else {
        driver->pwm_autoscale(true);
        driver->pwm_autograd(true);
        driver->TCOOLTHRS(tcoolthrs);
        driver->SGTHRS(threshold);
        driver->semin(0);
        driver->semax(0);
    }
}

String Motor::toJson() const {
    String j;
    j.reserve(256);
    j = "{";
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