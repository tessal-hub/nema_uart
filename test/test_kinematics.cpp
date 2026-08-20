#include <unity.h>
#include <Arduino.h>
#include "kinematics.h"
#include "config.h"

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

void test_physical_home_forward_kinematics(void) {
    ArmKinematics kinematics(DH_D1_MM, DH_A2_MM, DH_A3_MM, DH_D4_MM, DH_D6_TOOL_MM);

    float homeEncoderJoints[NUM_MOTORS] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    CartesianPose pose = {0};

    bool ok = kinematics.solveFK(homeEncoderJoints, pose);
    TEST_ASSERT_TRUE_MESSAGE(ok, "FK solve at physical home failed!");

    // Expected from confirmed physical sketch:
    // Wrist center = (126.0, 0.0, 365.0) mm
    // TCP with 20mm tool offset = (126.0, 0.0, 385.0) mm
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 126.0f, pose.x, "Home Pose X mismatch");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 0.0f, pose.y, "Home Pose Y mismatch");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 385.0f, pose.z, "Home Pose Z mismatch (139+138+88+20 = 385mm)");
}

void test_frame_origins_chain(void) {
    ArmKinematics kinematics(DH_D1_MM, DH_A2_MM, DH_A3_MM, DH_D4_MM, DH_D6_TOOL_MM);

    float homeEncoderJoints[NUM_MOTORS] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float origins[7][3];

    kinematics.getFrameOrigins(homeEncoderJoints, origins);

    // Frame 0 (Base Origin): [0, 0, 0]
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, origins[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, origins[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, origins[0][2]);

    // Frame 1 (Joint 1 Top / Shoulder Base): [0, 0, 139]
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, origins[1][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, origins[1][1]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 139.0f, origins[1][2]);

    // Frame 4 (Wrist Center): [126, 0, 365]
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 126.0f, origins[4][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, origins[4][1]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 365.0f, origins[4][2]);
}

void test_ik_roundtrip_precision(void) {
    ArmKinematics kinematics(DH_D1_MM, DH_A2_MM, DH_A3_MM, DH_D4_MM, DH_D6_TOOL_MM);

    // Test with reachable poses
    CartesianPose targetPose = { 150.0f, 50.0f, 280.0f, 0.0f, 0.0f, 0.0f };
    float currentJoints[NUM_MOTORS] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float solvedJoints[NUM_MOTORS] = {0};

    bool ikSuccess = kinematics.solveIK(targetPose, currentJoints, solvedJoints, {100, 0.05f, 0.01f});
    TEST_ASSERT_TRUE_MESSAGE(ikSuccess, "IK failed for reachable Cartesian target");

    // Forward Kinematics verification of solved joints
    CartesianPose computedPose = {0};
    bool fkSuccess = kinematics.solveFK(solvedJoints, computedPose);
    TEST_ASSERT_TRUE_MESSAGE(fkSuccess, "FK verification failed");

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, targetPose.x, computedPose.x, "IK-FK Roundtrip X error > 0.5mm");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, targetPose.y, computedPose.y, "IK-FK Roundtrip Y error > 0.5mm");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, targetPose.z, computedPose.z, "IK-FK Roundtrip Z error > 0.5mm");
}

void test_joint_limits_clamping(void) {
    ArmKinematics kinematics;

    float invalidJoints[NUM_MOTORS] = { 200.0f, -120.0f, 150.0f, 250.0f, -140.0f, 500.0f };
    bool ok = kinematics.checkJointLimits(invalidJoints);
    TEST_ASSERT_FALSE_MESSAGE(ok, "Invalid joints should fail limit check");

    kinematics.clampJointLimits(invalidJoints);
    bool clampedOk = kinematics.checkJointLimits(invalidJoints);
    TEST_ASSERT_TRUE_MESSAGE(clampedOk, "Clamped joints should satisfy all soft limits");

    TEST_ASSERT_FLOAT_WITHIN(0.01f, J1_MAX_LIMIT, invalidJoints[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, J2_MIN_LIMIT, invalidJoints[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, J3_MAX_LIMIT, invalidJoints[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, J4_MAX_LIMIT, invalidJoints[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, J5_MIN_LIMIT, invalidJoints[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, J6_MAX_LIMIT, invalidJoints[5]);
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_physical_home_forward_kinematics);
    RUN_TEST(test_frame_origins_chain);
    RUN_TEST(test_ik_roundtrip_precision);
    RUN_TEST(test_joint_limits_clamping);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {
}
#else
int main(int argc, char **argv) {
    return runUnityTests();
}
#endif
