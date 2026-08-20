#include <unity.h>
#include <Arduino.h>
#include "config.h"

// Test angle normalization helper
float normalizeAngle(float a) {
    float res = fmodf(a, 360.0f);
    if (res < 0.0f) res += 360.0f;
    return res;
}

// Test shortest angle error helper
float getShortestAngleError(float target, float current) {
    float diff = fmodf(target - current + 180.0f, 360.0f);
    if (diff < 0.0f) diff += 360.0f;
    return diff - 180.0f;
}

// Test 16-point LUT linear interpolation helper
float interpolateLUT(float rawAngle, const float sensorAngles[16], const float actualAngles[16]) {
    int idx0 = -1, idx1 = -1;
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        float a0 = sensorAngles[i];
        float a1 = sensorAngles[next];

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

    float a0 = sensorAngles[idx0];
    float a1 = sensorAngles[idx1];
    float target0 = actualAngles[idx0];
    float target1 = actualAngles[idx1];

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

void test_angle_normalization(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, normalizeAngle(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, normalizeAngle(180.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 359.9f, normalizeAngle(359.9f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, normalizeAngle(360.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, normalizeAngle(370.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 350.0f, normalizeAngle(-10.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, normalizeAngle(-180.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, normalizeAngle(-360.0f));
}

void test_shortest_angle_error(void) {
    // Normal cases
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, getShortestAngleError(45.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -45.0f, getShortestAngleError(0.0f, 45.0f));

    // Seam crossing: 355 deg -> 5 deg should be +10 deg, not -350 deg
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, getShortestAngleError(5.0f, 355.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, getShortestAngleError(355.0f, 5.0f));

    // Maximum distance 180 deg
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -180.0f, getShortestAngleError(180.0f, 0.0f));
}

void test_lut_calibration_interpolation(void) {
    float sensorAngles[16];
    float actualAngles[16];

    // Perfect linear test calibration
    for (int i = 0; i < 16; i++) {
        actualAngles[i] = i * 22.5f;
        // Introduce small synthetic non-linear sensor error: sin(x) * 1.5 deg
        sensorAngles[i] = normalizeAngle(actualAngles[i] + 1.5f * sinf(actualAngles[i] * 3.14159265f / 180.0f));
    }

    // Midpoint between station 0 (0 deg) and station 1 (22.5 deg)
    float rawTest = (sensorAngles[0] + sensorAngles[1]) / 2.0f;
    float corrected = interpolateLUT(rawTest, sensorAngles, actualAngles);

    // Corrected value should be close to actual midpoint 11.25 deg
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 11.25f, corrected);
}

void test_schmitt_deadband_logic(void) {
    float enterDeg = 0.3f;
    float exitDeg = 0.8f;
    bool inDeadband = false;

    // Moving from 5 deg towards 0 deg
    float errors[] = { 5.0f, 2.0f, 0.5f, 0.28f, 0.1f, 0.0f, 0.2f, 0.5f, 0.75f, 0.85f };
    bool expectedDeadband[] = { false, false, false, true, true, true, true, true, true, false };

    for (int i = 0; i < 10; i++) {
        float absErr = fabsf(errors[i]);
        if (inDeadband) {
            if (absErr > exitDeg) inDeadband = false;
        } else {
            if (absErr <= enterDeg) inDeadband = true;
        }
        TEST_ASSERT_EQUAL_MESSAGE(expectedDeadband[i], inDeadband, "Schmitt deadband state mismatch");
    }
}

int runMotionTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_angle_normalization);
    RUN_TEST(test_shortest_angle_error);
    RUN_TEST(test_lut_calibration_interpolation);
    RUN_TEST(test_schmitt_deadband_logic);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runMotionTests();
}

void loop() {
}
#else
int main(int argc, char **argv) {
    return runMotionTests();
}
#endif
