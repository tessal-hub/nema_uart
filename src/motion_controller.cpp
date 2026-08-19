#include "motion_controller.h"

MotionController::MotionController(uint8_t axisIndex, Motor* m, Sensor* s)
    : axisId(axisIndex), motor(m), sensor(s),
      fullStepsPerRev(DEFAULT_FULL_STEPS), currentMicrosteps(DEFAULT_MICROSTEPS),
      gearRatio(DEFAULT_GEAR_RATIO),
      targetAngle(0.0f), currentAngle(0.0f),
      angleTolerance(DEFAULT_ANGLE_TOLERANCE),
      deadbandEnter(DEFAULT_DEADBAND_ENTER),
      deadbandExit(DEFAULT_DEADBAND_EXIT),
      inDeadband(false),
      baseIntervalUs(DEFAULT_STEP_INTERVAL_US),
      positioningActive(false), closedLoopHold(false),
      dirInvert(false), reachedTarget(false),
      runawayDetected(false), lastObservedError(0.0f), activeMoveStartMs(0),
      isHomed(false), zeroOffsetAngle(0.0f),
      totalStrokeDeg(0.0f), limitLeftDeg(0.0f), limitRightDeg(0.0f),
      normalCurrentMa(DEFAULT_NORMAL_CURRENT), homingCurrentMa(DEFAULT_HOMING_CURRENT),
      pendingTask(TASK_NONE) {
    snprintf(nvsNamespace, sizeof(nvsNamespace), "mctrl_%u", axisId);
    updateStepsPerDegree();
    calibData.isCalibrated = false;
}

void MotionController::updateStepsPerDegree() {
    stepsPerDegree = (fullStepsPerRev * currentMicrosteps * gearRatio) / 360.0f;
}

float MotionController::normalizeAngle(float a) {
    float res = fmodf(a, 360.0f);
    if (res < 0.0f) res += 360.0f;
    return res;
}

float MotionController::getShortestAngleError(float target, float current) {
    float diff = fmodf(target - current + 180.0f, 360.0f);
    if (diff < 0.0f) diff += 360.0f;
    return diff - 180.0f;
}

void MotionController::begin() {
    loadSettings();
    loadCalibration();
    motor->begin(normalCurrentMa, currentMicrosteps, true);
    motor->setSpeed(baseIntervalUs);
    currentAngle = getHomeRelativeAngle();
    targetAngle = currentAngle;
}

float MotionController::getCorrectedAngle() {
    float rawAngle = sensor->getAngle(axisId);
    if (!calibData.isCalibrated) {
        return rawAngle;
    }

    int idx0 = -1, idx1 = -1;
    for (int i = 0; i < CALIB_POINTS; i++) {
        int next = (i + 1) % CALIB_POINTS;
        float a0 = calibData.sensorAngle[i];
        float a1 = calibData.sensorAngle[next];

        if (a0 <= a1) {
            if (rawAngle >= a0 && rawAngle <= a1) {
                idx0 = i; idx1 = next;
                break;
            }
        } else {
            if (rawAngle >= a0 || rawAngle <= a1) {
                idx0 = i; idx1 = next;
                break;
            }
        }
    }

    if (idx0 == -1 || idx1 == -1) return rawAngle;

    float a0 = calibData.sensorAngle[idx0];
    float a1 = calibData.sensorAngle[idx1];
    float target0 = calibData.actualAngle[idx0];
    float target1 = calibData.actualAngle[idx1];

    float diffSensor = a1 - a0;
    if (diffSensor < 0.0f) diffSensor += 360.0f;

    float offsetSensor = rawAngle - a0;
    if (offsetSensor < 0.0f) offsetSensor += 360.0f;

    float t = (diffSensor > 0.0001f) ? (offsetSensor / diffSensor) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float diffTarget = target1 - target0;
    if (diffTarget < 0.0f) diffTarget += 360.0f;

    float corrected = target0 + t * diffTarget;
    return normalizeAngle(corrected);
}

float MotionController::getHomeRelativeAngle() {
    float currentAbs = getCorrectedAngle();
    if (!isHomed) return currentAbs;
    return getShortestAngleError(currentAbs, zeroOffsetAngle);
}

void MotionController::saveCalibration() {
    prefs.begin(nvsNamespace, false);
    prefs.putBytes("calib", &calibData, sizeof(CalibData));
    prefs.end();
    Serial.printf(">> [CALIB J%u] Da luu bang hieu chuan vao NVS (%s)!\n", axisId + 1, nvsNamespace);
}

void MotionController::loadCalibration() {
    prefs.begin(nvsNamespace, true);
    if (prefs.isKey("calib")) {
        prefs.getBytes("calib", &calibData, sizeof(CalibData));
        Serial.printf("[INIT J%u] Da tai bang hieu chuan NVS! (Trang thai: %s)\n",
                      axisId + 1, calibData.isCalibrated ? "DA HIEU CHUAN" : "CHUA HIEU CHUAN");
    } else {
        calibData.isCalibrated = false;
    }
    prefs.end();
}

void MotionController::clearCalibration() {
    calibData.isCalibrated = false;
    saveCalibration();
    Serial.printf(">> [CALIB J%u] Da XOA bang hieu chuan.\n", axisId + 1);
}

void MotionController::saveSettings() {
    prefs.begin(nvsNamespace, false);
    prefs.putFloat("gear", gearRatio);
    prefs.putBool("invert", dirInvert);
    prefs.putBool("hold", closedLoopHold);
    prefs.putUInt("speed", baseIntervalUs);
    prefs.putUShort("curr", normalCurrentMa);
    prefs.putFloat("db_in", deadbandEnter);
    prefs.putFloat("db_out", deadbandExit);
    prefs.end();
}

void MotionController::loadSettings() {
    prefs.begin(nvsNamespace, true);
    if (prefs.isKey("gear")) gearRatio = prefs.getFloat("gear", DEFAULT_GEAR_RATIO);
    if (prefs.isKey("invert")) dirInvert = prefs.getBool("invert", false);
    if (prefs.isKey("hold")) closedLoopHold = prefs.getBool("hold", false);
    if (prefs.isKey("speed")) baseIntervalUs = prefs.getUInt("speed", DEFAULT_STEP_INTERVAL_US);
    if (prefs.isKey("curr")) normalCurrentMa = prefs.getUShort("curr", DEFAULT_NORMAL_CURRENT);
    if (prefs.isKey("db_in")) deadbandEnter = prefs.getFloat("db_in", DEFAULT_DEADBAND_ENTER);
    if (prefs.isKey("db_out")) deadbandExit = prefs.getFloat("db_out", DEFAULT_DEADBAND_EXIT);
    prefs.end();

    updateStepsPerDegree();
}

void MotionController::setDeadband(float enterDeg, float exitDeg) {
    if (enterDeg > 0.01f && exitDeg > enterDeg) {
        deadbandEnter = enterDeg;
        deadbandExit = exitDeg;
        saveSettings();
    }
}

void MotionController::runAutoCalibration() {
    Serial.printf("\n[CALIB J%u] BAT DAU QUA TRINH AUTO CALIBRATION (16 DIEM)...\n", axisId + 1);

    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    motor->stop();
    calibData.isCalibrated = false;
    delay(300);

    const uint32_t totalStepsOneRev = (uint32_t)(fullStepsPerRev * currentMicrosteps * gearRatio + 0.5f);
    const uint32_t stepsPerPoint = totalStepsOneRev / CALIB_POINTS;

    motor->setSpeed(baseIntervalUs);
    bool testDir = true;
    if (dirInvert) testDir = !testDir;

    for (int i = 0; i < CALIB_POINTS; i++) {
        if (i > 0) {
            motor->run(testDir, stepsPerPoint);
            while (motor->isRunning()) {
                motor->update();
                delayMicroseconds(5);
            }
        }

        delay(350);

        float sum = 0.0f;
        for (int s = 0; s < 8; s++) {
            sum += sensor->getAngle(axisId);
            delay(10);
        }
        float measured = sum / 8.0f;
        float actual = i * (360.0f / CALIB_POINTS);

        calibData.sensorAngle[i] = measured;
        calibData.actualAngle[i] = actual;
    }

    calibData.isCalibrated = true;
    saveCalibration();

    Serial.printf("[CALIB J%u] AUTO CALIBRATION HOAN TAT!\n", axisId + 1);

    motor->run(!testDir, (CALIB_POINTS - 1) * stepsPerPoint);
    while (motor->isRunning()) {
        motor->update();
        delayMicroseconds(5);
    }
    delay(200);
}

bool MotionController::seekEndstopSmooth(bool dir, uint32_t maxSteps, float &hitAngle, bool isDebug) {
    motor->setCurrent(homingCurrentMa);
    motor->setChopperMode(true);

    uint32_t currentInterval = 1200;
    const uint32_t targetInterval = 400;

    motor->setSpeed(currentInterval);
    motor->run(dir, maxSteps);

    bool stallDetected = false;
    unsigned long startTime = millis();
    unsigned long lastCheckMs = millis();

    float lastAngle = getCorrectedAngle();
    uint32_t stepsDone = 0;

    while (motor->isRunning()) {
        motor->update();
        unsigned long now = millis();

        if (currentInterval > targetInterval && (stepsDone % 3 == 0)) {
            currentInterval -= 10;
            if (currentInterval < targetInterval) currentInterval = targetInterval;
            motor->setSpeed(currentInterval);
        }
        stepsDone++;

        if (now - startTime > 250) {
            if (now - lastCheckMs >= 100) {
                lastCheckMs = now;
                float currentDeg = getCorrectedAngle();
                float deltaAngle = fabs(getShortestAngleError(currentDeg, lastAngle));

                if (deltaAngle < 0.08f) {
                    stallDetected = true;
                    hitAngle = currentDeg;
                    motor->stop();
                    break;
                }
                lastAngle = currentDeg;
            }
        }
        delayMicroseconds(5);
    }

    if (!stallDetected) {
        hitAngle = getCorrectedAngle();
    }

    return stallDetected;
}

void MotionController::runCenterHoming(bool isDebug) {
    Serial.printf("\n[HOMING J%u] BAT DAU HOMING TRUNG DIEM...\n", axisId + 1);

    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    motor->stop();
    delay(200);

    const uint32_t MAX_STEPS = (uint32_t)(fullStepsPerRev * currentMicrosteps * gearRatio * 3.0f);
    const uint32_t backoffSteps = (uint32_t)(5.0f * stepsPerDegree);

    // BƯỚC 1: Tìm cữ chặn bên Trái (CCW)
    float angleMin = 0.0f;
    bool foundMin = seekEndstopSmooth(false, MAX_STEPS, angleMin, isDebug);

    if (!foundMin) {
        Serial.printf("[LOI HOMING J%u] Khong tim thay cu chan Trai!\n", axisId + 1);
        motor->setCurrent(normalCurrentMa);
        return;
    }

    motor->setSpeed(600);
    motor->run(true, backoffSteps);
    while (motor->isRunning()) {
        motor->update();
        delayMicroseconds(5);
    }
    delay(400);

    // BƯỚC 2: Tìm cữ chặn bên Phải (CW)
    float angleMax = 0.0f;
    bool foundMax = seekEndstopSmooth(true, MAX_STEPS, angleMax, isDebug);

    if (!foundMax) {
        Serial.printf("[LOI HOMING J%u] Khong tim thay cu chan Phai!\n", axisId + 1);
        motor->setCurrent(normalCurrentMa);
        return;
    }

    motor->setSpeed(600);
    motor->run(false, backoffSteps);
    while (motor->isRunning()) {
        motor->update();
        delayMicroseconds(5);
    }
    delay(400);

    // BƯỚC 3: Tính toán theo cung lớn
    float arcCW = angleMax - angleMin;
    if (arcCW < 0.0f) arcCW += 360.0f;
    float arcCCW = 360.0f - arcCW;

    float majorArc = (arcCW >= arcCCW) ? arcCW : arcCCW;
    float centerAbsoluteAngle = 0.0f;
    if (arcCW >= arcCCW) {
        centerAbsoluteAngle = normalizeAngle(angleMin + (majorArc / 2.0f));
    } else {
        centerAbsoluteAngle = normalizeAngle(angleMin - (majorArc / 2.0f));
    }

    float halfStrokeDeg = majorArc / 2.0f;
    totalStrokeDeg = majorArc;

    float distFromRightToCenter = halfStrokeDeg - 5.0f;
    if (distFromRightToCenter < 0.0f) distFromRightToCenter = 0.0f;
    uint32_t stepsToCenter = (uint32_t)(distFromRightToCenter * stepsPerDegree + 0.5f);

    motor->setCurrent(normalCurrentMa);
    motor->setSpeed(baseIntervalUs);

    motor->run(false, stepsToCenter);
    while (motor->isRunning()) {
        motor->update();
        delayMicroseconds(5);
    }
    delay(300);

    // XÁC LẬP HOME TẠI TRUNG ĐIỂM
    zeroOffsetAngle = centerAbsoluteAngle;
    isHomed = true;
    targetAngle = 0.0f;
    currentAngle = 0.0f;
    limitLeftDeg = -halfStrokeDeg;
    limitRightDeg = +halfStrokeDeg;

    Serial.printf("[HOMING J%u] HOAN TAT! Set Home tai 0.00 deg (Stroke: %.2f deg)\n", axisId + 1, totalStrokeDeg);
}

void MotionController::setTargetAngle(float target) {
    if (isHomed && totalStrokeDeg > 0.0f) {
        float maxLimit = limitRightDeg - 1.0f;
        float minLimit = limitLeftDeg + 1.0f;
        if (target > maxLimit) target = maxLimit;
        if (target < minLimit) target = minLimit;
    }

    targetAngle = target;
    positioningActive = true;
    reachedTarget = false;
    inDeadband = false;
    runawayDetected = false;
    lastObservedError = fabs(getShortestAngleError(targetAngle, getHomeRelativeAngle()));
    activeMoveStartMs = millis();
}

void MotionController::jog(float delta) {
    setTargetAngle(getHomeRelativeAngle() + delta);
}

void MotionController::stop() {
    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    motor->stop();
}

void MotionController::moveRawSteps(bool cw, uint32_t steps, uint32_t speedUs) {
    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    if (speedUs >= 100 && speedUs <= 5000) {
        motor->setSpeed(speedUs);
    } else {
        motor->setSpeed(baseIntervalUs);
    }
    bool dir = cw;
    if (dirInvert) dir = !dir;
    motor->run(dir, steps);
}

void MotionController::runContinuous(bool cw, uint32_t speedUs) {
    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    if (speedUs >= 100 && speedUs <= 5000) {
        motor->setSpeed(speedUs);
    } else {
        motor->setSpeed(baseIntervalUs);
    }
    bool dir = cw;
    if (dirInvert) dir = !dir;
    motor->run(dir, 0xFFFFFFFF);
}

void MotionController::setDriverEnabled(bool enabled) {
    if (!enabled) {
        stop();
    }
    motor->enable(enabled);
}

void MotionController::setSpeed(uint32_t speedUs) {
    if (speedUs >= 100 && speedUs <= 2000) {
        baseIntervalUs = speedUs;
        motor->setSpeed(baseIntervalUs);
        saveSettings();
    }
}

void MotionController::setCurrent(uint16_t ma) {
    if (ma >= 200 && ma <= 1400) {
        normalCurrentMa = ma;
        motor->setCurrent(normalCurrentMa);
        saveSettings();
    }
}

void MotionController::setGearRatio(float ratio) {
    if (ratio >= 0.1f && ratio <= 100.0f) {
        gearRatio = ratio;
        updateStepsPerDegree();
        saveSettings();
    }
}

void MotionController::update() {
    // 1. Sinh xung bước non-blocking
    motor->update();

    // 2. Thực thi các tác vụ homing / calib bất đồng bộ
    if (pendingTask == TASK_HOME) {
        pendingTask = TASK_NONE;
        runCenterHoming();
    } else if (pendingTask == TASK_CALIB) {
        pendingTask = TASK_NONE;
        runAutoCalibration();
    }

    // 3. Vòng điều khiển vị trí bám góc (Closed-loop với Schmitt-Trigger Deadband & Runaway Protection)
    if (!positioningActive && !closedLoopHold) return;

    currentAngle = getHomeRelativeAngle();
    if (!sensor->isSensorOK(axisId)) {
        motor->stop();
        positioningActive = false;
        inDeadband = false;
        return;
    }

    float err = getShortestAngleError(targetAngle, currentAngle);
    float absErr = fabs(err);

    // Runaway Protection: Kiểm tra nếu động cơ quay làm sai số TĂNG LÊN thay vì giảm đi (Ngược chiều Invert)
    if (motor->isRunning() && (millis() - activeMoveStartMs > 300)) {
        if (absErr > (lastObservedError + RUNAWAY_ERROR_THRESHOLD)) {
            motor->stop();
            positioningActive = false;
            closedLoopHold = false;
            runawayDetected = true;
            Serial.printf(">> [CANH BAO NGUOC CHIEU J%u] Sai so tang tu %.1f do -> %.1f do! Da dung an toan. Vui long bat Invert (dao chieu)!\n",
                          axisId + 1, lastObservedError, absErr);
            return;
        }
    }

    // Schmitt-Trigger Deadband Logic
    if (inDeadband) {
        if (absErr > deadbandExit) {
            inDeadband = false;
            reachedTarget = false;
            lastObservedError = absErr;
            activeMoveStartMs = millis();
        } else {
            return;
        }
    }

    if (absErr <= deadbandEnter) {
        if (motor->isRunning()) {
            motor->stop();
        }
        reachedTarget = true;
        inDeadband = true;

        if (!closedLoopHold) {
            positioningActive = false;
        }
        return;
    }

    reachedTarget = false;

    if (!motor->isRunning()) {
        bool dir = (err > 0);
        if (dirInvert) dir = !dir;

        uint32_t neededSteps = (uint32_t)(absErr * stepsPerDegree + 0.5f);
        if (neededSteps == 0) neededSteps = 1;

        uint32_t interval = baseIntervalUs;
        if (absErr < 3.0f) {
            interval = baseIntervalUs * 2;
        } else if (absErr < 10.0f) {
            interval = (uint32_t)(baseIntervalUs * 1.4f);
        }
        motor->setSpeed(interval);
        lastObservedError = absErr;
        activeMoveStartMs = millis();
        motor->run(dir, neededSteps);
    }
}
