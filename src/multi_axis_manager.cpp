#include "multi_axis_manager.h"
#include <esp_task_wdt.h>

MultiAxisManager::MultiAxisManager(Motor* mList[NUM_MOTORS], MotionController* cList[NUM_MOTORS], Sensor* s)
    : sensor(s), ikHook(&defaultKinematics), waypointCount(0), currentWaypointIdx(-1),
      sequenceRunning(false), sequenceLoop(false), waypointDwellStartMs(0), waitingDwell(false),
      taskHandle(nullptr), taskRunning(false) {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        motors[i] = mList[i];
        controllers[i] = cList[i];
        maxVelocityDegPerSec[i] = 180.0f; // 180 deg/sec default max speed
    }
    stateMutex = xSemaphoreCreateMutex();
}

MultiAxisManager::~MultiAxisManager() {
    if (taskHandle != nullptr) {
        taskRunning = false;
        vTaskDelete(taskHandle);
    }
    if (stateMutex != nullptr) {
        vSemaphoreDelete(stateMutex);
    }
}

void MultiAxisManager::taskEntry(void* param) {
    MultiAxisManager* self = static_cast<MultiAxisManager*>(param);
    self->taskLoop();
}

void MultiAxisManager::taskLoop() {
    const TickType_t period = pdMS_TO_TICKS(
        (MOTION_TASK_PERIOD_MS > 0) ? MOTION_TASK_PERIOD_MS : 10
    );
    TickType_t lastWake = xTaskGetTickCount();

    // Đăng ký Task Watchdog Timer
    esp_task_wdt_add(nullptr);

    while (taskRunning) {
        esp_task_wdt_reset();

        if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            for (uint8_t i = 0; i < NUM_MOTORS; i++) {
                if (controllers[i] != nullptr) {
                    controllers[i]->update();
                }
            }

            // Xử lý quỹ đạo Waypoint tự động
            if (sequenceRunning) {
                processWaypointSequence();
            }

            xSemaphoreGive(stateMutex);
        }

        vTaskDelayUntil(&lastWake, period);
    }

    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
}

void MultiAxisManager::processWaypointSequence() {
    if (waypointCount == 0 || currentWaypointIdx < 0 || currentWaypointIdx >= waypointCount) {
        sequenceRunning = false;
        return;
    }

    // Kiểm tra xem tất cả các trục đã dừng và đạt góc đích chưa
    bool anyMoving = false;
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (motors[i] != nullptr && motors[i]->isRunning()) {
            anyMoving = true;
            break;
        }
    }

    if (!anyMoving) {
        if (!waitingDwell) {
            waypointDwellStartMs = millis();
            waitingDwell = true;
        } else {
            uint32_t elapsed = millis() - waypointDwellStartMs;
            if (elapsed >= waypoints[currentWaypointIdx].dwellTimeMs) {
                waitingDwell = false;
                currentWaypointIdx++;

                if (currentWaypointIdx >= waypointCount) {
                    if (sequenceLoop) {
                        currentWaypointIdx = 0;
                    } else {
                        sequenceRunning = false;
                        currentWaypointIdx = -1;
                        Serial.println("[WAYPOINT] Hoan tat chuoi quy dao!");
                        return;
                    }
                }

                // Thực thi chuyển động đến Waypoint tiếp theo
                setTargetAnglesSync(waypoints[currentWaypointIdx].joints,
                                    waypoints[currentWaypointIdx].moveTimeSec,
                                    true);
                Serial.printf("[WAYPOINT] Chuyen sang diem %d/%d: %s\n",
                              currentWaypointIdx + 1, waypointCount, waypoints[currentWaypointIdx].name);
            }
        }
    }
}

void MultiAxisManager::begin(uint8_t coreID, uint8_t priority, uint32_t period_ms) {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (controllers[i] != nullptr) {
            controllers[i]->begin();
        }
    }

    taskRunning = true;
    xTaskCreatePinnedToCore(
        taskEntry,
        "MotionControlTask",
        MOTION_TASK_STACK_SIZE,
        this,
        priority,
        &taskHandle,
        coreID
    );
}

void MultiAxisManager::setJointTarget(uint8_t axis, float angle) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        controllers[axis]->setTargetAngle(angle);
        xSemaphoreGive(stateMutex);
    }
}

void MultiAxisManager::jogJoint(uint8_t axis, float delta) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        controllers[axis]->jog(delta);
        xSemaphoreGive(stateMutex);
    }
}

void MultiAxisManager::moveJointRawSteps(uint8_t axis, bool cw, uint32_t steps, uint32_t speedUs) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        controllers[axis]->moveRawSteps(cw, steps, speedUs);
        xSemaphoreGive(stateMutex);
    }
}

void MultiAxisManager::runJointContinuous(uint8_t axis, bool cw, uint32_t speedUs) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        controllers[axis]->runContinuous(cw, speedUs);
        xSemaphoreGive(stateMutex);
    }
}

void MultiAxisManager::setJointDriverEnabled(uint8_t axis, bool enabled) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        controllers[axis]->setDriverEnabled(enabled);
        xSemaphoreGive(stateMutex);
    }
}

void MultiAxisManager::triggerJointHome(uint8_t axis) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    controllers[axis]->triggerHome();
}

void MultiAxisManager::triggerJointZero(uint8_t axis) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    controllers[axis]->triggerZero();
}

void MultiAxisManager::triggerJointCalib(uint8_t axis) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    controllers[axis]->triggerCalib();
}

void MultiAxisManager::stopJoint(uint8_t axis) {
    if (axis >= NUM_MOTORS || controllers[axis] == nullptr) return;
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        controllers[axis]->stop();
        xSemaphoreGive(stateMutex);
    }
}

void MultiAxisManager::setTargetAnglesSync(const float targets[NUM_MOTORS], float moveTimeSec, bool syncArrival) {
    if (stateMutex == nullptr || xSemaphoreTake(stateMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;

    float deltaAngles[NUM_MOTORS];
    float minTimes[NUM_MOTORS];
    float maxRequiredTime = 0.05f;

    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (controllers[i] == nullptr) continue;
        float cur = controllers[i]->getCurrentAngle();
        float diff = fabs(targets[i] - cur);
        while (diff > 180.0f) diff = fabs(360.0f - diff);
        deltaAngles[i] = diff;

        float maxV = (maxVelocityDegPerSec[i] > 1.0f) ? maxVelocityDegPerSec[i] : 180.0f;
        minTimes[i] = deltaAngles[i] / maxV;
        if (minTimes[i] > maxRequiredTime) {
            maxRequiredTime = minTimes[i];
        }
    }

    // Feasibility Clamping: Không thể chạy nhanh hơn giới hạn vận tốc phần cứng
    float actualDuration = (moveTimeSec > maxRequiredTime) ? moveTimeSec : maxRequiredTime;

    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (controllers[i] == nullptr || motors[i] == nullptr) continue;

        if (syncArrival && deltaAngles[i] > 0.05f) {
            float jointVelDegPerSec = deltaAngles[i] / actualDuration;
            float stepsPerDeg = (DEFAULT_FULL_STEPS * DEFAULT_MICROSTEPS * controllers[i]->getGearRatio()) / 360.0f;
            float stepsPerSec = jointVelDegPerSec * stepsPerDeg;

            uint32_t intervalUs = (stepsPerSec > 1.0f) ? (uint32_t)(1000000.0f / stepsPerSec) : DEFAULT_STEP_INTERVAL_US;
            if (intervalUs < MIN_STEP_INTERVAL_US) intervalUs = MIN_STEP_INTERVAL_US;
            if (intervalUs > MAX_STEP_INTERVAL_US) intervalUs = MAX_STEP_INTERVAL_US;

            // Khóa vận tốc đồng bộ cho Controller
            controllers[i]->setTargetAngleSync(targets[i], intervalUs);
        } else {
            controllers[i]->setTargetAngle(targets[i]);
        }
    }

    xSemaphoreGive(stateMutex);
}

bool MultiAxisManager::setCartesianPose(const CartesianPose& pose, const IKSolverParams& params, float moveTimeSec) {
    if (ikHook == nullptr) {
        ikHook = &defaultKinematics;
    }

    float currentJoints[NUM_MOTORS];
    getAllAngles(currentJoints);

    float resolvedJoints[NUM_MOTORS];
    bool success = ikHook->solveIK(pose, currentJoints, resolvedJoints, params);

    if (success) {
        setTargetAnglesSync(resolvedJoints, moveTimeSec, true);
        return true;
    }

    Serial.println("[IK] IK Solver that bai khi tinh toan goc khop cho toa do yeu cau!");
    return false;
}

bool MultiAxisManager::moveCartesianLinear(const CartesianPose& targetPose, float speedMmPerSec) {
    if (speedMmPerSec <= 1.0f) speedMmPerSec = 50.0f;

    CartesianPose startPose = {0};
    if (!getCartesianPose(startPose)) return false;

    float dx = targetPose.x - startPose.x;
    float dy = targetPose.y - startPose.y;
    float dz = targetPose.z - startPose.z;
    float distMm = sqrtf(dx * dx + dy * dy + dz * dz);

    float durationSec = (distMm > 0.1f) ? (distMm / speedMmPerSec) : 0.5f;
    if (durationSec < 0.2f) durationSec = 0.2f;

    return setCartesianPose(targetPose, {100, 0.1f, 0.01f}, durationSec);
}

bool MultiAxisManager::getCartesianPose(CartesianPose& outPose) {
    if (ikHook == nullptr) {
        ikHook = &defaultKinematics;
    }

    float currentJoints[NUM_MOTORS];
    getAllAngles(currentJoints);
    return ikHook->solveFK(currentJoints, outPose);
}

void MultiAxisManager::emergencyStopAll() {
    sequenceRunning = false;
    currentWaypointIdx = -1;

    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (uint8_t i = 0; i < NUM_MOTORS; i++) {
            if (controllers[i] != nullptr) {
                controllers[i]->stop();
            }
        }
        xSemaphoreGive(stateMutex);
    } else {
        // Fallback trực tiếp nếu mutex bị khóa
        for (uint8_t i = 0; i < NUM_MOTORS; i++) {
            if (motors[i] != nullptr) {
                motors[i]->stop();
            }
        }
    }
}

void MultiAxisManager::triggerAllHome() {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (controllers[i] != nullptr) {
            controllers[i]->triggerHome();
        }
    }
}

void MultiAxisManager::triggerAllZero() {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (controllers[i] != nullptr) {
            controllers[i]->triggerZero();
        }
    }
}

void MultiAxisManager::setAllDriversEnabled(bool enabled) {
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        for (uint8_t i = 0; i < NUM_MOTORS; i++) {
            if (controllers[i] != nullptr) {
                controllers[i]->setDriverEnabled(enabled);
            }
        }
        xSemaphoreGive(stateMutex);
    }
}

bool MultiAxisManager::addWaypoint(const char* name, const float joints[NUM_MOTORS], float moveTimeSec, uint16_t dwellMs) {
    if (waypointCount >= MAX_WAYPOINTS) return false;

    strncpy(waypoints[waypointCount].name, name, sizeof(waypoints[waypointCount].name) - 1);
    waypoints[waypointCount].name[sizeof(waypoints[waypointCount].name) - 1] = '\0';

    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        waypoints[waypointCount].joints[i] = joints[i];
    }
    waypoints[waypointCount].moveTimeSec = (moveTimeSec > 0.1f) ? moveTimeSec : 2.0f;
    waypoints[waypointCount].dwellTimeMs = dwellMs;

    if (ikHook != nullptr) {
        ikHook->solveFK(joints, waypoints[waypointCount].pose);
    }

    waypointCount++;
    return true;
}

void MultiAxisManager::clearWaypoints() {
    sequenceRunning = false;
    currentWaypointIdx = -1;
    waypointCount = 0;
}

void MultiAxisManager::startSequence(bool loop) {
    if (waypointCount == 0) return;
    sequenceLoop = loop;
    currentWaypointIdx = 0;
    waitingDwell = false;
    sequenceRunning = true;

    setTargetAnglesSync(waypoints[0].joints, waypoints[0].moveTimeSec, true);
    Serial.printf("[WAYPOINT] Bat dau chuoi quy dao (%d diem, loop=%s)\n",
                  waypointCount, loop ? "true" : "false");
}

void MultiAxisManager::pauseSequence() {
    sequenceRunning = false;
}

void MultiAxisManager::stopSequence() {
    sequenceRunning = false;
    currentWaypointIdx = -1;
    emergencyStopAll();
}

bool MultiAxisManager::isAnyRunning() {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (motors[i] != nullptr && motors[i]->isRunning()) return true;
    }
    return false;
}

bool MultiAxisManager::areAllHomed() {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (controllers[i] != nullptr && !controllers[i]->getIsHomed()) return false;
    }
    return true;
}

bool MultiAxisManager::areAllCalibrated() {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (controllers[i] != nullptr && !controllers[i]->getIsCalibrated()) return false;
    }
    return true;
}

void MultiAxisManager::getAllAngles(float anglesOut[NUM_MOTORS]) {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        anglesOut[i] = (controllers[i] != nullptr) ? controllers[i]->getCurrentAngle() : 0.0f;
    }
}

void MultiAxisManager::getAllTargets(float targetsOut[NUM_MOTORS]) {
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        targetsOut[i] = (controllers[i] != nullptr) ? controllers[i]->getTargetAngle() : 0.0f;
    }
}
