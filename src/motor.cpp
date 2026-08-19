#include "motor.h"

Motor::Motor(HardwareSerial* serialPort, float rSense, uint8_t uartAddress,
             uint8_t stepPinNum, const char* motorLabel)
    : driver(nullptr),
      stepPin(stepPinNum),
      address(uartAddress),
      label(motorLabel),
      running(false),
      dirCW(true),
      stepIntervalUs(1000),
      targetSteps(0),
      stepsRemaining(0),
      currentMa(700),
      spreadCycleMode(true),
      microstepsVal(16),
      holdScale(8),
      enabled(true),
      stepTimer(nullptr)
{
    driver = new TMC2209Stepper(serialPort, rSense, uartAddress);
}

Motor::~Motor() {
    if (stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
        esp_timer_delete(stepTimer);
        stepTimer = nullptr;
    }
    delete driver;
}

void IRAM_ATTR Motor::onStepTimer(void* arg) {
    Motor* self = static_cast<Motor*>(arg);
    if (!self->running) return;

    digitalWrite(self->stepPin, HIGH);
    delayMicroseconds(2);
    digitalWrite(self->stepPin, LOW);

    if (self->stepsRemaining > 0 && self->stepsRemaining != 0xFFFFFFFF) {
        self->stepsRemaining--;
        if (self->stepsRemaining == 0) {
            self->running = false;
            esp_timer_stop(self->stepTimer);
        }
    }
}

void Motor::begin(uint16_t initialCurrentMa, uint16_t initialMicrosteps,
                   bool initialSpreadCycle, uint8_t initialHoldScale, uint8_t iholddelay) {
    pinMode(stepPin, OUTPUT);
    digitalWrite(stepPin, LOW);

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
}

void Motor::run(bool cw, uint32_t steps) {
    if (!enabled) {
        enable(true);
    }
    dirCW = cw;
    driver->shaft(!cw);

    stepsRemaining = steps;
    targetSteps = steps;
    running = true;

    if (stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
        esp_timer_start_periodic(stepTimer, stepIntervalUs);
    }
}

void Motor::stop() {
    running = false;
    stepsRemaining = 0;
    targetSteps = 0;
    if (stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
    }
    digitalWrite(stepPin, LOW);
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
    stepIntervalUs = intervalUs;
    if (running && stepTimer != nullptr) {
        esp_timer_stop(stepTimer);
        esp_timer_start_periodic(stepTimer, stepIntervalUs);
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
    // Pulse generation is handled with hardware precision by esp_timer interrupt.
}

uint16_t Motor::getStallGuardResult() {
    return driver->SG_RESULT();
}

void Motor::setStallGuardThreshold(uint8_t threshold) {
    driver->SGTHRS(threshold);
}

void Motor::setupStallGuard(uint8_t threshold, uint32_t tcoolthrs) {
    setChopperMode(false); // StallGuard4 yêu cầu StealthChop
    driver->pwm_autoscale(true);
    driver->pwm_autograd(true);
    driver->TCOOLTHRS(tcoolthrs);
    driver->SGTHRS(threshold);
    driver->semin(0);
    driver->semax(0);
}

uint32_t Motor::getDriverVersion() {
    return driver->version();
}

String Motor::toJson() const {
    String j = "{";
    j += "\"running\":" + String(running ? "true" : "false") + ",";
    j += "\"dir\":\"" + String(dirCW ? "cw" : "ccw") + "\",";
    j += "\"stepsRemaining\":" + String(stepsRemaining) + ",";
    j += "\"targetSteps\":" + String(targetSteps) + ",";
    j += "\"stepIntervalUs\":" + String(stepIntervalUs) + ",";
    j += "\"currentMa\":" + String(currentMa) + ",";
    j += "\"holdScale\":" + String(holdScale) + ",";
    j += "\"spreadCycle\":" + String(spreadCycleMode ? "true" : "false") + ",";
    j += "\"microsteps\":" + String(microstepsVal);
    j += "}";
    return j;
}