#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "motor.h"
#include "sensor.h"
#include "motion_controller.h"
#include "multi_axis_manager.h"
#include "web_server_manager.h"
#include "kinematics.h"

// ==============================================================================
// 1. KHỞI TẠO 6 ĐỘNG CƠ TRÊN 2 CỔNG HARDWARE UART
// ==============================================================================
Motor motor0(&SERIAL_PORT_1, R_SENSE, 0b00, STEP_PIN_0, DIR_PIN_0, "Joint 1 (Base Yaw)");
Motor motor1(&SERIAL_PORT_1, R_SENSE, 0b01, STEP_PIN_1, DIR_PIN_1, "Joint 2 (Shoulder)");
Motor motor2(&SERIAL_PORT_1, R_SENSE, 0b10, STEP_PIN_2, DIR_PIN_2, "Joint 3 (Elbow)");
Motor motor3(&SERIAL_PORT_1, R_SENSE, 0b11, STEP_PIN_3, DIR_PIN_3, "Joint 4 (Wrist Roll)");
Motor motor4(&SERIAL_PORT_2, R_SENSE, 0b00, STEP_PIN_4, DIR_PIN_4, "Joint 5 (Wrist Pitch)");
Motor motor5(&SERIAL_PORT_2, R_SENSE, 0b01, STEP_PIN_5, DIR_PIN_5, "Joint 6 (Flange Roll)");

Motor* motorList[NUM_MOTORS] = { &motor0, &motor1, &motor2, &motor3, &motor4, &motor5 };

// ==============================================================================
// 2. KHỞI TẠO CẢM BIẾN AS5600 VÀ 6 MOTION CONTROLLERS
// ==============================================================================
Sensor sensor;

MotionController ctrl0(0, &motor0, &sensor);
MotionController ctrl1(1, &motor1, &sensor);
MotionController ctrl2(2, &motor2, &sensor);
MotionController ctrl3(3, &motor3, &sensor);
MotionController ctrl4(4, &motor4, &sensor);
MotionController ctrl5(5, &motor5, &sensor);

MotionController* controllerList[NUM_MOTORS] = { &ctrl0, &ctrl1, &ctrl2, &ctrl3, &ctrl4, &ctrl5 };

// ==============================================================================
// 3. BỘ ĐIỀU PHỐI ĐA TRỤC & WEB SERVER
// ==============================================================================
MultiAxisManager axisManager(motorList, controllerList, &sensor);
WebServerManager webServer(&axisManager, &sensor);

// Mutex bảo vệ truy cập UART đa điểm trên Serial1 và Serial2
SemaphoreHandle_t uartMutex1 = nullptr;
SemaphoreHandle_t uartMutex2 = nullptr;

// Buffer đọc lệnh từ Serial CLI
String inputString = "";
bool stringComplete = false;

// ==============================================================================
// 4. CHỨC NĂNG CHẨN ĐOÁN & KIỂM TRA UART
// ==============================================================================
void testAllUarts() {
    Serial.println("\n--- [KIEM TRA KET NOI TMC2209 MULTI-DROP UART] ---");
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        TMC2209Diag diag = motorList[i]->getDriverStatus();
        Serial.printf("  Joint %d (M%d) [Addr %u, STEP Pin %d, DIR Pin %d]: %s (Ver: 0x%02X, SG: %u, CS: %u)\n",
                      i + 1, i, motorList[i]->getAddress(), motorList[i]->getStepPin(), motorList[i]->getDirPin(),
                      diag.uartOk ? "OK (Ket noi tot)" : "LOI UART (Khong phan hoi)",
                      diag.driverVersion, diag.sgResult, diag.csActual);
    }
    Serial.println("--------------------------------------------------\n");
}

void printSystemStatus() {
    CartesianPose pose = {0};
    axisManager.getCartesianPose(pose);

    Serial.println("\n============== [TRANG THAI HE THONG 6 TRUC] ==============");
    Serial.printf("  Firmware : %s %s (Build: %s)\n", FW_NAME, FW_VERSION, FW_BUILD_DATE);
    Serial.printf("  Free RAM : %u KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("  Wi-Fi    : %s (STA IP: %s | AP IP: %s)\n",
                  WiFi.status() == WL_CONNECTED ? "Connected (STA)" : "AP Mode Only",
                  WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str());
    Serial.printf("  TCP Pose : X=%.1f mm, Y=%.1f mm, Z=%.1f mm | Roll=%.1f, Pitch=%.1f, Yaw=%.1f\n",
                  pose.x, pose.y, pose.z, pose.roll, pose.pitch, pose.yaw);
    Serial.println("----------------------------------------------------------");
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        float cur = controllerList[i]->getCurrentAngle();
        float tgt = controllerList[i]->getTargetAngle();
        float err = controllerList[i]->getError();
        AS5600Diag sDiag = sensor.getDiagnostics(i);
        Serial.printf("  J%d: Cur=%6.2f deg | Tgt=%6.2f deg | Err=%5.2f | %s | %s | AS5600: %s (AGC:%u)\n",
                      i + 1, cur, tgt, err,
                      controllerList[i]->getIsHomed() ? "HOMED" : "UNHOMED",
                      motorList[i]->isRunning() ? "RUNNING" : (controllerList[i]->isInDeadband() ? "HOLD" : "IDLE"),
                      sDiag.readSuccess ? (sDiag.magnetOptimal ? "OK" : "MAG_WARN") : "I2C_ERR",
                      sDiag.agc);
    }
    Serial.println("==========================================================\n");
}

void printHelpMenu() {
    Serial.println("\n================ [DANH SACH LENH SERIAL CLI & G-CODE] ================");
    Serial.println(" 1. DIEU KHIEN DON TRUC:");
    Serial.println("    M<1-6> <angle>               : Dat goc dich (vd: M1 45.0)");
    Serial.println("    M<1-6> JOG <delta>           : Nhich goc tuong doi (vd: M2 JOG 5.0)");
    Serial.println("    M<1-6> STEP <steps> [speed]  : Quay buoc raw (vd: M3 STEP 400 300)");
    Serial.println("    M<1-6> RUN <CW/CCW> [speed]  : Quay lien tuc (vd: M1 RUN CW)");
    Serial.println("    M<1-6> STOP                  : Dung truc chi dinh");
    Serial.println("    M<1-6> HOME                  : Chay Homing cung lon tim trung diem");
    Serial.println("    M<1-6> ZERO                  : Dat vi tri hien tai lam goc 0.00 deg");
    Serial.println("    M<1-6> CALIB                 : Chay Auto Calibration 16 diem LUT");
    Serial.println("    M<1-6> ENABLE / FREE         : Bat / Tha tu do truc");
    Serial.println("    M<1-6> INVERT <0/1>          : Cai dat dao chieu quay");
    Serial.println("    M<1-6> HOLD <0/1>            : Cai dat giu vi tri vong kin");
    Serial.println("    M<1-6> SPEED <us>            : Cai dat toc do xung (150-1500 us)");
    Serial.println("    M<1-6> CURR <mA>             : Cai dat dong RMS TMC2209 (200-1400)");
    Serial.println("");
    Serial.println(" 2. DIEU KHIEN DONG BO & G-CODE CHUAN:");
    Serial.println("    G0 X<x> Y<y> Z<z> [R<r>] [P<p>] [W<w>] [T<t>] : Di chuyen nhanh Descartes");
    Serial.println("    G1 X<x> Y<y> Z<z> [R<r>] [P<p>] [W<w>] [F<f>] : Di chuyen noi suy duong thang");
    Serial.println("    G28                          : Homing tat ca cac truc");
    Serial.println("    M17 / M18 (M84)              : Bat / Tat cap nguon Driver 6 truc");
    Serial.println("    M112 / STOP / S              : DUNG KHAN CAP TAT CA 6 TRUC!");
    Serial.println("    M114 / STATUS                : In trang thai & vi tri hien tai");
    Serial.println("    ALL <j1> <j2> <j3> <j4> <j5> <j6> [time] : Quay dong bo 6 truc");
    Serial.println("");
    Serial.println(" 3. KHONG GIAN DESCARTES (IK):");
    Serial.println("    IK <x> <y> <z> [roll] [pitch] [yaw] [time] : Di chuyen dau gap TCP");
    Serial.println("    POSE                         : In toa do TCP hien tai");
    Serial.println("");
    Serial.println(" 4. QUY DAO WAYPOINTS:");
    Serial.println("    WP ADD <name> [time] [dwell] : Luu goc hien tai thanh Waypoint");
    Serial.println("    WP LIST                      : Danh sach Waypoints");
    Serial.println("    WP START [LOOP]              : Bat dau phat quy dao");
    Serial.println("    WP PAUSE / WP STOP / WP CLEAR: Quan ly chuoi quy dao");
    Serial.println("");
    Serial.println(" 5. HE THONG & CHAN DOAN:");
    Serial.println("    TEST UART                    : Kiem tra ket noi thanh ghi TMC2209");
    Serial.println("    REBOOT                       : Khoi dong lai ESP32");
    Serial.println("    HELP / ?                     : Hien thi menu tro giup");
    Serial.println("======================================================================\n");
}

// ==============================================================================
// 5. XỬ LÝ LỆNH TỪ SERIAL CLI & G-CODE
// ==============================================================================
String handleSerialCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return "";

    // 1. Lệnh Dừng Khẩn Cấp Toàn Cục
    if (cmd.equalsIgnoreCase("STOP") || cmd.equalsIgnoreCase("EMERGENCY STOP") ||
        cmd.equalsIgnoreCase("S") || cmd.equalsIgnoreCase("M112")) {
        axisManager.emergencyStopAll();
        Serial.println(">> [SERIAL] ⛔ DA DUNG KHAN CAP TAT CA 6 TRUC!");
        return "⛔ DA DUNG KHAN CAP TAT CA 6 TRUC!";
    }

    // 2. Lệnh Trợ Giúp / Thông Tin
    // 2. Lệnh Trợ Giúp / Thông Tin
    if (cmd.equalsIgnoreCase("HELP") || cmd.equalsIgnoreCase("?")) {
        printHelpMenu();
        return "Type HELP in Serial monitor for full menu";
    }

    if (cmd.equalsIgnoreCase("STATUS") || cmd.equalsIgnoreCase("M114")) {
        printSystemStatus();
        return "Printed status to Serial";
    }

    if (cmd.equalsIgnoreCase("TEST UART") || cmd.equalsIgnoreCase("UART") || cmd.equalsIgnoreCase("TEST")) {
        testAllUarts();
        return "UART test complete";
    }

    if (cmd.equalsIgnoreCase("POSE")) {
        CartesianPose pose = {0};
        axisManager.getCartesianPose(pose);
        char buf[128];
        snprintf(buf, sizeof(buf), "X=%.1f mm, Y=%.1f mm, Z=%.1f mm, R=%.1f, P=%.1f, Y=%.1f",
                 pose.x, pose.y, pose.z, pose.roll, pose.pitch, pose.yaw);
        Serial.printf(">> [TCP POSE] %s\n", buf);
        return String(buf);
    }

    if (cmd.equalsIgnoreCase("REBOOT")) {
        Serial.println(">> [SYSTEM] Rebooting ESP32...");
        delay(500);
        ESP.restart();
        return "Rebooting...";
    }

    // 3. G-code Standard Commands (G0, G1, G28, M17, M18, M84)
    if (cmd.equalsIgnoreCase("G28") || cmd.equalsIgnoreCase("ALL HOME")) {
        axisManager.triggerAllHome();
        Serial.println(">> [G28] Bat dau Homing tat ca 6 truc...");
        return "G28: Homing all axes...";
    }

    if (cmd.equalsIgnoreCase("M17") || cmd.equalsIgnoreCase("ALL ENABLE")) {
        axisManager.setAllDriversEnabled(true);
        Serial.println(">> [M17] Bat cap nguon Driver tat ca 6 truc.");
        return "M17: Enabled all drivers";
    }

    if (cmd.equalsIgnoreCase("M18") || cmd.equalsIgnoreCase("M84") ||
        cmd.equalsIgnoreCase("ALL FREE") || cmd.equalsIgnoreCase("ALL DISABLE")) {
        axisManager.setAllDriversEnabled(false);
        Serial.println(">> [M18/M84] Tha tu do tat ca 6 truc.");
        return "M18/M84: Disabled all drivers";
    }

    if (cmd.equalsIgnoreCase("ALL ZERO")) {
        axisManager.triggerAllZero();
        Serial.println(">> [ALL ZERO] Da dat mốc Home 0.00 deg cho tat ca cac truc.");
        return "All axes zeroed";
    }

    if (cmd.equalsIgnoreCase("ALL AUTODIR") || cmd.equalsIgnoreCase("ALL DIR")) {
        axisManager.triggerAllAutoDir();
        Serial.println(">> [ALL AUTODIR] Dang tu dong kiem tra va luu chieu quay 6 truc...");
        return "All axes auto-direction started";
    }

    // G0 / G1 Cartesian Motions
    if (cmd.startsWith("G0 ") || cmd.startsWith("g0 ") || cmd.startsWith("G1 ") || cmd.startsWith("g1 ")) {
        bool isLinear = (cmd.charAt(1) == '1');
        CartesianPose targetPose = {0};
        axisManager.getCartesianPose(targetPose);

        float feedRate = 50.0f; // mm/s default
        float moveTime = 2.0f;

        // Parse key-value tokens (X<val> Y<val> Z<val> R<val> P<val> W<val> F<val> T<val>)
        String tokens = cmd.substring(3);
        tokens.trim();

        char buf[128];
        tokens.toCharArray(buf, sizeof(buf));
        char* token = strtok(buf, " ");

        while (token != nullptr) {
            char key = toupper(token[0]);
            float val = atof(token + 1);
            if (key == 'X') targetPose.x = val;
            else if (key == 'Y') targetPose.y = val;
            else if (key == 'Z') targetPose.z = val;
            else if (key == 'R') targetPose.roll = val;
            else if (key == 'P') targetPose.pitch = val;
            else if (key == 'W') targetPose.yaw = val;
            else if (key == 'F') feedRate = val;
            else if (key == 'T') moveTime = val;

            token = strtok(nullptr, " ");
        }

        bool ok = false;
        if (isLinear) {
            ok = axisManager.moveCartesianLinear(targetPose, feedRate);
            Serial.printf(">> [G1 LINEAR] Den [X:%.1f, Y:%.1f, Z:%.1f] @ Feed: %.1f mm/s (%s)\n",
                          targetPose.x, targetPose.y, targetPose.z, feedRate, ok ? "OK" : "LOI TAM VOI");
            return ok ? "G1 Linear motion started" : "G1 Error: Reach limit";
        } else {
            ok = axisManager.setCartesianPose(targetPose, {100, 0.1f, 0.01f}, moveTime);
            Serial.printf(">> [G0 RAPID] Den [X:%.1f, Y:%.1f, Z:%.1f] (T=%.1fs) (%s)\n",
                          targetPose.x, targetPose.y, targetPose.z, moveTime, ok ? "OK" : "LOI TAM VOI");
            return ok ? "G0 Rapid motion started" : "G0 Error: Reach limit";
        }
    }

    // ALL <a1> <a2> <a3> <a4> <a5> <a6> [time]
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
            return "ALL: Coordinated motion started";
        } else {
            Serial.println(">> [SERIAL ERROR] Lenh ALL can 6 goc (vd: ALL 0 45 -30 90 0 0 [time])");
            return "Error: ALL requires 6 angles";
        }
    }

    // 4. Lệnh Động Học Không Gian IK <x> <y> <z> [roll] [pitch] [yaw] [time]
    if (cmd.startsWith("IK ") || cmd.startsWith("ik ")) {
        String args = cmd.substring(3);
        args.trim();

        char buf[128];
        args.toCharArray(buf, sizeof(buf));
        char* token = strtok(buf, " ");

        CartesianPose targetPose = {0};
        float params[7] = {0};
        int count = 0;

        while (token != nullptr && count < 7) {
            params[count++] = atof(token);
            token = strtok(nullptr, " ");
        }

        if (count >= 3) {
            targetPose.x = params[0];
            targetPose.y = params[1];
            targetPose.z = params[2];
            targetPose.roll  = (count >= 4) ? params[3] : 0.0f;
            targetPose.pitch = (count >= 5) ? params[4] : 0.0f;
            targetPose.yaw   = (count >= 6) ? params[5] : 0.0f;
            float moveTime   = (count >= 7) ? params[6] : 2.0f;

            bool ok = axisManager.setCartesianPose(targetPose, {100, 0.1f, 0.01f}, moveTime);
            if (ok) {
                Serial.printf(">> [IK GOTO] Di chuyen TCP den [X:%.1f, Y:%.1f, Z:%.1f] (T=%.1fs)\n",
                              targetPose.x, targetPose.y, targetPose.z, moveTime);
                return "IK Target Accepted";
            } else {
                Serial.println(">> [IK ERROR] Toa do ngoai tam voi hoac vi pham gioi han khop!");
                return "IK Error: Unreachable or joint limit";
            }
        } else {
            Serial.println(">> [IK ERROR] Cu phap: IK <x> <y> <z> [roll] [pitch] [yaw] [time]");
            return "Syntax: IK <x> <y> <z> [roll] [pitch] [yaw] [time]";
        }
    }

    // 5. Lệnh Waypoints & Quỹ Đạo
    if (cmd.startsWith("WP ") || cmd.startsWith("wp ")) {
        String sub = cmd.substring(3);
        sub.trim();

        if (sub.equalsIgnoreCase("LIST")) {
            uint8_t count = axisManager.getWaypointCount();
            Serial.printf("\n--- DANH SACH WAYPOINTS (%d DIEM) ---\n", count);
            for (uint8_t i = 0; i < count; i++) {
                const Waypoint* wp = axisManager.getWaypoint(i);
                if (wp) {
                    Serial.printf("  #%d [%s]: [%.1f, %.1f, %.1f, %.1f, %.1f, %.1f] (T=%.1fs, Dwell=%ums)\n",
                                  i + 1, wp->name, wp->joints[0], wp->joints[1], wp->joints[2],
                                  wp->joints[3], wp->joints[4], wp->joints[5], wp->moveTimeSec, wp->dwellTimeMs);
                }
            }
            Serial.println("----------------------------------------\n");
            return "Waypoint list printed to Serial";
        } else if (sub.startsWith("ADD ") || sub.startsWith("add ")) {
            String name = sub.substring(4);
            name.trim();
            float joints[NUM_MOTORS];
            axisManager.getAllAngles(joints);
            axisManager.addWaypoint(name.c_str(), joints, 2.0f, 500);
            Serial.printf(">> [WP] Da them Waypoint: %s\n", name.c_str());
            return "Waypoint added: " + name;
        } else if (sub.startsWith("START") || sub.startsWith("start")) {
            bool loop = sub.indexOf("LOOP") > 0 || sub.indexOf("loop") > 0;
            axisManager.startSequence(loop);
            Serial.printf(">> [WP] Bat dau chay quy dao (Loop: %s)\n", loop ? "TRUE" : "FALSE");
            return "Sequence started";
        } else if (sub.equalsIgnoreCase("PAUSE")) {
            axisManager.pauseSequence();
            Serial.println(">> [WP] Tam dung chuoi quy dao.");
            return "Sequence paused";
        } else if (sub.equalsIgnoreCase("STOP")) {
            axisManager.stopSequence();
            Serial.println(">> [WP] Dung chuoi quy dao.");
            return "Sequence stopped";
        } else if (sub.equalsIgnoreCase("CLEAR")) {
            axisManager.clearWaypoints();
            Serial.println(">> [WP] Da xoa tat ca cac Waypoints.");
            return "Waypoints cleared";
        }
        return "Unknown WP command";
    }

    // 6. Lệnh Cho Từng Khớp: M1..M6 <command>
    if ((cmd.charAt(0) == 'M' || cmd.charAt(0) == 'm') && isdigit(cmd.charAt(1))) {
        uint8_t axis = cmd.charAt(1) - '1';
        if (axis >= NUM_MOTORS) {
            Serial.printf(">> [SERIAL ERROR] Khong co truc M%d (Chi ho tro M1-M6)\n", axis + 1);
            return "Error: Invalid motor index (M1-M6)";
        }

        String sub = cmd.substring(2);
        sub.trim();

        if (sub.length() == 0) return "Motor " + String(axis + 1);

        // M<x> <angle> (vd: M1 45.0)
        if (isdigit(sub.charAt(0)) || (sub.charAt(0) == '-' && sub.length() > 1 && isdigit(sub.charAt(1)))) {
            float ang = sub.toFloat();
            axisManager.setJointTarget(axis, ang);
            Serial.printf(">> [SERIAL M%d] Dat goc muc tieu: %.2f deg\n", axis + 1, ang);
            return "M" + String(axis + 1) + " target: " + String(ang, 2);
        }

        if (sub.startsWith("JOG ") || sub.startsWith("jog ")) {
            float delta = sub.substring(4).toFloat();
            axisManager.jogJoint(axis, delta);
            Serial.printf(">> [SERIAL M%d] Nhich goc: %+.2f deg\n", axis + 1, delta);
            return "M" + String(axis + 1) + " jog: " + String(delta, 2);
        } else if (sub.equalsIgnoreCase("HOME") || sub.equalsIgnoreCase("HOMING")) {
            axisManager.triggerJointHome(axis);
            Serial.printf(">> [SERIAL M%d] Bat dau Homing cung lon...\n", axis + 1);
            return "M" + String(axis + 1) + " homing started";
        } else if (sub.equalsIgnoreCase("ZERO")) {
            axisManager.triggerJointZero(axis);
            Serial.printf(">> [SERIAL M%d] Dat mốc Home 0.00 deg tai vi tri hien tai.\n", axis + 1);
            return "M" + String(axis + 1) + " zero set";
        } else if (sub.equalsIgnoreCase("CALIB")) {
            axisManager.triggerJointCalib(axis);
            Serial.printf(">> [SERIAL M%d] Bat dau Auto Calib LUT 16 diem...\n", axis + 1);
            return "M" + String(axis + 1) + " calibration started";
        } else if (sub.equalsIgnoreCase("CALIB CLEAR")) {
            controllerList[axis]->clearCalibration();
            Serial.printf(">> [SERIAL M%d] Da xoa bang hieu chuan Calib.\n", axis + 1);
            return "M" + String(axis + 1) + " calib cleared";
        } else if (sub.equalsIgnoreCase("AUTODIR") || sub.equalsIgnoreCase("DIR")) {
            axisManager.triggerJointAutoDir(axis);
            Serial.printf(">> [SERIAL M%d] Dang tu dong kiem tra chieu quay...\n", axis + 1);
            return "M" + String(axis + 1) + " autodir started";
        } else if (sub.equalsIgnoreCase("STOP")) {
            axisManager.stopJoint(axis);
            Serial.printf(">> [SERIAL M%d] Dung dong co.\n", axis + 1);
            return "M" + String(axis + 1) + " stopped";
        } else if (sub.startsWith("INVERT ") || sub.startsWith("invert ")) {
            bool inv = sub.substring(7).toInt() == 1;
            controllerList[axis]->setDirInvert(inv);
            Serial.printf(">> [SERIAL M%d] Da cai dat Invert = %s\n", axis + 1, inv ? "TRUE" : "FALSE");
            return "M" + String(axis + 1) + " invert set";
        } else if (sub.startsWith("HOLD ") || sub.startsWith("hold ")) {
            bool h = sub.substring(5).toInt() == 1;
            controllerList[axis]->setClosedLoopHold(h);
            Serial.printf(">> [SERIAL M%d] Da cai dat Closed-Loop Hold = %s\n", axis + 1, h ? "TRUE" : "FALSE");
            return "M" + String(axis + 1) + " hold set";
        } else if (sub.startsWith("SPEED ") || sub.startsWith("speed ")) {
            uint32_t spd = (uint32_t)sub.substring(6).toInt();
            controllerList[axis]->setSpeed(spd);
            Serial.printf(">> [SERIAL M%d] Da cai dat Speed = %u us\n", axis + 1, spd);
            return "M" + String(axis + 1) + " speed set";
        } else if (sub.startsWith("CURR ") || sub.startsWith("curr ")) {
            uint16_t cr = (uint16_t)sub.substring(5).toInt();
            controllerList[axis]->setCurrent(cr);
            Serial.printf(">> [SERIAL M%d] Da cai dat RMS Current = %u mA\n", axis + 1, cr);
            return "M" + String(axis + 1) + " current set";
        } else if (sub.startsWith("HOMECURR ") || sub.startsWith("homecurr ") || sub.startsWith("HCURR ") || sub.startsWith("hcurr ")) {
            int spIdx = sub.indexOf(' ');
            uint16_t hcr = (uint16_t)sub.substring(spIdx + 1).toInt();
            controllerList[axis]->setHomingCurrent(hcr);
            Serial.printf(">> [SERIAL M%d] Da cai dat Homing Current = %u mA\n", axis + 1, hcr);
            return "M" + String(axis + 1) + " homing current set to " + String(hcr) + " mA";
        } else if (sub.startsWith("GEAR ") || sub.startsWith("gear ")) {
            float gr = sub.substring(5).toFloat();
            controllerList[axis]->setGearRatio(gr);
            Serial.printf(">> [SERIAL M%d] Da cai dat Gear Ratio = %.2f : 1\n", axis + 1, gr);
            return "M" + String(axis + 1) + " gear ratio set";
        } else if (sub.startsWith("MAXVEL ") || sub.startsWith("maxvel ")) {
            float mv = sub.substring(7).toFloat();
            axisManager.setMaxVelocity(axis, mv);
            Serial.printf(">> [SERIAL M%d] Da cai dat Max Velocity = %.2f deg/s\n", axis + 1, axisManager.getMaxVelocity(axis));
            return "M" + String(axis + 1) + " max velocity set to " + String(axisManager.getMaxVelocity(axis), 2) + " deg/s";
        } else if (sub.startsWith("STEP ") || sub.startsWith("step ")) {
            String stepArgs = sub.substring(5);
            stepArgs.trim();
            long st = stepArgs.toInt();
            int spdIdx = stepArgs.indexOf(' ');
            uint32_t spd = 0;
            if (spdIdx > 0) spd = (uint32_t)stepArgs.substring(spdIdx + 1).toInt();

            if (st >= 0) axisManager.moveJointRawSteps(axis, true, (uint32_t)st, spd);
            else axisManager.moveJointRawSteps(axis, false, (uint32_t)(-st), spd);
            Serial.printf(">> [SERIAL M%d] Quay %ld buoc raw.\n", axis + 1, st);
            return "M" + String(axis + 1) + " step executed";
        } else if (sub.startsWith("RUN CW") || sub.startsWith("run cw")) {
            axisManager.runJointContinuous(axis, true);
            Serial.printf(">> [SERIAL M%d] Quay lien tuc CW.\n", axis + 1);
            return "M" + String(axis + 1) + " running CW";
        } else if (sub.startsWith("RUN CCW") || sub.startsWith("run ccw")) {
            axisManager.runJointContinuous(axis, false);
            Serial.printf(">> [SERIAL M%d] Quay lien tuc CCW.\n", axis + 1);
            return "M" + String(axis + 1) + " running CCW";
        } else if (sub.equalsIgnoreCase("FREE") || sub.equalsIgnoreCase("DISABLE")) {
            axisManager.setJointDriverEnabled(axis, false);
            Serial.printf(">> [SERIAL M%d] Tha tu do truc.\n", axis + 1);
            return "M" + String(axis + 1) + " disabled";
        } else if (sub.equalsIgnoreCase("ENABLE")) {
            axisManager.setJointDriverEnabled(axis, true);
            Serial.printf(">> [SERIAL M%d] Bat cap nguon Driver.\n", axis + 1);
            return "M" + String(axis + 1) + " enabled";
        }
        return "Unknown motor command";
    }

    Serial.println(">> [SERIAL] Invalid command. Type 'HELP' to see command list.");
    return "Invalid command. Type 'HELP'.";
}

// ==============================================================================
// 6. SETUP & SYSTEM BOOT
// ==============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n==========================================================================");
    Serial.printf("   ESP32-S3 6-AXIS CLOSED-LOOP STEPPER & KINEMATICS CONTROLLER (%s)\n", FW_VERSION);
    Serial.println("   Author: Robotics & Motion Control Engineering Team                     ");
    Serial.println("==========================================================================");

    // 0. Initialize Task Watchdog Timer
    //    arduino-esp32 3.x ships IDF 5.1 where esp_task_wdt_init(uint32_t, bool) is still valid.
    //    (esp_task_wdt_reconfigure/config_t are IDF 5.2+ only)
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);   // timeout, trigger_panic

    // 1. Start AS5600 6-Channel sensor task (PCA9548A on Core 0, 500Hz)
    Serial.println("[INIT] Starting AS5600 6-Channel Task (PCA9548A Core 0, 500Hz)...");
    sensor.begin(SENSOR_TASK_CORE, SENSOR_TASK_PRIORITY, SENSOR_TASK_PERIOD_MS);
    delay(100);

    // 2. Create UART mutexes and start dual hardware serial ports
    uartMutex1 = xSemaphoreCreateMutex();
    uartMutex2 = xSemaphoreCreateMutex();
    if (uartMutex1 == nullptr || uartMutex2 == nullptr) {
        Serial.println("[FATAL] Failed to create UART mutexes! Rebooting...");
        delay(1000);
        ESP.restart();
    }

    motor0.setUartMutex(&uartMutex1);
    motor1.setUartMutex(&uartMutex1);
    motor2.setUartMutex(&uartMutex1);
    motor3.setUartMutex(&uartMutex1);

    motor4.setUartMutex(&uartMutex2);
    motor5.setUartMutex(&uartMutex2);

    Serial.printf("[INIT] Starting Serial1 for M1-M4 (TX: %d, RX: %d)...\n", TX_PIN_1, RX_PIN_1);
    SERIAL_PORT_1.begin(TMC_UART_BAUD, SERIAL_8N1, RX_PIN_1, TX_PIN_1);

    Serial.printf("[INIT] Starting Serial2 for M5-M6 (TX: %d, RX: %d)...\n", TX_PIN_2, RX_PIN_2);
    SERIAL_PORT_2.begin(TMC_UART_BAUD, SERIAL_8N1, RX_PIN_2, TX_PIN_2);

    // 3. Start multi-axis motion control task on Core 1 (100Hz)
    Serial.println("[INIT] Starting 6-Axis Motion Control Task (Core 1, 100Hz)...");
    axisManager.begin(MOTION_TASK_CORE, MOTION_TASK_PRIORITY, MOTION_TASK_PERIOD_MS);

    // 4. Pre-load sample waypoints
    float pHome[NUM_MOTORS]  = { 0.0f,   0.0f,  0.0f, 0.0f,   0.0f, 0.0f };
    float pReady[NUM_MOTORS] = { 0.0f, -30.0f, 45.0f, 0.0f,  30.0f, 0.0f };
    float pReach[NUM_MOTORS] = { 0.0f,  30.0f, 30.0f, 0.0f, -45.0f, 0.0f };
    axisManager.addWaypoint("Home",  pHome,  2.0f, 500);
    axisManager.addWaypoint("Ready", pReady, 2.0f, 500);
    axisManager.addWaypoint("Reach", pReach, 2.0f, 500);

    // 5. Test UART connectivity of all 6 drivers
    testAllUarts();

    // 6. Start Web Server & dual WiFi (AP + STA)
    webServer.setCommandHandler(handleSerialCommand);
    webServer.begin(DEFAULT_AP_SSID, DEFAULT_AP_PASS);

    // 7. Configure ArduinoOTA (port-based wireless firmware upload)
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPort(OTA_PORT);
    // Optional: ArduinoOTA.setPasswordHash(OTA_PASSWORD_HASH);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        // Stop all motion safely before OTA
        axisManager.emergencyStopAll();
        Serial.printf("\n[OTA] Starting update: %s\n", type.c_str());
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete! Rebooting...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint8_t lastPct = 0xFF;
        uint8_t pct = (uint8_t)(progress * 100U / total);
        if (pct != lastPct) {
            lastPct = pct;
            Serial.printf("[OTA] Progress: %u%%\r", pct);
        }
    });
    ArduinoOTA.onError([](ota_error_t error) {
        const char* errStr = "Unknown";
        switch (error) {
            case OTA_AUTH_ERROR:    errStr = "Auth Failed";    break;
            case OTA_BEGIN_ERROR:   errStr = "Begin Failed";   break;
            case OTA_CONNECT_ERROR: errStr = "Connect Failed"; break;
            case OTA_RECEIVE_ERROR: errStr = "Receive Failed"; break;
            case OTA_END_ERROR:     errStr = "End Failed";     break;
        }
        Serial.printf("[OTA] Error[%u]: %s\n", error, errStr);
    });
    ArduinoOTA.begin();
    Serial.printf("[OTA] ArduinoOTA ready on '%s.local' port %d\n", OTA_HOSTNAME, OTA_PORT);

    Serial.println("[READY] 6-axis system ready! Type 'HELP' for command list.");
}

// ==============================================================================
// 7. MAIN LOOP (CORE 1 - ARDUINO LOOP)
// ==============================================================================
void loop() {
    // 1. Handle web browser requests & REST API
    webServer.handle();

    // 2. Handle ArduinoOTA firmware updates
    ArduinoOTA.handle();

    // 3. Read serial input (bounded to 256 chars to prevent heap exhaustion)
    while (Serial.available()) {
        char inChar = (char)Serial.read();
        if (inChar == '\n' || inChar == '\r') {
            if (inputString.length() > 0) stringComplete = true;
        } else {
            if (inputString.length() < 256) {
                inputString += inChar;
            }
        }
    }

    if (stringComplete) {
        handleSerialCommand(inputString);
        inputString = "";
        stringComplete = false;
    }
}