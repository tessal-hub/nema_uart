#ifndef MULTI_AXIS_MANAGER_H
#define MULTI_AXIS_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "motor.h"
#include "sensor.h"
#include "motion_controller.h"

// ==============================================================================
// 1. KINEMATICS STRUCTURES & EXTENSION HOOK
// ==============================================================================
struct CartesianPose {
    float x;        // mm
    float y;        // mm
    float z;        // mm
    float roll;     // degrees
    float pitch;    // degrees
    float yaw;      // degrees
};

struct IKSolverParams {
    uint16_t maxIterations;
    float toleranceMm;
    float dampingFactor;
};

class KinematicsHook {
public:
    virtual ~KinematicsHook() {}
    virtual bool solveIK(const CartesianPose& targetPose,
                         const float currentJoints[NUM_MOTORS],
                         float outJoints[NUM_MOTORS],
                         const IKSolverParams& params) = 0;
    virtual bool solveFK(const float jointAngles[NUM_MOTORS],
                         CartesianPose& outPose) = 0;
};

// ==============================================================================
// 2. MULTI-AXIS MANAGER CLASS
// ==============================================================================
class MultiAxisManager {
private:
    Motor* motors[NUM_MOTORS];
    MotionController* controllers[NUM_MOTORS];
    Sensor* sensor;
    KinematicsHook* ikHook;

    float maxVelocityDegPerSec[NUM_MOTORS];

    SemaphoreHandle_t stateMutex;
    TaskHandle_t taskHandle;
    volatile bool taskRunning;

    static void taskEntry(void* param);
    void taskLoop();

public:
    MultiAxisManager(Motor* mList[NUM_MOTORS], MotionController* cList[NUM_MOTORS], Sensor* s);
    ~MultiAxisManager();

    void begin(uint8_t coreID = MOTION_TASK_CORE,
               uint8_t priority = MOTION_TASK_PRIORITY,
               uint32_t period_ms = MOTION_TASK_PERIOD_MS);

    void setKinematicsHook(KinematicsHook* hook) { ikHook = hook; }

    // --- Single-Axis Controls ---
    void setJointTarget(uint8_t axis, float angle);
    void jogJoint(uint8_t axis, float delta);
    void moveJointRawSteps(uint8_t axis, bool cw, uint32_t steps, uint32_t speedUs = 0);
    void runJointContinuous(uint8_t axis, bool cw, uint32_t speedUs = 0);
    void setJointDriverEnabled(uint8_t axis, bool enabled);
    void triggerJointHome(uint8_t axis);
    void triggerJointCalib(uint8_t axis);
    void stopJoint(uint8_t axis);

    // --- Multi-Axis Coordinated Controls ---
    void setTargetAnglesSync(const float targets[NUM_MOTORS], float moveTimeSec = 0.0f, bool syncArrival = true);
    bool setCartesianPose(const CartesianPose& pose,
                          const IKSolverParams& params = {100, 0.1f, 0.01f},
                          float moveTimeSec = 0.0f);
    void emergencyStopAll();
    void setAllDriversEnabled(bool enabled);

    // --- Query Functions ---
    bool isAnyRunning();
    bool areAllHomed();
    bool areAllCalibrated();
    void getAllAngles(float anglesOut[NUM_MOTORS]);
    void getAllTargets(float targetsOut[NUM_MOTORS]);

    MotionController* getController(uint8_t axis) {
        return (axis < NUM_MOTORS) ? controllers[axis] : nullptr;
    }
    Motor* getMotor(uint8_t axis) {
        return (axis < NUM_MOTORS) ? motors[axis] : nullptr;
    }
    Sensor* getSensor() { return sensor; }
};

#endif // MULTI_AXIS_MANAGER_H
