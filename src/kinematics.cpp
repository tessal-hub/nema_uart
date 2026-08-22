#include "kinematics.h"

const float ArmKinematics::DEG_TO_RAD_FACTOR = 3.14159265358979323846f / 180.0f;
const float ArmKinematics::RAD_TO_DEG_FACTOR = 180.0f / 3.14159265358979323846f;

ArmKinematics::ArmKinematics(float _d1, float _a2, float _a3, float _d4, float _d6_tool)
    : d1(_d1), a2(_a2), a3(_a3), d4(_d4), d6_tool(_d6_tool) {}

void ArmKinematics::encoderToDhRad(const float encoderAnglesDeg[NUM_MOTORS], float dhThetasRad[NUM_MOTORS]) const {
    if (encoderAnglesDeg == nullptr || dhThetasRad == nullptr) return;
    dhThetasRad[0] = rad(encoderAnglesDeg[0]);
    dhThetasRad[1] = rad(encoderAnglesDeg[1] + DH_THETA2_OFFSET_DEG); // -90.0 deg offset
    dhThetasRad[2] = rad(encoderAnglesDeg[2]);
    dhThetasRad[3] = rad(encoderAnglesDeg[3]);
    dhThetasRad[4] = rad(encoderAnglesDeg[4]);
    dhThetasRad[5] = rad(encoderAnglesDeg[5]);
}

void ArmKinematics::dhRadToEncoderDeg(const float dhThetasRad[NUM_MOTORS], float encoderAnglesDeg[NUM_MOTORS]) const {
    if (dhThetasRad == nullptr || encoderAnglesDeg == nullptr) return;
    encoderAnglesDeg[0] = deg(dhThetasRad[0]);
    encoderAnglesDeg[1] = deg(dhThetasRad[1]) - DH_THETA2_OFFSET_DEG; // +90.0 deg offset
    encoderAnglesDeg[2] = deg(dhThetasRad[2]);
    encoderAnglesDeg[3] = deg(dhThetasRad[3]);
    encoderAnglesDeg[4] = deg(dhThetasRad[4]);
    encoderAnglesDeg[5] = deg(dhThetasRad[5]);
}

void ArmKinematics::dhTransform(float a_prev, float alpha_prev, float d, float theta, float outT[4][4]) {
    float ct = cosf(theta);
    float st = sinf(theta);
    float ca = cosf(alpha_prev);
    float sa = sinf(alpha_prev);

    outT[0][0] = ct;       outT[0][1] = -st;      outT[0][2] = 0.0f;  outT[0][3] = a_prev;
    outT[1][0] = st * ca;  outT[1][1] = ct * ca;  outT[1][2] = -sa;   outT[1][3] = -sa * d;
    outT[2][0] = st * sa;  outT[2][1] = ct * sa;  outT[2][2] = ca;    outT[2][3] = ca * d;
    outT[3][0] = 0.0f;     outT[3][1] = 0.0f;     outT[3][2] = 0.0f;  outT[3][3] = 1.0f;
}

void ArmKinematics::matrixMultiply(const float A[4][4], const float B[4][4], float out[4][4]) {
    float temp[4][4] = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i][j] = A[i][0] * B[0][j] +
                         A[i][1] * B[1][j] +
                         A[i][2] * B[2][j] +
                         A[i][3] * B[3][j];
        }
    }
    memcpy(out, temp, sizeof(temp));
}

void ArmKinematics::eulerToRotationMatrix(float rollDeg, float pitchDeg, float yawDeg, float R[3][3]) {
    float r = rollDeg * DEG_TO_RAD_FACTOR;
    float p = pitchDeg * DEG_TO_RAD_FACTOR;
    float y = yawDeg * DEG_TO_RAD_FACTOR;

    float cr = cosf(r), sr = sinf(r);
    float cp = cosf(p), sp = sinf(p);
    float cy = cosf(y), sy = sinf(y);

    // ZYX Euler Rotation Matrix (Yaw -> Pitch -> Roll)
    R[0][0] = cy * cp;
    R[0][1] = cy * sp * sr - sy * cr;
    R[0][2] = cy * sp * cr + sy * sr;

    R[1][0] = sy * cp;
    R[1][1] = sy * sp * sr + cy * cr;
    R[1][2] = sy * sp * cr - cy * sr;

    R[2][0] = -sp;
    R[2][1] = cp * sr;
    R[2][2] = cp * cr;
}

void ArmKinematics::rotationMatrixToEuler(const float R[3][3], float& rollDeg, float& pitchDeg, float& yawDeg) {
    // Pitch: -sp = R[2][0] -> pitch = asin(-R[2][0])
    float sp = -R[2][0];
    if (sp > 1.0f) sp = 1.0f;
    if (sp < -1.0f) sp = -1.0f;
    float p = asinf(sp);

    float cp = cosf(p);
    float r = 0.0f;
    float y = 0.0f;

    if (fabsf(cp) > 0.0001f) {
        r = atan2f(R[2][1], R[2][2]);
        y = atan2f(R[1][0], R[0][0]);
    } else {
        // Gimbal lock
        r = atan2f(-R[1][2], R[1][1]);
        y = 0.0f;
    }

    rollDeg  = r * RAD_TO_DEG_FACTOR;
    pitchDeg = p * RAD_TO_DEG_FACTOR;
    yawDeg   = y * RAD_TO_DEG_FACTOR;
}

bool ArmKinematics::solveFK(const float jointAngles[NUM_MOTORS], CartesianPose& outPose) {
    if (jointAngles == nullptr) return false;

    float dhThetas[NUM_MOTORS];
    encoderToDhRad(jointAngles, dhThetas);

    // DH parameters: (a_{i-1} [mm], alpha_{i-1} [rad], d_i [mm])
    const float dhParams[6][3] = {
        { 0.0f,  0.0f,                         d1 },       // joint 1
        { 0.0f, -3.1415926535f / 2.0f,         0.0f },     // joint 2
        { a2,    0.0f,                         0.0f },     // joint 3
        { a3,   -3.1415926535f / 2.0f,         d4 },       // joint 4
        { 0.0f,  3.1415926535f / 2.0f,         0.0f },     // joint 5
        { 0.0f, -3.1415926535f / 2.0f,         0.0f }      // joint 6
    };

    float T[4][4] = {
        {1,0,0,0},
        {0,1,0,0},
        {0,0,1,0},
        {0,0,0,1}
    };

    for (int i = 0; i < 6; i++) {
        float Ti[4][4];
        dhTransform(dhParams[i][0], dhParams[i][1], dhParams[i][2], dhThetas[i], Ti);
        matrixMultiply(T, Ti, T);
    }

    // Apply tool offset along the final z-axis (column 2 of rotation matrix)
    outPose.x = T[0][3] + T[0][2] * d6_tool;
    outPose.y = T[1][3] + T[1][2] * d6_tool;
    outPose.z = T[2][3] + T[2][2] * d6_tool;

    float R[3][3];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            R[r][c] = T[r][c];
        }
    }
    rotationMatrixToEuler(R, outPose.roll, outPose.pitch, outPose.yaw);

    return true;
}

void ArmKinematics::getFrameOrigins(const float jointAngles[NUM_MOTORS], float outOrigins[7][3]) const {
    if (jointAngles == nullptr || outOrigins == nullptr) return;

    float dhThetas[NUM_MOTORS];
    encoderToDhRad(jointAngles, dhThetas);

    const float dhParams[6][3] = {
        { 0.0f,  0.0f,                         d1 },
        { 0.0f, -3.1415926535f / 2.0f,         0.0f },
        { a2,    0.0f,                         0.0f },
        { a3,   -3.1415926535f / 2.0f,         d4 },
        { 0.0f,  3.1415926535f / 2.0f,         0.0f },
        { 0.0f, -3.1415926535f / 2.0f,         0.0f }
    };

    float T[4][4] = {
        {1,0,0,0},
        {0,1,0,0},
        {0,0,1,0},
        {0,0,0,1}
    };

    // Base origin (Frame 0)
    outOrigins[0][0] = 0.0f;
    outOrigins[0][1] = 0.0f;
    outOrigins[0][2] = 0.0f;

    for (int i = 0; i < 6; i++) {
        float Ti[4][4];
        dhTransform(dhParams[i][0], dhParams[i][1], dhParams[i][2], dhThetas[i], Ti);
        matrixMultiply(T, Ti, T);
        outOrigins[i + 1][0] = T[0][3];
        outOrigins[i + 1][1] = T[1][3];
        outOrigins[i + 1][2] = T[2][3];
    }
}

bool ArmKinematics::solveIK(const CartesianPose& targetPose,
                            const float currentJoints[NUM_MOTORS],
                            float outJoints[NUM_MOTORS],
                            const IKSolverParams& params) {
    if (outJoints == nullptr) return false;

    // 1. Ma trận xoay mục tiêu từ Roll, Pitch, Yaw
    float R_target[3][3];
    eulerToRotationMatrix(targetPose.roll, targetPose.pitch, targetPose.yaw, R_target);

    // Vector hướng trục Z của Tool: R_target[:, 2]
    float z_tool_x = R_target[0][2];
    float z_tool_y = R_target[1][2];
    float z_tool_z = R_target[2][2];

    // 2. Tọa độ tâm cổ tay (Wrist Center = Frame 4 Origin)
    float xw = targetPose.x - d6_tool * z_tool_x;
    float yw = targetPose.y - d6_tool * z_tool_y;
    float zw = targetPose.z - d6_tool * z_tool_z;

    // 3. Giải Joint 1 (Base Yaw): theta1
    float theta1 = atan2f(yw, xw);

    // 4. Giải Joint 2 và Joint 3 trong mặt phẳng cánh tay
    float Rw = sqrtf(xw * xw + yw * yw);
    float Zw_prime = zw - d1;
    float Yarm = -Zw_prime; // Hệ trục phẳng chuẩn

    // Chiều dài link 2: a2 (138 mm)
    // Chiều dài hiệu dụng link 3 từ elbow đến wrist center: L3_eff = sqrt(a3^2 + d4^2) = sqrt(88^2 + 126^2)
    float L2 = a2;
    float L3_eff = sqrtf(a3 * a3 + d4 * d4);
    float phi = atan2f(d4, a3); // Góc lệch giữa a3 và d4 (~55.07 deg)

    float S_squared = Rw * Rw + Yarm * Yarm;
    float S = sqrtf(S_squared);

    float maxReach = L2 + L3_eff - 1.0f; // Tránh kỳ dị
    float minReach = fabsf(L2 - L3_eff) + 1.0f;

    if (S > maxReach) {
        float scale = maxReach / (S > 0.001f ? S : 0.001f);
        Rw *= scale;
        Yarm *= scale;
        S = maxReach;
        S_squared = S * S;
    } else if (S < minReach) {
        float scale = minReach / (S > 0.001f ? S : 0.001f);
        Rw *= scale;
        Yarm *= scale;
        S = minReach;
        S_squared = S * S;
    }

    // Định lý hàm Cosin cho góc khớp khuỷu (theta3)
    // gamma = theta3 + phi
    float cos_gamma = (S_squared - L2 * L2 - L3_eff * L3_eff) / (2.0f * L2 * L3_eff);
    if (cos_gamma > 1.0f) cos_gamma = 1.0f;
    if (cos_gamma < -1.0f) cos_gamma = -1.0f;

    // Chọn nghiệm Elbow Up / Down gần với trạng thái hiện tại
    float gamma = acosf(cos_gamma);
    float theta3 = gamma - phi;

    // Giải theta2
    float alpha = atan2f(Yarm, Rw);
    float beta = atan2f(L3_eff * sinf(gamma), L2 + L3_eff * cosf(gamma));
    float theta2 = alpha - beta;

    // 5. Giải hướng xoay cổ tay Spherical Wrist (Joints 4, 5, 6)
    // Tính ma trận xoay R_0_3 từ theta1, theta2, theta3
    float T03[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    float T1[4][4], T2[4][4], T3[4][4];
    dhTransform(0.0f,  0.0f,                         d1,   theta1, T1);
    dhTransform(0.0f, -3.1415926535f / 2.0f,         0.0f, theta2, T2);
    dhTransform(a2,    0.0f,                         0.0f, theta3, T3);

    matrixMultiply(T03, T1, T03);
    matrixMultiply(T03, T2, T03);
    matrixMultiply(T03, T3, T03);

    // R36 = (R03)^T * R_target
    float R03_T[3][3];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            R03_T[r][c] = T03[c][r]; // Transpose
        }
    }

    float R36[3][3] = {0};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            for (int k = 0; k < 3; k++) {
                R36[r][c] += R03_T[r][k] * R_target[k][c];
            }
        }
    }

    // Từ cấu trúc DH của joints 4, 5, 6:
    // R36[1, 2] = cos(theta5)
    // R36[0, 2] = -cos(theta4)*sin(theta5)
    // R36[2, 2] = sin(theta4)*sin(theta5)
    // R36[1, 0] = sin(theta5)*cos(theta6)
    // R36[1, 1] = -sin(theta5)*sin(theta6)
    float cos_theta5 = R36[1][2];
    if (cos_theta5 > 1.0f) cos_theta5 = 1.0f;
    if (cos_theta5 < -1.0f) cos_theta5 = -1.0f;

    float sin_theta5 = sqrtf(R36[0][2] * R36[0][2] + R36[2][2] * R36[2][2]);
    float theta5 = atan2f(sin_theta5, cos_theta5);

    float theta4 = 0.0f;
    float theta6 = 0.0f;

    if (fabsf(sin_theta5) > 0.001f) {
        theta4 = atan2f(R36[2][2], -R36[0][2]);
        theta6 = atan2f(-R36[1][1], R36[1][0]);
    } else {
        // Wrist singularity: theta5 ~ 0, giữ theta4 hiện tại
        theta4 = (currentJoints != nullptr) ? rad(currentJoints[3]) : 0.0f;
        theta6 = atan2f(R36[0][1], R36[0][0]) - theta4;
    }

    // 6. Đổi từ góc DH rad sang góc Encoder độ
    float dhResult[NUM_MOTORS] = { theta1, theta2, theta3, theta4, theta5, theta6 };
    dhRadToEncoderDeg(dhResult, outJoints);

    // Kẹp giới hạn an toàn
    clampJointLimits(outJoints);
    return true;
}

bool ArmKinematics::checkJointLimits(const float joints[NUM_MOTORS]) const {
    if (joints == nullptr) return false;
    if (joints[0] < J1_MIN_LIMIT || joints[0] > J1_MAX_LIMIT) return false;
    if (joints[1] < J2_MIN_LIMIT || joints[1] > J2_MAX_LIMIT) return false;
    if (joints[2] < J3_MIN_LIMIT || joints[2] > J3_MAX_LIMIT) return false;
    if (joints[3] < J4_MIN_LIMIT || joints[3] > J4_MAX_LIMIT) return false;
    if (joints[4] < J5_MIN_LIMIT || joints[4] > J5_MAX_LIMIT) return false;
    if (joints[5] < J6_MIN_LIMIT || joints[5] > J6_MAX_LIMIT) return false;
    return true;
}

void ArmKinematics::clampJointLimits(float joints[NUM_MOTORS]) const {
    if (joints == nullptr) return;
    if (joints[0] < J1_MIN_LIMIT) joints[0] = J1_MIN_LIMIT;
    if (joints[0] > J1_MAX_LIMIT) joints[0] = J1_MAX_LIMIT;

    if (joints[1] < J2_MIN_LIMIT) joints[1] = J2_MIN_LIMIT;
    if (joints[1] > J2_MAX_LIMIT) joints[1] = J2_MAX_LIMIT;

    if (joints[2] < J3_MIN_LIMIT) joints[2] = J3_MIN_LIMIT;
    if (joints[2] > J3_MAX_LIMIT) joints[2] = J3_MAX_LIMIT;

    if (joints[3] < J4_MIN_LIMIT) joints[3] = J4_MIN_LIMIT;
    if (joints[3] > J4_MAX_LIMIT) joints[3] = J4_MAX_LIMIT;

    if (joints[4] < J5_MIN_LIMIT) joints[4] = J5_MIN_LIMIT;
    if (joints[4] > J5_MAX_LIMIT) joints[4] = J5_MAX_LIMIT;

    if (joints[5] < J6_MIN_LIMIT) joints[5] = J6_MIN_LIMIT;
    if (joints[5] > J6_MAX_LIMIT) joints[5] = J6_MAX_LIMIT;
}
