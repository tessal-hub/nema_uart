#include "motion_controller.h"
#include <esp_task_wdt.h>

MotionController::MotionController(uint8_t axisIndex, Motor* m, Sensor* s)
    : axisId(axisIndex), motor(m), sensor(s),
      fullStepsPerRev(DEFAULT_FULL_STEPS), currentMicrosteps(DEFAULT_MICROSTEPS),
      gearRatio((axisIndex < NUM_MOTORS) ? DEFAULT_AXIS_GEAR_RATIOS[axisIndex] : 6.0f),
      targetAngle(0.0f), currentAngle(0.0f),
      angleTolerance(DEFAULT_ANGLE_TOLERANCE),
      deadbandEnter(DEFAULT_DEADBAND_ENTER),
      deadbandExit(DEFAULT_DEADBAND_EXIT),
      inDeadband(false),
      baseIntervalUs(DEFAULT_STEP_INTERVAL_US),
      syncIntervalUs(DEFAULT_STEP_INTERVAL_US),
      isSynchronizedMove(false),
      positioningActive(false), closedLoopHold(false),
      dirInvert(false), reachedTarget(false),
      runawayDetected(false), prevCycleError(0.0f), errorIncreasingStreak(0), lastTrendCheckMs(0),
      isHomed(false), zeroOffsetAngle(0.0f),
      totalStrokeDeg(360.0f), limitLeftDeg(-180.0f), limitRightDeg(180.0f),
      normalCurrentMa(DEFAULT_NORMAL_CURRENT), homingCurrentMa(DEFAULT_HOMING_CURRENT),
      stallThreshold(DEFAULT_STALL_THRESHOLD),
      pendingTask(TASK_NONE) {
    snprintf(nvsNamespace, sizeof(nvsNamespace), "mctrl_%u", axisId);
    updateStepsPerDegree();
    calibData.isCalibrated = false;
}

void MotionController::updateStepsPerDegree() {
    stepsPerDegree = (fullStepsPerRev * currentMicrosteps * fabsf(gearRatio)) / 360.0f;
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
    prefs.putUShort("h_curr", homingCurrentMa);
    prefs.putUChar("sg_th", stallThreshold);
    prefs.putFloat("db_in", deadbandEnter);
    prefs.putFloat("db_out", deadbandExit);
    prefs.putBool("homed", isHomed);
    prefs.putFloat("z_off", zeroOffsetAngle);
    prefs.putFloat("lim_l", limitLeftDeg);
    prefs.putFloat("lim_r", limitRightDeg);
    prefs.putFloat("stroke", totalStrokeDeg);
    prefs.end();
}

void MotionController::loadSettings() {
    prefs.begin(nvsNamespace, true);
    float defRatio = (axisId < NUM_MOTORS) ? DEFAULT_AXIS_GEAR_RATIOS[axisId] : 6.0f;
    if (prefs.isKey("gear")) gearRatio = prefs.getFloat("gear", defRatio);
    else gearRatio = defRatio;

    // Tự động gán dirInvert = true nếu tỉ số truyền âm và chưa từng cài đặt invert trong NVS
    bool defInvert = (gearRatio < 0.0f);
    if (prefs.isKey("invert")) dirInvert = prefs.getBool("invert", defInvert);
    else dirInvert = defInvert;
    if (prefs.isKey("hold")) closedLoopHold = prefs.getBool("hold", false);
    if (prefs.isKey("speed")) baseIntervalUs = prefs.getUInt("speed", DEFAULT_STEP_INTERVAL_US);
    if (prefs.isKey("curr")) normalCurrentMa = prefs.getUShort("curr", DEFAULT_NORMAL_CURRENT);
    if (prefs.isKey("h_curr")) homingCurrentMa = prefs.getUShort("h_curr", DEFAULT_HOMING_CURRENT);
    if (prefs.isKey("sg_th")) stallThreshold = prefs.getUChar("sg_th", DEFAULT_STALL_THRESHOLD);
    if (prefs.isKey("db_in")) deadbandEnter = prefs.getFloat("db_in", DEFAULT_DEADBAND_ENTER);
    if (prefs.isKey("db_out")) deadbandExit = prefs.getFloat("db_out", DEFAULT_DEADBAND_EXIT);
    if (prefs.isKey("homed")) isHomed = prefs.getBool("homed", false);
    if (prefs.isKey("z_off")) zeroOffsetAngle = prefs.getFloat("z_off", 0.0f);
    if (prefs.isKey("lim_l")) limitLeftDeg = prefs.getFloat("lim_l", -180.0f);
    if (prefs.isKey("lim_r")) limitRightDeg = prefs.getFloat("lim_r", 180.0f);
    if (prefs.isKey("stroke")) totalStrokeDeg = prefs.getFloat("stroke", 360.0f);
    prefs.end();

    updateStepsPerDegree();
}

void MotionController::setHomingCurrent(uint16_t ma) {
    if (ma >= 200 && ma <= 1400) {
        homingCurrentMa = ma;
        saveSettings();
    }
}

void MotionController::setStallThreshold(uint8_t th) {
    stallThreshold = th;
    motor->setStallGuardThreshold(stallThreshold);
    saveSettings();
}

void MotionController::setLimits(float minDeg, float maxDeg) {
    if (maxDeg > minDeg) {
        limitLeftDeg = minDeg;
        limitRightDeg = maxDeg;
        totalStrokeDeg = maxDeg - minDeg;
        saveSettings();
    }
}

void MotionController::setDeadband(float enterDeg, float exitDeg) {
    if (enterDeg > 0.01f && exitDeg > enterDeg) {
        deadbandEnter = enterDeg;
        deadbandExit = exitDeg;
        saveSettings();
    }
}

void MotionController::setHomeHere() {
    zeroOffsetAngle = getCorrectedAngle();
    isHomed = true;
    targetAngle = 0.0f;
    currentAngle = 0.0f;
    saveSettings();
    Serial.printf(">> [ZERO J%u] Dat mốc Home 0.00 deg tai goc thuc: %.2f deg\n", axisId + 1, zeroOffsetAngle);
}

bool MotionController::detectAndAutoSetDirection(bool returnToStart) {
    Serial.printf("\n[AUTO-DIR J%u] Kiem tra chieu quay dong co vs cam bien AS5600...\n", axisId + 1);

    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    isSynchronizedMove = false;
    motor->stop();
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_task_wdt_reset();

    // 1. Doc goc ban dau (lay trung binh 5 mau loc nhieu)
    float startAngle = 0.0f;
    for (int i = 0; i < 5; i++) {
        startAngle += sensor->getAngle(axisId);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    startAngle /= 5.0f;

    // Tinh so buoc test tuong ung ~1.5 - 2.0 do goc quay khop
    float absGear = fabsf(gearRatio);
    if (absGear < 0.1f) absGear = 1.0f;
    uint32_t testSteps = (uint32_t)(absGear * fullStepsPerRev * currentMicrosteps * 2.0f / 360.0f + 0.5f);
    if (testSteps < 60) testSteps = 60;
    if (testSteps > 1500) testSteps = 1500;

    motor->setCurrent(normalCurrentMa);
    motor->setSpeed(HOMING_STEP_INTERVAL_US);

    // Buoc 1: Chay thu ve huong CW (true)
    motor->run(true, testSteps);
    while (motor->isRunning()) {
        vTaskDelay(pdMS_TO_TICKS(1));
        esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_task_wdt_reset();

    float angleAfterCW = 0.0f;
    for (int i = 0; i < 5; i++) {
        angleAfterCW += sensor->getAngle(axisId);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    angleAfterCW /= 5.0f;

    float deltaCW = getShortestAngleError(angleAfterCW, startAngle);
    bool determined = false;

    if (fabsf(deltaCW) >= 0.15f) {
        // Phat hien chuyen dong ro rang khi chay CW
        if (deltaCW > 0.0f) {
            dirInvert = false; // Motor CW -> Goc tang (Cung chieu thuan)
        } else {
            dirInvert = true;  // Motor CW -> Goc giam (Hop so ty le am hoac nguoc cuc tu)
        }
        determined = true;
    } else {
        // Co the dang cham cu chan CW, thu test chieu nguoc lai CCW (false)
        motor->run(false, testSteps * 2);
        while (motor->isRunning()) {
            vTaskDelay(pdMS_TO_TICKS(1));
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(150));
        esp_task_wdt_reset();

        float angleAfterCCW = 0.0f;
        for (int i = 0; i < 5; i++) {
            angleAfterCCW += sensor->getAngle(axisId);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        angleAfterCCW /= 5.0f;

        float deltaCCW = getShortestAngleError(angleAfterCCW, startAngle);
        if (fabsf(deltaCCW) >= 0.15f) {
            if (deltaCCW < 0.0f) {
                dirInvert = false; // CCW -> Goc giam => CW se la goc tang (Thuan)
            } else {
                dirInvert = true;  // CCW -> Goc tang => CW se la goc giam (Nguoc)
            }
            determined = true;
        }

        // Tra ve vi tri ban dau neu da chay 2x CCW
        if (returnToStart) {
            motor->run(true, testSteps);
            while (motor->isRunning()) {
                vTaskDelay(pdMS_TO_TICKS(1));
                esp_task_wdt_reset();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    if (determined) {
        saveSettings();
        Serial.printf(">> [AUTO-DIR J%u] HOAN TAT! Delta = %+.2f deg ==> Tu dong cai dirInvert = %s (Da luu vao Flash NVS)!\n",
                      axisId + 1, (fabsf(deltaCW) >= 0.15f ? deltaCW : 0.0f),
                      dirInvert ? "TRUE (DAO CHIEU)" : "FALSE (CHIEU THUAN)");
    } else {
        Serial.printf(">> [AUTO-DIR J%u] CANH BAO: Khong do duoc bien thien goc ro rang (Delta < 0.15 deg).\n", axisId + 1);
    }

    // Tra ve vi tri xuat phat neu chay don huong CW
    if (returnToStart && determined && fabsf(deltaCW) >= 0.15f) {
        motor->run(false, testSteps);
        while (motor->isRunning()) {
            vTaskDelay(pdMS_TO_TICKS(1));
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return determined;
}

void MotionController::runAutoCalibration() {
    Serial.printf("\n[CALIB J%u] BAT DAU QUA TRINH AUTO CALIBRATION (16 DIEM)...\n", axisId + 1);

    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    isSynchronizedMove = false;
    motor->stop();
    calibData.isCalibrated = false;
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_task_wdt_reset();

    const uint32_t totalStepsOneRev = (uint32_t)(fullStepsPerRev * currentMicrosteps * fabsf(gearRatio) + 0.5f);
    const uint32_t stepsPerPoint = totalStepsOneRev / CALIB_POINTS;

    motor->setSpeed(baseIntervalUs);
    bool testDir = true;
    if (dirInvert) testDir = !testDir;

    for (int i = 0; i < CALIB_POINTS; i++) {
        if (i > 0) {
            motor->run(testDir, stepsPerPoint);
            while (motor->isRunning()) {
                vTaskDelay(pdMS_TO_TICKS(1));
                esp_task_wdt_reset();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
        esp_task_wdt_reset();

        float sum = 0.0f;
        for (int s = 0; s < 8; s++) {
            sum += sensor->getAngle(axisId);
            vTaskDelay(pdMS_TO_TICKS(10));
            esp_task_wdt_reset();
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
        vTaskDelay(pdMS_TO_TICKS(1));
        esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}

bool MotionController::seekEndstopSmooth(bool dir, uint32_t maxSteps, float &hitAngle, bool isDebug) {
    motor->setCurrent(homingCurrentMa);

    // Đảm bảo chiều di chuyển vật lý tuân thủ đúng dirInvert
    bool physicalDir = dir;
    if (dirInvert) physicalDir = !physicalDir;

    // Vận tốc bò chậm mô-men xoắn cao ổn định (1600us = ~625 steps/sec)
    motor->setSpeed(HOMING_STEP_INTERVAL_US);
    motor->run(physicalDir, maxSteps);

    bool stallDetected = false;
    unsigned long startTime = millis();
    unsigned long lastCheckMs = millis();

    float lastAngle = getCorrectedAngle();
    uint8_t stationaryStreak = 0;

    // Tự động tính ngưỡng dừng cữ theo tỉ số truyền của từng khớp:
    // Ở 1600us/step, mỗi 100ms phát ra ~62.5 bước.
    // Góc di chuyển lý thuyết của khớp trong 100ms:
    float expectedDegIn100ms = (stepsPerDegree > 0.1f) ? (62.5f / stepsPerDegree) : 1.0f;
    float stallThresholdDeg = expectedDegIn100ms * 0.35f;
    if (stallThresholdDeg > 0.08f) stallThresholdDeg = 0.08f;
    if (stallThresholdDeg < 0.02f) stallThresholdDeg = 0.02f;

    while (motor->isRunning()) {
        unsigned long now = millis();

        // Chờ 450ms cho động cơ vào dải vận tốc hành trình ổn định
        if (now - startTime > 450) {
            if (now - lastCheckMs >= 100) {
                lastCheckMs = now;
                float currentDeg = getCorrectedAngle();
                float deltaAngle = fabsf(getShortestAngleError(currentDeg, lastAngle));

                // Khi chạy tự do, deltaAngle >> stallThresholdDeg.
                // Khi chạm cữ cứng, rotor bị giữ chặt -> deltaAngle < stallThresholdDeg.
                if (deltaAngle < stallThresholdDeg) {
                    stationaryStreak++;
                    // Yêu cầu 3 chu kỳ liên tiếp (300ms đứng yên thực tế) để xác nhận chạm cữ
                    if (stationaryStreak >= 3) {
                        stallDetected = true;
                        hitAngle = currentDeg;
                        motor->stop();
                        if (isDebug) {
                            Serial.printf(">> [STALL J%u] Chạm cữ vật lý xác nhận tại góc %.2f° (Dir: %s, Thresh: %.3f°)\n",
                                          axisId + 1, hitAngle, dir ? "CW" : "CCW", stallThresholdDeg);
                        }
                        break;
                    }
                } else {
                    stationaryStreak = 0;
                }
                lastAngle = currentDeg;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        esp_task_wdt_reset();
    }

    if (!stallDetected) {
        hitAngle = getCorrectedAngle();
    }

    motor->setCurrent(normalCurrentMa);
    motor->setSpeed(baseIntervalUs);
    return stallDetected;
}

void MotionController::runCenterHoming(bool isDebug) {
    Serial.printf("\n[HOMING J%u] BAT DAU HOMING DÒ CỮ AN TOÀN...\n", axisId + 1);

    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    isSynchronizedMove = false;
    motor->stop();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_task_wdt_reset();

    const uint32_t MAX_STEPS = (uint32_t)(fullStepsPerRev * currentMicrosteps * fabsf(gearRatio) * 2.0f);
    const uint32_t backoffSteps = (uint32_t)(5.0f * stepsPerDegree);

    // BƯỚC 1: Tìm cữ chặn bên Trái (CCW / Min)
    float angleMin = 0.0f;
    bool foundMin = seekEndstopSmooth(false, MAX_STEPS, angleMin, true);

    if (!foundMin) {
        Serial.printf("[LOI HOMING J%u] Khong tim thay cu chan Trai (CCW)!\n", axisId + 1);
        motor->setCurrent(normalCurrentMa);
        return;
    }

    Serial.printf(">> [HOMING J%u] Da cham cu Trai: %.2f deg. Dang lui 5 deg...\n", axisId + 1, angleMin);

    // Lùi 5 độ an toàn về phía CW (true) bằng vận tốc êm HOMING_STEP_INTERVAL_US
    moveRawSteps(true, backoffSteps, HOMING_STEP_INTERVAL_US);
    while (motor->isRunning()) {
        vTaskDelay(pdMS_TO_TICKS(1));
        esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_task_wdt_reset();

    // BƯỚC 2: Tìm cữ chặn bên Phải (CW / Max)
    float angleMax = 0.0f;
    bool foundMax = seekEndstopSmooth(true, MAX_STEPS, angleMax, true);

    if (foundMax) {
        // TRƯỜNG HỢP 1: CÓ ĐỦ 2 CỮ (TRÁI & PHẢI) -> CĂN GIỮA TRUNG ĐIỂM
        Serial.printf(">> [HOMING J%u] Da cham cu Phai: %.2f deg. Dang can giua...\n", axisId + 1, angleMax);

        // Lùi 5 độ về phía CCW (false)
        moveRawSteps(false, backoffSteps, HOMING_STEP_INTERVAL_US);
        while (motor->isRunning()) {
            vTaskDelay(pdMS_TO_TICKS(1));
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_task_wdt_reset();

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
        motor->setSpeed(HOMING_STEP_INTERVAL_US);

        moveRawSteps(false, stepsToCenter, HOMING_STEP_INTERVAL_US);
        while (motor->isRunning()) {
            vTaskDelay(pdMS_TO_TICKS(1));
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_task_wdt_reset();

        zeroOffsetAngle = centerAbsoluteAngle;
        isHomed = true;
        targetAngle = 0.0f;
        currentAngle = 0.0f;
        limitLeftDeg = -halfStrokeDeg;
        limitRightDeg = +halfStrokeDeg;
        saveSettings();

        Serial.printf("[HOMING J%u] HOAN TAT! Set Home 0.00 deg (Cung hanh trinh: %.2f deg)\n", axisId + 1, totalStrokeDeg);
    } else {
        // TRƯỜNG HỢP 2: CHỈ CÓ 1 CỮ CHẶN (MIN) -> ĐẶT 0.00° TẠI VỊ TRÍ LÙI CỮ MIN
        Serial.printf(">> [HOMING J%u] Khong tim thay cu Phai -> Thiet lap Home theo cu Min.\n", axisId + 1);

        float currentAbs = getCorrectedAngle();
        zeroOffsetAngle = currentAbs;
        isHomed = true;
        targetAngle = 0.0f;
        currentAngle = 0.0f;
        limitLeftDeg = -5.0f;
        limitRightDeg = 180.0f;
        totalStrokeDeg = 185.0f;
        saveSettings();

        Serial.printf("[HOMING J%u] HOAN TAT! Set Home 0.00 deg tai vi tri hien tai.\n", axisId + 1);
    }

    motor->setCurrent(normalCurrentMa);
    motor->setSpeed(baseIntervalUs);
}

void MotionController::setTargetAngle(float target) {
    if (isHomed && totalStrokeDeg > 0.0f) {
        float maxLimit = limitRightDeg - 0.5f;
        float minLimit = limitLeftDeg + 0.5f;
        if (target > maxLimit) target = maxLimit;
        if (target < minLimit) target = minLimit;
    }

    targetAngle = target;
    isSynchronizedMove = false;
    positioningActive = true;
    reachedTarget = false;
    inDeadband = false;
    runawayDetected = false;
    errorIncreasingStreak = 0;
    prevCycleError = fabs(getShortestAngleError(targetAngle, getHomeRelativeAngle()));
    lastTrendCheckMs = millis();
}

void MotionController::setTargetAngleSync(float target, uint32_t intervalUs) {
    if (isHomed && totalStrokeDeg > 0.0f) {
        float maxLimit = limitRightDeg - 0.5f;
        float minLimit = limitLeftDeg + 0.5f;
        if (target > maxLimit) target = maxLimit;
        if (target < minLimit) target = minLimit;
    }

    targetAngle = target;
    syncIntervalUs = intervalUs;
    isSynchronizedMove = true;
    positioningActive = true;
    reachedTarget = false;
    inDeadband = false;
    runawayDetected = false;
    errorIncreasingStreak = 0;
    prevCycleError = fabs(getShortestAngleError(targetAngle, getHomeRelativeAngle()));
    lastTrendCheckMs = millis();
}

void MotionController::jog(float delta) {
    setTargetAngle(getHomeRelativeAngle() + delta);
}

void MotionController::stop() {
    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    isSynchronizedMove = false;
    motor->stop();
}

void MotionController::moveRawSteps(bool cw, uint32_t steps, uint32_t speedUs) {
    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    isSynchronizedMove = false;
    if (speedUs >= MIN_STEP_INTERVAL_US && speedUs <= MAX_STEP_INTERVAL_US) {
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
    isSynchronizedMove = false;
    if (speedUs >= MIN_STEP_INTERVAL_US && speedUs <= MAX_STEP_INTERVAL_US) {
        motor->setSpeed(speedUs);
    } else {
        motor->setSpeed(baseIntervalUs);
    }
    bool dir = cw;
    if (dirInvert) dir = !dir;
    motor->runContinuous(dir);
}

void MotionController::setDriverEnabled(bool enabled) {
    if (!enabled) {
        stop();
    }
    motor->enable(enabled);
}

void MotionController::setSpeed(uint32_t speedUs) {
    if (speedUs >= MIN_STEP_INTERVAL_US && speedUs <= MAX_STEP_INTERVAL_US) {
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
    if (fabsf(ratio) >= 0.05f && fabsf(ratio) <= 200.0f) {
        gearRatio = ratio;
        updateStepsPerDegree();
        if (ratio < 0.0f) dirInvert = true;
        saveSettings();
    }
}

void MotionController::executePendingTask() {
    ControllerTask task = pendingTask;
    pendingTask = TASK_NONE;

    if (task == TASK_HOME) {
        runCenterHoming(true);
    } else if (task == TASK_CALIB) {
        runAutoCalibration();
    } else if (task == TASK_ZERO) {
        setHomeHere();
    } else if (task == TASK_AUTODIR) {
        detectAndAutoSetDirection(true);
    }
}

void MotionController::update() {
    // 1. Sinh xung bước non-blocking
    motor->update();

    // 2. Vòng điều khiển vị trí bám góc
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

    // Runaway Protection: Kiểm tra đạo hàm xu hướng sai số theo thời gian (Sliding Derivative Trend)
    uint32_t now = millis();
    if (motor->isRunning() && (now - lastTrendCheckMs >= 50)) {
        lastTrendCheckMs = now;
        if (absErr > (prevCycleError + 0.25f)) {
            errorIncreasingStreak++;
            if (errorIncreasingStreak >= 4) { // Sai số tăng liên tục trong >200ms
                motor->stop();
                positioningActive = false;
                closedLoopHold = false;
                isSynchronizedMove = false;
                runawayDetected = true;
                Serial.printf(">> [CANH BAO NGUOC CHIEU J%u] Sai so lien tuc tang tu %.2f do -> %.2f do! Da dung an toan. Vui long bat Invert!\n",
                              axisId + 1, prevCycleError, absErr);
                return;
            }
        } else if (absErr < prevCycleError - 0.1f) {
            errorIncreasingStreak = 0;
        }
        prevCycleError = absErr;
    }

    // Schmitt-Trigger Deadband Logic
    if (inDeadband) {
        if (absErr > deadbandExit) {
            inDeadband = false;
            reachedTarget = false;
            prevCycleError = absErr;
            errorIncreasingStreak = 0;
            lastTrendCheckMs = millis();
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
        isSynchronizedMove = false;

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

        // Nếu đang trong chu kỳ quay đồng bộ đa trục, KHÔNG làm chậm cục bộ để giữ đúng tỷ lệ T_sync
        if (isSynchronizedMove) {
            interval = syncIntervalUs;
        } else {
            // Adaptive speed scaling cho lệnh đơn trục thông thường
            if (absErr < 2.0f) {
                interval = baseIntervalUs * 2;
            } else if (absErr < 8.0f) {
                interval = (uint32_t)(baseIntervalUs * 1.4f);
            }
        }

        motor->setSpeed(interval);
        prevCycleError = absErr;
        errorIncreasingStreak = 0;
        lastTrendCheckMs = millis();
        motor->run(dir, neededSteps);
    }
}
