#ifndef KINEMATICS_H
#define KINEMATICS_H

#include <Arduino.h>
#include <math.h>
#include "config.h"

// Cấu trúc tọa độ Descartes không gian 3D
struct CartesianPose {
    float x;        // mm (Trục X)
    float y;        // mm (Trục Y)
    float z;        // mm (Trục Z)
    float roll;     // degrees (Góc xoay quanh trục X)
    float pitch;    // degrees (Góc xoay quanh trục Y)
    float yaw;      // degrees (Góc xoay quanh trục Z)
};

// Tham số giải thuật nghịch học động học (IK Solver)
struct IKSolverParams {
    uint16_t maxIterations;
    float toleranceMm;
    float dampingFactor;
};

// Cấu trúc điểm chuyển động (Waypoint)
struct Waypoint {
    char name[20];
    float joints[NUM_MOTORS];
    CartesianPose pose;
    float moveTimeSec;
    uint16_t dwellTimeMs;
};

// Lớp giao diện KinematicsHook
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

// Bộ giải thuật động học Standard DH 6-DOF đã được xác thực
class ArmKinematics : public KinematicsHook {
private:
    float d1;       // 139.0 mm
    float a2;       // 138.0 mm
    float a3;       // 88.0 mm
    float d4;       // 126.0 mm
    float d6_tool;  // 20.0 mm (Tool TCP offset)

    static const float DEG_TO_RAD_FACTOR;
    static const float RAD_TO_DEG_FACTOR;

    float rad(float deg) const { return deg * DEG_TO_RAD_FACTOR; }
    float deg(float rad) const { return rad * RAD_TO_DEG_FACTOR; }

    static void dhTransform(float a_prev, float alpha_prev, float d, float theta, float outT[4][4]);
    static void matrixMultiply(const float A[4][4], const float B[4][4], float out[4][4]);
    static void eulerToRotationMatrix(float rollDeg, float pitchDeg, float yawDeg, float R[3][3]);
    static void rotationMatrixToEuler(const float R[3][3], float& rollDeg, float& pitchDeg, float& yawDeg);

public:
    ArmKinematics(float _d1 = DH_D1_MM,
                  float _a2 = DH_A2_MM,
                  float _a3 = DH_A3_MM,
                  float _d4 = DH_D4_MM,
                  float _d6_tool = DH_D6_TOOL_MM);

    // Chuyển đổi giữa góc Encoder và góc quy ước DH
    void encoderToDhRad(const float encoderAnglesDeg[NUM_MOTORS], float dhThetasRad[NUM_MOTORS]) const;
    void dhRadToEncoderDeg(const float dhThetasRad[NUM_MOTORS], float encoderAnglesDeg[NUM_MOTORS]) const;

    // Động học thuận: [J1..J6 encoder] -> [X, Y, Z, Roll, Pitch, Yaw]
    bool solveFK(const float jointAngles[NUM_MOTORS], CartesianPose& outPose) override;

    // Động học nghịch: [X, Y, Z, Roll, Pitch, Yaw] -> [J1..J6 encoder]
    bool solveIK(const CartesianPose& targetPose,
                 const float currentJoints[NUM_MOTORS],
                 float outJoints[NUM_MOTORS],
                 const IKSolverParams& params) override;

    // Lấy tọa độ không gian 3D của tất cả các khớp (Frame 0 đến Frame 6 + TCP)
    void getFrameOrigins(const float jointAngles[NUM_MOTORS], float outOrigins[7][3]) const;

    // Kiểm tra và kẹp giới hạn góc mềm các khớp
    bool checkJointLimits(const float joints[NUM_MOTORS]) const;
    void clampJointLimits(float joints[NUM_MOTORS]) const;
};

#endif // KINEMATICS_H
