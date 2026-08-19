#include "multi_axis_manager.h"

MultiAxisManager::MultiAxisManager(Motor* mList[NUM_MOTORS], MotionController* cList[NUM_MOTORS], Sensor* s)
    : sensor(s), ikHook(nullptr), taskHandle(nullptr), taskRunning(false) {
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

    while (taskRunning) {
        if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            for (uint8_t i = 0; i < NUM_MOTORS; i++) {
                if (controllers[i] != nullptr) {
                    controllers[i]->update();
                }
            }
            xSemaphoreGive(stateMutex);
        }

        vTaskDelayUntil(&lastWake, period);
    }

    vTaskDelete(nullptr);
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

    // Feasibility Clamping: Cannot move faster than physical max velocity allows
    float actualDuration = (moveTimeSec > maxRequiredTime) ? moveTimeSec : maxRequiredTime;

    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        if (controllers[i] == nullptr || motors[i] == nullptr) continue;

        if (syncArrival && deltaAngles[i] > 0.05f) {
            float jointVelDegPerSec = deltaAngles[i] / actualDuration;
            float stepsPerDeg = (DEFAULT_FULL_STEPS * DEFAULT_MICROSTEPS * controllers[i]->getGearRatio()) / 360.0f;
            float stepsPerSec = jointVelDegPerSec * stepsPerDeg;

            uint32_t intervalUs = (stepsPerSec > 1.0f) ? (uint32_t)(1000000.0f / stepsPerSec) : DEFAULT_STEP_INTERVAL_US;
            if (intervalUs < 150) intervalUs = 150;
            if (intervalUs > 2000) intervalUs = 2000;

            motors[i]->setSpeed(intervalUs);
        }

        controllers[i]->setTargetAngle(targets[i]);
    }

    xSemaphoreGive(stateMutex);
}

bool MultiAxisManager::setCartesianPose(const CartesianPose& pose, const IKSolverParams& params, float moveTimeSec) {
    if (ikHook == nullptr) {
        Serial.println("[IK] Kinematics solver hook not registered!");
        return false;
    }

    float currentJoints[NUM_MOTORS];
    getAllAngles(currentJoints);

    float resolvedJoints[NUM_MOTORS];
    bool success = ikHook->solveIK(pose, currentJoints, resolvedJoints, params);

    if (success) {
        setTargetAnglesSync(resolvedJoints, moveTimeSec, true);
        return true;
    }

    Serial.println("[IK] IK Solver failed to reach target Cartesian pose within limits!");
    return false;
}

void MultiAxisManager::emergencyStopAll() {
    if (stateMutex != nullptr && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (uint8_t i = 0; i < NUM_MOTORS; i++) {
            if (controllers[i] != nullptr) {
                controllers[i]->stop();
            }
        }
        xSemaphoreGive(stateMutex);
    } else {
        // Direct stop fallback if mutex is blocked
        for (uint8_t i = 0; i < NUM_MOTORS; i++) {
            if (motors[i] != nullptr) {
                motors[i]->stop();
            }
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
