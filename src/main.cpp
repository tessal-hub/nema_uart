#include <Arduino.h>
#include "config.h"
#include "motor.h"
#include "sensor.h"
#include "motion_controller.h"
#include "multi_axis_manager.h"
#include "web_server_manager.h"

// --- 1. KHỞI TẠO 6 MOTOR TRÊN 2 CỔNG HARDWARE UART ---
Motor motor0(&SERIAL_PORT_1, R_SENSE, 0b00, STEP_PIN_0, DIR_PIN_0, "Motor 1");
Motor motor1(&SERIAL_PORT_1, R_SENSE, 0b01, STEP_PIN_1, DIR_PIN_1, "Motor 2");
Motor motor2(&SERIAL_PORT_1, R_SENSE, 0b10, STEP_PIN_2, DIR_PIN_2, "Motor 3");
Motor motor3(&SERIAL_PORT_1, R_SENSE, 0b11, STEP_PIN_3, DIR_PIN_3, "Motor 4");
Motor motor4(&SERIAL_PORT_2, R_SENSE, 0b00, STEP_PIN_4, DIR_PIN_4, "Motor 5");
Motor motor5(&SERIAL_PORT_2, R_SENSE, 0b01, STEP_PIN_5, DIR_PIN_5, "Motor 6");

Motor* motorList[NUM_MOTORS] = { &motor0, &motor1, &motor2, &motor3, &motor4, &motor5 };

// --- 2. KHỞI TẠO CẢM BIẾN AS5600 & 6 MOTION CONTROLLER ---
Sensor sensor;

MotionController ctrl0(0, &motor0, &sensor);
MotionController ctrl1(1, &motor1, &sensor);
MotionController ctrl2(2, &motor2, &sensor);
MotionController ctrl3(3, &motor3, &sensor);
MotionController ctrl4(4, &motor4, &sensor);
MotionController ctrl5(5, &motor5, &sensor);

MotionController* controllerList[NUM_MOTORS] = { &ctrl0, &ctrl1, &ctrl2, &ctrl3, &ctrl4, &ctrl5 };

// --- 3. BỘ ĐIỀU PHỐI ĐA TRỤC & WEB SERVER ---
MultiAxisManager axisManager(motorList, controllerList, &sensor);
WebServerManager webServer(&axisManager, &sensor);

// Buffer đọc Serial
String inputString = "";
bool stringComplete = false;

void testAllUarts() {
    Serial.println("\n--- [KIEM TRA KET NOI TMC2209 UART] ---");
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        bool ok = motorList[i]->testUART();
        uint8_t ver = motorList[i]->getDriverVersion();
        Serial.printf("  Joint %d (M%d) [Addr %u, STEP Pin %d, DIR Pin %d]: %s (Version: 0x%02X)\n",
                      i + 1, i, motorList[i]->getAddress(), motorList[i]->getStepPin(), motorList[i]->getDirPin(),
                      ok ? "OK (Ket noi tot)" : "LOI UART (Khong phan hoi)", ver);
    }
    Serial.println("----------------------------------------\n");
}

void handleSerialCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    // Lệnh toàn cục STOP
    if (cmd.equalsIgnoreCase("STOP") || cmd.equalsIgnoreCase("EMERGENCY STOP") || cmd.equalsIgnoreCase("S")) {
        axisManager.emergencyStopAll();
        Serial.println(">> [SERIAL] DA DUNG KHAN CAP TAT CA 6 TRUC!");
        return;
    }

    // Lệnh kiểm tra UART
    if (cmd.equalsIgnoreCase("TEST UART") || cmd.equalsIgnoreCase("UART") || cmd.equalsIgnoreCase("TEST")) {
        testAllUarts();
        return;
    }

    // Lệnh đồng bộ tất cả: ALL <a1> <a2> <a3> <a4> <a5> <a6> [time]
    if (cmd.startsWith("ALL ") || cmd.startsWith("all ")) {
        String args = cmd.substring(4);
        args.trim();
        float targets[NUM_MOTORS] = {0};
        int parsed = 0;

        char buf[128];
        args.toCharArray(buf, sizeof(buf));
        char* token = strtok(buf, " ");
        float moveTime = 0.0f;

        while (token != nullptr && parsed < NUM_MOTORS) {
            targets[parsed++] = atof(token);
            token = strtok(nullptr, " ");
        }
        if (token != nullptr) {
            moveTime = atof(token);
        }

        if (parsed == NUM_MOTORS) {
            axisManager.setTargetAnglesSync(targets, moveTime, true);
            Serial.printf(">> [SERIAL ALL] Dang quay dong bo 6 truc: [%.1f, %.1f, %.1f, %.1f, %.1f, %.1f] (T=%.1fs)\n",
                          targets[0], targets[1], targets[2], targets[3], targets[4], targets[5], moveTime);
        } else {
            Serial.println(">> [SERIAL ERROR] Lenh ALL can du 6 goc (vd: ALL 0 45 -30 90 0 0 [time])");
        }
        return;
    }

    // Lệnh cho từng motor: M1..M6 <command>
    if ((cmd.charAt(0) == 'M' || cmd.charAt(0) == 'm') && isdigit(cmd.charAt(1))) {
        uint8_t axis = cmd.charAt(1) - '1';
        if (axis >= NUM_MOTORS) {
            Serial.printf(">> [SERIAL ERROR] Khong co truc M%d (Chi ho tro M1-M6)\n", axis + 1);
            return;
        }

        String sub = cmd.substring(2);
        sub.trim();

        if (sub.length() == 0) return;

        // M<x> <angle>
        if (isdigit(sub.charAt(0)) || (sub.charAt(0) == '-' && sub.length() > 1 && isdigit(sub.charAt(1)))) {
            float ang = sub.toFloat();
            axisManager.setJointTarget(axis, ang);
            Serial.printf(">> [SERIAL M%d] Dat goc muc tieu: %.2f deg\n", axis + 1, ang);
            return;
        }

        if (sub.equalsIgnoreCase("HOME") || sub.equalsIgnoreCase("HOMING")) {
            axisManager.triggerJointHome(axis);
            Serial.printf(">> [SERIAL M%d] Bat dau Homing...\n", axis + 1);
        } else if (sub.equalsIgnoreCase("CALIB")) {
            axisManager.triggerJointCalib(axis);
            Serial.printf(">> [SERIAL M%d] Bat dau Auto Calib LUT...\n", axis + 1);
        } else if (sub.equalsIgnoreCase("STOP")) {
            axisManager.stopJoint(axis);
            Serial.printf(">> [SERIAL M%d] Dung dong co.\n", axis + 1);
        } else if (sub.startsWith("INVERT ") || sub.startsWith("invert ")) {
            bool inv = sub.substring(7).toInt() == 1;
            controllerList[axis]->setDirInvert(inv);
            Serial.printf(">> [SERIAL M%d] Da cai dat Invert = %s\n", axis + 1, inv ? "TRUE" : "FALSE");
        } else if (sub.startsWith("STEP ") || sub.startsWith("step ")) {
            long st = sub.substring(5).toInt();
            if (st >= 0) axisManager.moveJointRawSteps(axis, true, (uint32_t)st);
            else axisManager.moveJointRawSteps(axis, false, (uint32_t)(-st));
            Serial.printf(">> [SERIAL M%d] Quay %ld buoc raw.\n", axis + 1, st);
        } else if (sub.equalsIgnoreCase("RUN CW")) {
            axisManager.runJointContinuous(axis, true);
            Serial.printf(">> [SERIAL M%d] Quay lien tuc CW.\n", axis + 1);
        } else if (sub.equalsIgnoreCase("RUN CCW")) {
            axisManager.runJointContinuous(axis, false);
            Serial.printf(">> [SERIAL M%d] Quay lien tuc CCW.\n", axis + 1);
        } else if (sub.equalsIgnoreCase("FREE") || sub.equalsIgnoreCase("DISABLE")) {
            axisManager.setJointDriverEnabled(axis, false);
            Serial.printf(">> [SERIAL M%d] Tha tu do truc.\n", axis + 1);
        } else if (sub.equalsIgnoreCase("ENABLE")) {
            axisManager.setJointDriverEnabled(axis, true);
            Serial.printf(">> [SERIAL M%d] Bat cap nguon Driver.\n", axis + 1);
        }
        return;
    }

    Serial.println(">> [SERIAL] Cu phap: 'M1 45', 'M2 STEP 400', 'M3 INVERT 1', 'TEST UART', 'ALL 0 45 -30 90 0 0', 'STOP'");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n==========================================================================");
    Serial.println("   ESP32-S3 6-AXIS CLOSED-LOOP STEPPER CONTROLLER + IK ARCHITECTURE      ");
    Serial.println("==========================================================================");

    // 1. Khởi động Task đọc 6 cảm biến AS5600 qua PCA9548A trên Core 0 (500Hz)
    Serial.println("[INIT] Khoi dong AS5600 6-Channel Task (PCA9548A Core 0, 500Hz)...");
    sensor.begin(SENSOR_TASK_CORE, SENSOR_TASK_PRIORITY, SENSOR_TASK_PERIOD_MS);
    delay(100);

    // 2. Khởi động Dual Hardware Serial (Serial1 & Serial2)
    Serial.printf("[INIT] Khoi dong Serial1 cho M1-M4 (TX: %d, RX: %d)...\n", TX_PIN_1, RX_PIN_1);
    SERIAL_PORT_1.begin(115200, SERIAL_8N1, RX_PIN_1, TX_PIN_1);

    Serial.printf("[INIT] Khoi dong Serial2 cho M5-M6 (TX: %d, RX: %d)...\n", TX_PIN_2, RX_PIN_2);
    SERIAL_PORT_2.begin(115200, SERIAL_8N1, RX_PIN_2, TX_PIN_2);

    // 3. Khởi động Multi-Axis Motion Control Task trên Core 1 (100Hz)
    Serial.println("[INIT] Khoi dong 6-Axis Motion Control Task (Core 1, 100Hz)...");
    axisManager.begin(MOTION_TASK_CORE, MOTION_TASK_PRIORITY, MOTION_TASK_PERIOD_MS);

    // 4. In kết quả kiểm tra UART của 6 driver
    testAllUarts();

    // 5. Khởi tạo Web Server & Wi-Fi Kép (AP + STA)
    webServer.begin("NEMA-6AXIS-CONTROLLER", "12345678");

    Serial.println("[READY] He thong 6 truc da san sang hoat dong!");
}

void loop() {
    // 1. Phục vụ Web Browser & REST API
    webServer.handle();

    // 2. Đọc dữ liệu nhập từ Serial Monitor
    while (Serial.available()) {
        char inChar = (char)Serial.read();
        if (inChar == '\n' || inChar == '\r') {
            if (inputString.length() > 0) stringComplete = true;
        } else {
            inputString += inChar;
        }
    }

    if (stringComplete) {
        handleSerialCommand(inputString);
        inputString = "";
        stringComplete = false;
    }
}