#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "motor.h"
#include "sensor.h"

const int CALIB_POINTS = 16;

struct CalibData {
    float sensorAngle[CALIB_POINTS];
    float actualAngle[CALIB_POINTS];
    bool isCalibrated;
};

enum ControllerTask { TASK_NONE, TASK_HOME, TASK_CALIB };

class MotionController {
private:
    uint8_t axisId;
    Motor* motor;
    Sensor* sensor;
    Preferences prefs;
    char nvsNamespace[16];

    uint16_t fullStepsPerRev;
    uint16_t currentMicrosteps;
    float gearRatio;
    float stepsPerDegree;

    CalibData calibData;

    float targetAngle;
    float currentAngle;
    float angleTolerance;
    float deadbandEnter;
    float deadbandExit;
    bool inDeadband;

    uint32_t baseIntervalUs;

    bool positioningActive;
    bool closedLoopHold;
    bool dirInvert;
    bool reachedTarget;
    bool runawayDetected;
    float lastObservedError;
    uint32_t activeMoveStartMs;

    bool isHomed;
    float zeroOffsetAngle;
    float totalStrokeDeg;
    float limitLeftDeg;
    float limitRightDeg;

    uint16_t normalCurrentMa;
    uint16_t homingCurrentMa;

    volatile ControllerTask pendingTask;

    void updateStepsPerDegree();
    float normalizeAngle(float a);
    float getShortestAngleError(float target, float current);
    bool seekEndstopSmooth(bool dir, uint32_t maxSteps, float &hitAngle, bool isDebug = false);

public:
    MotionController(uint8_t axisIndex, Motor* m, Sensor* s);

    void begin();
    void update();

    // Calibration
    void runAutoCalibration();
    void clearCalibration();
    void saveCalibration();
    void loadCalibration();

    // Settings Persistence
    void saveSettings();
    void loadSettings();

    // Homing & Positioning
    void runCenterHoming(bool isDebug = false);
    void setTargetAngle(float target);
    void jog(float delta);
    void stop();

    // Raw / Simple Motor Control
    void moveRawSteps(bool cw, uint32_t steps, uint32_t speedUs = 0);
    void runContinuous(bool cw, uint32_t speedUs = 0);
    void setDriverEnabled(bool enabled);

    // Async task triggering from Web / Serial
    void triggerHome() { pendingTask = TASK_HOME; }
    void triggerCalib() { pendingTask = TASK_CALIB; }

    // Angle queries
    float getCorrectedAngle();
    float getHomeRelativeAngle();

    // Getters & Setters
    uint8_t getAxisId() const { return axisId; }
    Motor* getMotor() { return motor; }
    bool getIsHomed() const { return isHomed; }
    bool getIsCalibrated() const { return calibData.isCalibrated; }
    bool getIsRunning() const { return motor->isRunning(); }
    bool isDriverEnabled() const { return motor->isEnabled(); }
    uint32_t getMotorStepsRemaining() const { return motor->getStepsRemaining(); }
    float getTargetAngle() const { return targetAngle; }
    float getCurrentAngle() { return getHomeRelativeAngle(); }
    float getError() { return getShortestAngleError(targetAngle, getHomeRelativeAngle()); }
    float getTotalStroke() const { return totalStrokeDeg; }
    float getLimitLeft() const { return limitLeftDeg; }
    float getLimitRight() const { return limitRightDeg; }
    float getGearRatio() const { return gearRatio; }
    uint32_t getSpeed() const { return baseIntervalUs; }
    uint16_t getCurrentMa() const { return normalCurrentMa; }
    bool getClosedLoopHold() const { return closedLoopHold; }
    bool getDirInvert() const { return dirInvert; }
    float getDeadbandEnter() const { return deadbandEnter; }
    float getDeadbandExit() const { return deadbandExit; }
    bool isInDeadband() const { return inDeadband; }
    bool isRunawayDetected() const { return runawayDetected; }
    const CalibData& getCalibData() const { return calibData; }

    void setSpeed(uint32_t speedUs);
    void setCurrent(uint16_t ma);
    void setGearRatio(float ratio);
    void setClosedLoopHold(bool hold) { closedLoopHold = hold; saveSettings(); }
    void setDirInvert(bool invert) { dirInvert = invert; runawayDetected = false; saveSettings(); }
    void setTolerance(float tol) { if (tol >= 0.05f && tol <= 10.0f) angleTolerance = tol; }
    void setDeadband(float enterDeg, float exitDeg);
    void resetRunaway() { runawayDetected = false; }
};

#endif // MOTION_CONTROLLER_H
