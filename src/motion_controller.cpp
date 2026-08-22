#include "motion_controller.h"
#include "web_server_manager.h"
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
      normalCurrentMa(DEFAULT_NORMAL_CURRENT),
      homingCurrentMa((axisIndex < NUM_MOTORS) ? DEFAULT_AXIS_HOMING_CURRENTS[axisIndex] : DEFAULT_HOMING_CURRENT),
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
    uint16_t defHomingCurr = (axisId < NUM_MOTORS) ? DEFAULT_AXIS_HOMING_CURRENTS[axisId] : DEFAULT_HOMING_CURRENT;
    if (prefs.isKey("h_curr")) {
        homingCurrentMa = prefs.getUShort("h_curr", defHomingCurr);
    } else {
        homingCurrentMa = defHomingCurr;
    }
    // Cập nhật lên mức mặc định mới cho J2/J3 nếu đang bị kẹt ở mức quá thấp cũ (< 900mA)
    if (homingCurrentMa < defHomingCurr) {
        homingCurrentMa = defHomingCurr;
    }
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

bool MotionController::detectAndAutoSetDirection(bool returnToStart, bool useHomingCurrent) {
    sysLogf("\n[AUTO-DIR J%u] Kiem tra chieu quay dong co vs cam bien AS5600...\n", axisId + 1);

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

    // Dung dong Homing (an toan hon, thap hon) neu goi tu trong quy trinh Homing,
    // nguoc lai dung dong chay binh thuong cho lenh AUTODIR thu cong doc lap
    motor->setCurrent(useHomingCurrent ? homingCurrentMa : normalCurrentMa);
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
        // (SUA: truoc day chi chay lai testSteps, thieu mot nua quang duong da di chuyen)
        if (returnToStart) {
            motor->run(true, testSteps * 2);
            while (motor->isRunning()) {
                vTaskDelay(pdMS_TO_TICKS(1));
                esp_task_wdt_reset();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    if (determined) {
        saveSettings();
        sysLogf(">> [AUTO-DIR J%u] HOAN TAT! Delta = %+.2f deg ==> Tu dong cai dirInvert = %s (Da luu vao Flash NVS)!\n",
                axisId + 1, (fabsf(deltaCW) >= 0.15f ? deltaCW : 0.0f),
                dirInvert ? "TRUE (DAO CHIEU)" : "FALSE (CHIEU THUAN)");
    } else {
        sysLogf(">> [AUTO-DIR J%u] CANH BAO: Khong do duoc bien thien goc ro rang (Delta < 0.15 deg).\n", axisId + 1);
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

bool MotionController::seekEndstopSmooth(bool dir, uint32_t maxSteps, float &hitAngle, float &netTravel, bool isDebug) {
    motor->setCurrent(homingCurrentMa);

    // Đảm bảo chiều di chuyển vật lý tuân thủ đúng dirInvert
    bool physicalDir = dir;
    if (dirInvert) physicalDir = !physicalDir;

    uint32_t homingSpeedUs = (axisId < NUM_MOTORS) ? DEFAULT_AXIS_HOMING_SPEEDS[axisId] : HOMING_STEP_INTERVAL_US;

    // Vận tốc bò chậm mô-men xoắn cao ổn định (2500us cho J2/J3 để sinh lực nâng cực đại)
    motor->setSpeed(homingSpeedUs);
    motor->run(physicalDir, maxSteps);

    bool stallDetected = false;
    unsigned long startTime = millis();
    unsigned long lastLogMs = millis();

    // ===== THUẬT TOÁN CỬA SỔ TRƯỢT 1.25 GIÂY (DIRECTION-AGNOSTIC SLIDING WINDOW) =====
    const int HISTORY_SAMPLES = 25; // 25 mẫu × 50ms = 1250ms (1.25 giây)
    float unrolledHistory[HISTORY_SAMPLES];
    int histIdx = 0;
    bool histFull = false;

    float startRawAngle = getCorrectedAngle();
    float lastRawAngle = startRawAngle;
    float unrolledAngle = 0.0f;
    float peakMovedAngle = startRawAngle;
    float maxTravelSoFar = 0.0f;

    const float MIN_TRAVEL_IN_1200MS = 0.35f; // Cần di chuyển ít nhất 0.35° trong 1.25 giây

    while (motor->isRunning()) {
        unsigned long now = millis();

        // 1. Giới hạn thời gian tuyệt đối bảo vệ an toàn
        if (now - startTime > HOMING_MAX_DURATION_MS) {
            motor->stop();
            if (isDebug) {
                sysLogf(">> [TIMEOUT J%u] Homing vuot qua %u ms! Dung an toan.\n",
                        axisId + 1, (unsigned)HOMING_MAX_DURATION_MS);
            }
            break;
        }

        // 2. Đọc góc cảm biến và cập nhật unrolled angle liên tục (không bị giới hạn 0-360°)
        float currentRaw = getCorrectedAngle();
        float delta = getShortestAngleError(currentRaw, lastRawAngle);
        unrolledAngle += delta;
        lastRawAngle = currentRaw;

        // Theo dõi góc đạt độ dịch chuyển xa nhất
        if (fabsf(unrolledAngle) > maxTravelSoFar) {
            maxTravelSoFar = fabsf(unrolledAngle);
            peakMovedAngle = currentRaw;
        }

        // 3. Đọc mẫu cũ nhất từ 1.25s trước (trước khi ghi đè)
        float oldestUnrolled = unrolledHistory[histIdx % HISTORY_SAMPLES];

        // Lưu mẫu hiện tại vào bộ đệm vòng
        unrolledHistory[histIdx % HISTORY_SAMPLES] = unrolledAngle;
        histIdx++;
        if (histIdx >= HISTORY_SAMPLES) histFull = true;

        // 4. Kiểm tra độ dịch chuyển trong 1.25s sau khi bộ đệm đã đầy (sau 1.25s đầu tiên)
        if (histFull && (now - startTime > 1200)) {
            float distMoved1s = fabsf(unrolledAngle - oldestUnrolled);

            // Log debug định kỳ mỗi 300ms
            if (isDebug && (now - lastLogMs >= 300)) {
                lastLogMs = now;
                sysLogf("[J%u MOVE] raw=%.2f unrolled=%+.2f dist1.2s=%.2f° (thresh=%.2f°)\n",
                        axisId + 1, currentRaw, unrolledAngle, distMoved1s, MIN_TRAVEL_IN_1200MS);
            }

            if (distMoved1s < MIN_TRAVEL_IN_1200MS) {
                stallDetected = true;
                hitAngle = peakMovedAngle; // Lấy góc tại điểm chạm cữ xa nhất
                motor->stop();
                if (isDebug) {
                    sysLogf(">> [STALL J%u] CHAM CU CO KHI XAC NHAN tai goc %.2f° (dist1.2s=%.2f° < %.2f°, totalTravel=%+.2f°)\n",
                            axisId + 1, hitAngle, distMoved1s, MIN_TRAVEL_IN_1200MS, unrolledAngle);
                }
                break;
            }
        } else {
            // Đang trong giai đoạn khởi động tích lũy dữ liệu 1.2s
            if (isDebug && (now - lastLogMs >= 300)) {
                lastLogMs = now;
                sysLogf("[J%u WARMUP] raw=%.2f unrolled=%+.2f collecting %d/%d\n",
                        axisId + 1, currentRaw, unrolledAngle, histIdx, HISTORY_SAMPLES);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
        esp_task_wdt_reset();
    }

    if (!stallDetected) {
        hitAngle = getCorrectedAngle();
    }

    netTravel = unrolledAngle;
    motor->setSpeed(baseIntervalUs);
    return stallDetected;
}

void MotionController::runCenterHoming(bool isDebug) {
    sysLogf("\n[HOMING J%u] BAT DAU HOMING DÒ CỮ AN TOÀN...\n", axisId + 1);

    uint32_t homingSpeedUs = (axisId < NUM_MOTORS) ? DEFAULT_AXIS_HOMING_SPEEDS[axisId] : HOMING_STEP_INTERVAL_US;

    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    runawayDetected = false;
    isSynchronizedMove = false;
    motor->stop();
    motor->setCurrent(homingCurrentMa); // Đảm bảo dòng mô-men xoắn cao được giữ liên tục
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_task_wdt_reset();

    // ===== GIAI ĐOẠN 0.5: Dò chiều quay tự động trước khi tìm cữ =====
    sysLogf(">> [HOMING J%u] Giai doan xac dinh chieu quay truoc khi do cu...\n", axisId + 1);
    bool dirConfirmed = detectAndAutoSetDirection(true, true); // returnToStart=true, useHomingCurrent=true
    if (!dirConfirmed) {
        sysLogf(">> [HOMING J%u] CANH BAO: Khong xac dinh duoc chieu quay ro rang tu buoc do\n"
                "   (co the dang dung sat mot gioi han co san). Tiep tuc dong homing voi\n"
                "   cai dat Invert hien tai (%s).\n",
                axisId + 1, dirInvert ? "DAO CHIEU" : "THUAN");
    }

    // Đảm bảo dòng Homing duy trì sau bước auto-dir
    motor->setCurrent(homingCurrentMa);

    const uint32_t MAX_STEPS = (uint32_t)(fullStepsPerRev * currentMicrosteps * fabsf(gearRatio) * 3.0f);
    const float BACKOFF_DEG = 8.0f; // Lùi 8 độ đảm bảo tách rời hoàn toàn khỏi cữ cơ khí & quán tính
    const uint32_t backoffSteps = (uint32_t)(BACKOFF_DEG * stepsPerDegree);

    // BƯỚC 1: Tìm cữ chặn bên Trái (CCW / Min)
    float angleMin = 0.0f;
    float travel1 = 0.0f;
    bool foundMin = seekEndstopSmooth(false, MAX_STEPS, angleMin, travel1, true);

    if (!foundMin) {
        sysLogf("[LOI HOMING J%u] Khong tim thay cu chan Trai (CCW)!\n", axisId + 1);
        motor->setCurrent(normalCurrentMa);
        return;
    }

    sysLogf(">> [HOMING J%u] Da cham cu Trai: %.2f deg. Dang lui %.1f deg...\n", axisId + 1, angleMin, BACKOFF_DEG);

    // Lùi 8 độ an toàn về phía CW (true) với dòng homing khỏe
    motor->setCurrent(homingCurrentMa);
    moveRawSteps(true, backoffSteps, homingSpeedUs);
    while (motor->isRunning()) {
        vTaskDelay(pdMS_TO_TICKS(1));
        esp_task_wdt_reset();
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_task_wdt_reset();

    // BƯỚC 2: Tìm cữ chặn bên Phải (CW / Max)
    float angleMax = 0.0f;
    float travel2 = 0.0f;
    bool foundMax = seekEndstopSmooth(true, MAX_STEPS, angleMax, travel2, true);

    // Tính chính xác cung hành trình và góc trung điểm dựa trên chiều biến thiên thực tế trong Bước 2
    float strokeDeg = 0.0f;
    float centerAbsoluteAngle = 0.0f;

    if (foundMax) {
        if (travel2 >= 0.0f) {
            // Bước 2 quét theo chiều góc cảm biến TĂNG
            strokeDeg = angleMax - angleMin;
            if (strokeDeg < 0.0f) strokeDeg += 360.0f;
            centerAbsoluteAngle = normalizeAngle(angleMin + strokeDeg / 2.0f);
        } else {
            // Bước 2 quét theo chiều góc cảm biến GIẢM
            strokeDeg = angleMin - angleMax;
            if (strokeDeg < 0.0f) strokeDeg += 360.0f;
            centerAbsoluteAngle = normalizeAngle(angleMin - strokeDeg / 2.0f);
        }

        if (strokeDeg < 15.0f || strokeDeg > 350.0f) {
            sysLogf(">> [CANH BAO HOMING J%u] Cung hanh trinh do duoc (%.2f deg) khong hop ly (<15 deg hoac >350 deg)! Chuyen sang mode 1 cu an toan.\n",
                    axisId + 1, strokeDeg);
            foundMax = false;
        }
    }

    if (foundMax) {
        // TRƯỜNG HỢP 1: CÓ ĐỦ 2 CỮ (TRÁI & PHẢI) VÀ CUNG HỢP LÝ -> CĂN CHÍNH GIỮA TRUNG ĐIỂM
        sysLogf(">> [HOMING J%u] Da cham cu Phai: %.2f deg (Hanh trinh thuc: %.2f deg, Tam: %.2f deg). Dang can giua...\n",
                axisId + 1, angleMax, strokeDeg, centerAbsoluteAngle);

        // Lùi 8 độ về phía CCW (false) để tách khỏi cữ Phải
        motor->setCurrent(homingCurrentMa);
        moveRawSteps(false, backoffSteps, homingSpeedUs);
        while (motor->isRunning()) {
            vTaskDelay(pdMS_TO_TICKS(1));
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_task_wdt_reset();

        float halfStrokeDeg = strokeDeg / 2.0f;
        totalStrokeDeg = strokeDeg;

        // Từ vị trí lùi BACKOFF_DEG của cữ Phải, quãng đường cần chạy để về đúng tâm:
        float distFromRightToCenter = halfStrokeDeg - BACKOFF_DEG;
        if (distFromRightToCenter > 0.0f) {
            uint32_t stepsToCenter = (uint32_t)(distFromRightToCenter * stepsPerDegree + 0.5f);
            moveRawSteps(false, stepsToCenter, homingSpeedUs);
            while (motor->isRunning()) {
                vTaskDelay(pdMS_TO_TICKS(1));
                esp_task_wdt_reset();
            }
        } else if (distFromRightToCenter < 0.0f) {
            uint32_t stepsToCenter = (uint32_t)((-distFromRightToCenter) * stepsPerDegree + 0.5f);
            moveRawSteps(true, stepsToCenter, homingSpeedUs);
            while (motor->isRunning()) {
                vTaskDelay(pdMS_TO_TICKS(1));
                esp_task_wdt_reset();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_task_wdt_reset();

        zeroOffsetAngle = centerAbsoluteAngle;
        isHomed = true;
        targetAngle = 0.0f;
        currentAngle = 0.0f;
        limitLeftDeg = -halfStrokeDeg;
        limitRightDeg = +halfStrokeDeg;
        motor->setCurrent(normalCurrentMa);
        saveSettings();

        sysLogf("[HOMING J%u] HOAN TAT! Set Home 0.00 deg tai Tam (Hanh trinh: %.2f deg, Gioi han: [%.1f deg, +%.1f deg])\n",
                axisId + 1, totalStrokeDeg, limitLeftDeg, limitRightDeg);
    } else {
        // TRƯỜNG HỢP 2: CHỈ CÓ 1 CỮ CHẶN (MIN) HOẶC TIMEOUT -> QUAY VỀ LÙI 8° TỪ CỮ MIN
        sysLogf(">> [HOMING J%u] Khong xac dinh duoc cu Phai hop le -> Quay ve lui %.1f deg tu cu Trai de set Home an toan.\n", axisId + 1, BACKOFF_DEG);

        float currentAbs = getCorrectedAngle();
        float distToMin = fabsf(getShortestAngleError(currentAbs, angleMin));
        uint32_t stepsToMin = (uint32_t)(distToMin * stepsPerDegree + 0.5f);

        motor->setCurrent(homingCurrentMa);
        moveRawSteps(false, stepsToMin, homingSpeedUs);
        while (motor->isRunning()) {
            vTaskDelay(pdMS_TO_TICKS(1));
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(150));
        moveRawSteps(true, backoffSteps, homingSpeedUs);
        while (motor->isRunning()) {
            vTaskDelay(pdMS_TO_TICKS(1));
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(150));

        zeroOffsetAngle = getCorrectedAngle();
        isHomed = true;
        targetAngle = 0.0f;
        currentAngle = 0.0f;
        limitLeftDeg = -5.0f;
        limitRightDeg = 180.0f;
        totalStrokeDeg = 185.0f;
        saveSettings();

        sysLogf("[HOMING J%u] HOAN TAT! Set Home 0.00 deg tai vi tri lui cu Trai.\n", axisId + 1);
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

void MotionController::forceStopState() {
    positioningActive = false;
    closedLoopHold = false;
    inDeadband = false;
    isSynchronizedMove = false;
    if (motor != nullptr) {
        motor->stop();
    }
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
