#ifndef MULTI_AXIS_MANAGER_H
#define MULTI_AXIS_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "motor.h"
#include "sensor.h"
#include "motion_controller.h"
#include "kinematics.h"

class MultiAxisManager {
private:
    Motor* motors[NUM_MOTORS];
    MotionController* controllers[NUM_MOTORS];
    Sensor* sensor;
    KinematicsHook* ikHook;
    ArmKinematics defaultKinematics;

    float maxVelocityDegPerSec[NUM_MOTORS];

    // Quản lý Waypoint / Quỹ đạo tự động
    Waypoint waypoints[MAX_WAYPOINTS];
    uint8_t waypointCount;
    int8_t currentWaypointIdx;
    bool sequenceRunning;
    bool sequenceLoop;
    uint32_t waypointDwellStartMs;
    bool waitingDwell;

    SemaphoreHandle_t stateMutex;
    TaskHandle_t taskHandle;
    volatile bool taskRunning;

    static void taskEntry(void* param);
    void taskLoop();
    void processWaypointSequence();

public:
    MultiAxisManager(Motor* mList[NUM_MOTORS], MotionController* cList[NUM_MOTORS], Sensor* s);
    ~MultiAxisManager();

    void begin(uint8_t coreID = MOTION_TASK_CORE,
               uint8_t priority = MOTION_TASK_PRIORITY,
               uint32_t period_ms = MOTION_TASK_PERIOD_MS);

    void setKinematicsHook(KinematicsHook* hook) { ikHook = hook ? hook : &defaultKinematics; }

    // --- Single-Axis Controls ---
    void setJointTarget(uint8_t axis, float angle);
    void jogJoint(uint8_t axis, float delta);
    void moveJointRawSteps(uint8_t axis, bool cw, uint32_t steps, uint32_t speedUs = 0);
    void runJointContinuous(uint8_t axis, bool cw, uint32_t speedUs = 0);
    void setJointDriverEnabled(uint8_t axis, bool enabled);
    void triggerJointHome(uint8_t axis);
    void triggerJointZero(uint8_t axis);
    void triggerJointCalib(uint8_t axis);
    void triggerJointAutoDir(uint8_t axis);
    void stopJoint(uint8_t axis);
    void setMaxVelocity(uint8_t axis, float degPerSec);
    float getMaxVelocity(uint8_t axis) const;

    // --- Multi-Axis Coordinated Controls ---
    void setTargetAnglesSync(const float targets[NUM_MOTORS], float moveTimeSec = 0.0f, bool syncArrival = true);
    bool setCartesianPose(const CartesianPose& pose,
                          const IKSolverParams& params = {100, 0.1f, 0.01f},
                          float moveTimeSec = 0.0f);
    bool moveCartesianLinear(const CartesianPose& targetPose, float speedMmPerSec = 50.0f);
    bool getCartesianPose(CartesianPose& outPose);
    void emergencyStopAll();
    void triggerAllHome();
    void triggerAllZero();
    void triggerAllAutoDir();
    void setAllDriversEnabled(bool enabled);

    // --- Waypoint & Trajectory Sequence Management ---
    bool addWaypoint(const char* name, const float joints[NUM_MOTORS], float moveTimeSec = 2.0f, uint16_t dwellMs = 500);
    void clearWaypoints();
    void startSequence(bool loop = false);
    void pauseSequence();
    void stopSequence();
    uint8_t getWaypointCount() const { return waypointCount; }
    int8_t getCurrentWaypointIndex() const { return currentWaypointIdx; }
    bool isSequenceRunning() const { return sequenceRunning; }
    const Waypoint* getWaypoint(uint8_t idx) const { return (idx < waypointCount) ? &waypoints[idx] : nullptr; }

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
    ArmKinematics* getKinematics() { return &defaultKinematics; }
};

#endif // MULTI_AXIS_MANAGER_H
