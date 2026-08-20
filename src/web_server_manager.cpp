#include "web_server_manager.h"

WebServerManager::WebServerManager(MultiAxisManager* mam, Sensor* s)
    : axisManager(mam), sensor(s), server(WEB_SERVER_PORT), bootTimeMs(millis()) {}

void WebServerManager::begin(const char* apSsid, const char* apPass) {
    bootTimeMs = millis();

    // 1. Khởi tạo Wi-Fi Dual-Mode (AP + STA)
    Serial.println("[WIFI] Dang khoi tao che do Dual Wi-Fi (AP + STA)...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid, apPass);

    IPAddress apIp = WiFi.softAPIP();
    Serial.println("----------------------------------------------------------");
    Serial.printf(" >> AP WIFI SSID : %s\n", apSsid);
    Serial.printf(" >> AP PASSWORD  : %s\n", apPass);
    Serial.printf(" >> AP WEB UI IP : http://%s\n", apIp.toString().c_str());

    // 2. Nạp và kết nối Wi-Fi nhà nếu có trong NVS Flash
    prefs.begin("wifi", true);
    if (prefs.isKey("ssid")) {
        String savedSSID = prefs.getString("ssid", "");
        String savedPass = prefs.getString("pass", "");
        if (savedSSID.length() > 0) {
            Serial.printf(" >> Dang ket noi vao Wi-Fi nha: '%s'...\n", savedSSID.c_str());
            WiFi.begin(savedSSID.c_str(), savedPass.c_str());
        }
    }
    prefs.end();
    Serial.println("----------------------------------------------------------");

    // 3. Khởi động mDNS
    if (MDNS.begin(DEFAULT_MDNS_HOST)) {
        Serial.printf(" >> mDNS URL     : http://%s.local\n\n", DEFAULT_MDNS_HOST);
    }

    // 4. Đăng ký các routes và khởi động server
    setupRoutes();
    server.begin();
    Serial.printf("[WEB] Web Server da khoi chay tai port %d!\n", WEB_SERVER_PORT);
}

void WebServerManager::setupRoutes() {
    server.on("/", HTTP_GET, [this]() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });

    // Single-Motor routes
    server.on("/api/motor/goto", HTTP_POST, [this]() { handleMotorGoto(); });
    server.on("/api/motor/jog", HTTP_POST, [this]() { handleMotorJog(); });
    server.on("/api/motor/step", HTTP_POST, [this]() { handleMotorStep(); });
    server.on("/api/motor/run", HTTP_POST, [this]() { handleMotorRun(); });
    server.on("/api/motor/stop", HTTP_POST, [this]() { handleMotorStop(); });
    server.on("/api/motor/enable", HTTP_POST, [this]() { handleMotorEnable(); });
    server.on("/api/motor/home", HTTP_POST, [this]() { handleMotorHome(); });
    server.on("/api/motor/zero", HTTP_POST, [this]() { handleMotorZero(); });
    server.on("/api/motor/calib", HTTP_POST, [this]() { handleMotorCalib(); });
    server.on("/api/motor/calib_clear", HTTP_POST, [this]() { handleMotorCalibClear(); });
    server.on("/api/motor/settings", HTTP_POST, [this]() { handleMotorSettings(); });

    // Multi-Axis routes
    server.on("/api/all/goto", HTTP_POST, [this]() { handleAllGoto(); });
    server.on("/api/all/stop", HTTP_POST, [this]() { handleAllStop(); });
    server.on("/api/all/home", HTTP_POST, [this]() { handleAllHome(); });
    server.on("/api/all/zero", HTTP_POST, [this]() { handleAllZero(); });
    server.on("/api/all/enable", HTTP_POST, [this]() { handleAllEnable(); });

    // Inverse Kinematics (IK) routes
    server.on("/api/ik/goto", HTTP_POST, [this]() { handleIKGoto(); });
    server.on("/api/ik/pose", HTTP_GET, [this]() { handleIKPose(); });

    // Waypoint & Trajectory routes
    server.on("/api/waypoint/list", HTTP_GET, [this]() { handleWaypointList(); });
    server.on("/api/waypoint/add", HTTP_POST, [this]() { handleWaypointAdd(); });
    server.on("/api/waypoint/clear", HTTP_POST, [this]() { handleWaypointClear(); });
    server.on("/api/waypoint/start", HTTP_POST, [this]() { handleWaypointStart(); });
    server.on("/api/waypoint/pause", HTTP_POST, [this]() { handleWaypointPause(); });
    server.on("/api/waypoint/stop", HTTP_POST, [this]() { handleWaypointStop(); });

    // Wi-Fi & System routes
    server.on("/api/wifi/scan", HTTP_GET, [this]() { handleWifiScan(); });
    server.on("/api/wifi/save", HTTP_POST, [this]() { handleWifiSave(); });
    server.on("/api/wifi/clear", HTTP_POST, [this]() { handleWifiClear(); });
    server.on("/api/system/reboot", HTTP_POST, [this]() { handleSystemReboot(); });
}

bool WebServerManager::parseAxis(uint8_t& outAxis) {
    if (server.hasArg("axis")) {
        int ax = server.arg("axis").toInt();
        if (ax >= 0 && ax < NUM_MOTORS) {
            outAxis = (uint8_t)ax;
            return true;
        }
    }
    outAxis = 0;
    return false;
}

void WebServerManager::handleStatus() {
    bool wifiConn = (WiFi.status() == WL_CONNECTED);
    String wifiIp = wifiConn ? WiFi.localIP().toString() : "";
    String apIp = WiFi.softAPIP().toString();

    CartesianPose tcpPose = {0};
    axisManager->getCartesianPose(tcpPose);

    uint32_t uptimeSec = (millis() - bootTimeMs) / 1000;
    uint32_t freeHeap = ESP.getFreeHeap();

    String json = "{";
    json += "\"fw_name\":\"" + String(FW_NAME) + "\",";
    json += "\"fw_version\":\"" + String(FW_VERSION) + "\",";
    json += "\"uptime_sec\":" + String(uptimeSec) + ",";
    json += "\"free_heap\":" + String(freeHeap) + ",";
    json += "\"wifi_connected\":" + String(wifiConn ? "true" : "false") + ",";
    json += "\"wifi_ssid\":\"" + (wifiConn ? WiFi.SSID() : "") + "\",";
    json += "\"wifi_rssi\":" + String(wifiConn ? WiFi.RSSI() : 0) + ",";
    json += "\"wifi_ip\":\"" + wifiIp + "\",";
    json += "\"ap_ip\":\"" + apIp + "\",";
    json += "\"all_homed\":" + String(axisManager->areAllHomed() ? "true" : "false") + ",";
    json += "\"all_calibrated\":" + String(axisManager->areAllCalibrated() ? "true" : "false") + ",";
    json += "\"any_running\":" + String(axisManager->isAnyRunning() ? "true" : "false") + ",";
    json += "\"seq_running\":" + String(axisManager->isSequenceRunning() ? "true" : "false") + ",";
    json += "\"seq_idx\":" + String(axisManager->getCurrentWaypointIndex()) + ",";
    json += "\"seq_count\":" + String(axisManager->getWaypointCount()) + ",";

    // Cartesian TCP pose
    json += "\"tcp\":{";
    json += "\"x\":" + String(tcpPose.x, 1) + ",";
    json += "\"y\":" + String(tcpPose.y, 1) + ",";
    json += "\"z\":" + String(tcpPose.z, 1) + ",";
    json += "\"roll\":" + String(tcpPose.roll, 1) + ",";
    json += "\"pitch\":" + String(tcpPose.pitch, 1) + ",";
    json += "\"yaw\":" + String(tcpPose.yaw, 1);
    json += "},";

    // Axes details
    json += "\"axes\":[";
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        MotionController* mc = axisManager->getController(i);
        Motor* m = axisManager->getMotor(i);
        AS5600Diag diag = sensor->getDiagnostics(i);
        TMC2209Diag tmcDiag = m ? m->getDriverStatus() : TMC2209Diag{0};

        if (i > 0) json += ",";
        json += "{";
        json += "\"axis\":" + String(i) + ",";
        json += "\"currentAngle\":" + String(mc ? mc->getCurrentAngle() : 0.0f, 2) + ",";
        json += "\"targetAngle\":" + String(mc ? mc->getTargetAngle() : 0.0f, 2) + ",";
        json += "\"error\":" + String(mc ? mc->getError() : 0.0f, 2) + ",";
        json += "\"rawAngle\":" + String(sensor->getAngle(i), 2) + ",";
        json += "\"absAngle\":" + String(mc ? mc->getCorrectedAngle() : 0.0f, 2) + ",";
        json += "\"accumAngle\":" + String(sensor->getAccumulatedAngle(i), 2) + ",";
        json += "\"turns\":" + String(sensor->getTurnCount(i)) + ",";
        json += "\"isHomed\":" + String((mc && mc->getIsHomed()) ? "true" : "false") + ",";
        json += "\"isCalibrated\":" + String((mc && mc->getIsCalibrated()) ? "true" : "false") + ",";
        json += "\"isRunning\":" + String((m && m->isRunning()) ? "true" : "false") + ",";
        json += "\"driver_enabled\":" + String((m && m->isEnabled()) ? "true" : "false") + ",";
        json += "\"inDeadband\":" + String((mc && mc->isInDeadband()) ? "true" : "false") + ",";
        json += "\"runaway_error\":" + String((mc && mc->isRunawayDetected()) ? "true" : "false") + ",";
        json += "\"uart_ok\":" + String((m && m->isUartOK()) ? "true" : "false") + ",";
        json += "\"driver_version\":" + String(m ? m->getDriverVersion() : 0) + ",";
        json += "\"over_temp\":" + String(tmcDiag.overTemp ? "true" : "false") + ",";
        json += "\"over_temp_warn\":" + String(tmcDiag.overTempWarning ? "true" : "false") + ",";
        json += "\"short_gnd_a\":" + String(tmcDiag.shortToGndA ? "true" : "false") + ",";
        json += "\"short_gnd_b\":" + String(tmcDiag.shortToGndB ? "true" : "false") + ",";
        json += "\"open_load_a\":" + String(tmcDiag.openLoadA ? "true" : "false") + ",";
        json += "\"open_load_b\":" + String(tmcDiag.openLoadB ? "true" : "false") + ",";
        json += "\"sg_result\":" + String(tmcDiag.sgResult) + ",";
        json += "\"totalStroke\":" + String(mc ? mc->getTotalStroke() : 0.0f, 2) + ",";
        json += "\"limitLeft\":" + String(mc ? mc->getLimitLeft() : 0.0f, 2) + ",";
        json += "\"limitRight\":" + String(mc ? mc->getLimitRight() : 0.0f, 2) + ",";
        json += "\"as5600_ok\":" + String(sensor->isSensorOK(i) ? "true" : "false") + ",";
        json += "\"magnet_optimal\":" + String(diag.magnetOptimal ? "true" : "false") + ",";
        json += "\"agc\":" + String(diag.agc) + ",";
        json += "\"magnitude\":" + String(diag.magnitude) + ",";
        json += "\"speed\":" + String(mc ? mc->getSpeed() : DEFAULT_STEP_INTERVAL_US) + ",";
        json += "\"currentSpeed\":" + String(m ? m->getCurrentInterval() : DEFAULT_STEP_INTERVAL_US) + ",";
        json += "\"current\":" + String(mc ? mc->getCurrentMa() : DEFAULT_NORMAL_CURRENT) + ",";
        json += "\"gearRatio\":" + String(mc ? mc->getGearRatio() : DEFAULT_GEAR_RATIO, 2) + ",";
        json += "\"closedLoopHold\":" + String((mc && mc->getClosedLoopHold()) ? "true" : "false") + ",";
        json += "\"dirInvert\":" + String((mc && mc->getDirInvert()) ? "true" : "false");
        json += "}";
    }

    json += "]}";
    server.send(200, "application/json", json);
}

void WebServerManager::handleMotorGoto() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    if (server.hasArg("angle")) {
        float angle = server.arg("angle").toFloat();
        axisManager->setJointTarget(axis, angle);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing angle");
    }
}

void WebServerManager::handleMotorJog() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    if (server.hasArg("delta")) {
        float delta = server.arg("delta").toFloat();
        axisManager->jogJoint(axis, delta);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing delta");
    }
}

void WebServerManager::handleMotorStep() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    bool cw = true;
    if (server.hasArg("dir")) {
        String dir = server.arg("dir");
        if (dir.equalsIgnoreCase("ccw") || dir == "0" || dir.equalsIgnoreCase("false")) {
            cw = false;
        }
    }
    uint32_t steps = 200;
    if (server.hasArg("steps")) {
        steps = (uint32_t)server.arg("steps").toInt();
        if (steps == 0) steps = 1;
    }
    uint32_t speed = 0;
    if (server.hasArg("speed")) {
        speed = (uint32_t)server.arg("speed").toInt();
    }
    axisManager->moveJointRawSteps(axis, cw, steps, speed);
    server.send(200, "text/plain", "OK");
}

void WebServerManager::handleMotorRun() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    bool cw = true;
    if (server.hasArg("dir")) {
        String dir = server.arg("dir");
        if (dir.equalsIgnoreCase("ccw") || dir == "0" || dir.equalsIgnoreCase("false")) {
            cw = false;
        }
    }
    uint32_t speed = 0;
    if (server.hasArg("speed")) {
        speed = (uint32_t)server.arg("speed").toInt();
    }
    axisManager->runJointContinuous(axis, cw, speed);
    server.send(200, "text/plain", "OK");
}

void WebServerManager::handleMotorStop() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->stopJoint(axis);
    server.send(200, "text/plain", "STOPPED");
}

void WebServerManager::handleMotorEnable() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    if (server.hasArg("en")) {
        bool en = (server.arg("en").toInt() == 1 || server.arg("en").equalsIgnoreCase("true"));
        axisManager->setJointDriverEnabled(axis, en);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing en param");
    }
}

void WebServerManager::handleMotorHome() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->triggerJointHome(axis);
    server.send(200, "text/plain", "HOMING_STARTED");
}

void WebServerManager::handleMotorZero() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->triggerJointZero(axis);
    server.send(200, "text/plain", "ZERO_SET");
}

void WebServerManager::handleMotorCalib() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->triggerJointCalib(axis);
    server.send(200, "text/plain", "CALIB_STARTED");
}

void WebServerManager::handleMotorCalibClear() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    MotionController* mc = axisManager->getController(axis);
    if (mc) mc->clearCalibration();
    server.send(200, "text/plain", "CALIB_CLEARED");
}

void WebServerManager::handleMotorSettings() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        server.send(400, "text/plain", "Missing or invalid axis (0..5)");
        return;
    }
    MotionController* mc = axisManager->getController(axis);
    if (mc) {
        if (server.hasArg("hold")) mc->setClosedLoopHold(server.arg("hold").toInt() == 1);
        if (server.hasArg("invert")) mc->setDirInvert(server.arg("invert").toInt() == 1);
        if (server.hasArg("speed")) mc->setSpeed((uint32_t)server.arg("speed").toInt());
        if (server.hasArg("curr")) mc->setCurrent((uint16_t)server.arg("curr").toInt());
        if (server.hasArg("gear")) mc->setGearRatio(server.arg("gear").toFloat());
        if (server.hasArg("lim_min") && server.hasArg("lim_max")) {
            mc->setLimits(server.arg("lim_min").toFloat(), server.arg("lim_max").toFloat());
        }
    }
    server.send(200, "text/plain", "SETTINGS_SAVED");
}

void WebServerManager::handleAllGoto() {
    if (server.hasArg("angles")) {
        String anglesStr = server.arg("angles");
        float targets[NUM_MOTORS] = {0};
        int parsed = 0;

        char buf[128];
        anglesStr.toCharArray(buf, sizeof(buf));
        char* token = strtok(buf, ", ");
        while (token != nullptr && parsed < NUM_MOTORS) {
            targets[parsed++] = atof(token);
            token = strtok(nullptr, ", ");
        }

        float moveTime = 0.0f;
        if (server.hasArg("time")) {
            moveTime = server.arg("time").toFloat();
        }

        axisManager->setTargetAnglesSync(targets, moveTime, true);
        server.send(200, "text/plain", "ALL_GOTO_STARTED");
    } else {
        server.send(400, "text/plain", "Missing angles");
    }
}

void WebServerManager::handleAllStop() {
    axisManager->emergencyStopAll();
    server.send(200, "text/plain", "ALL_STOPPED");
}

void WebServerManager::handleAllHome() {
    axisManager->triggerAllHome();
    server.send(200, "text/plain", "ALL_HOMING_STARTED");
}

void WebServerManager::handleAllZero() {
    axisManager->triggerAllZero();
    server.send(200, "text/plain", "ALL_ZERO_SET");
}

void WebServerManager::handleAllEnable() {
    bool en = true;
    if (server.hasArg("en")) {
        en = (server.arg("en").toInt() == 1 || server.arg("en").equalsIgnoreCase("true"));
    }
    axisManager->setAllDriversEnabled(en);
    server.send(200, "text/plain", "ALL_ENABLE_SET");
}

void WebServerManager::handleIKGoto() {
    if (!server.hasArg("x") || !server.hasArg("y") || !server.hasArg("z")) {
        server.send(400, "text/plain", "Missing x, y, z coordinates");
        return;
    }

    CartesianPose pose = {0};
    pose.x = server.arg("x").toFloat();
    pose.y = server.arg("y").toFloat();
    pose.z = server.arg("z").toFloat();
    pose.roll  = server.hasArg("roll")  ? server.arg("roll").toFloat()  : 0.0f;
    pose.pitch = server.hasArg("pitch") ? server.arg("pitch").toFloat() : 0.0f;
    pose.yaw   = server.hasArg("yaw")   ? server.arg("yaw").toFloat()   : 0.0f;

    float moveTime = server.hasArg("time") ? server.arg("time").toFloat() : 2.0f;

    bool ok = axisManager->setCartesianPose(pose, {100, 0.1f, 0.01f}, moveTime);
    if (ok) {
        server.send(200, "text/plain", "IK_TARGET_ACCEPTED");
    } else {
        server.send(422, "text/plain", "IK_TARGET_UNREACHABLE");
    }
}

void WebServerManager::handleIKPose() {
    CartesianPose pose = {0};
    axisManager->getCartesianPose(pose);

    String json = "{";
    json += "\"x\":" + String(pose.x, 2) + ",";
    json += "\"y\":" + String(pose.y, 2) + ",";
    json += "\"z\":" + String(pose.z, 2) + ",";
    json += "\"roll\":" + String(pose.roll, 2) + ",";
    json += "\"pitch\":" + String(pose.pitch, 2) + ",";
    json += "\"yaw\":" + String(pose.yaw, 2);
    json += "}";
    server.send(200, "application/json", json);
}

void WebServerManager::handleWaypointList() {
    uint8_t count = axisManager->getWaypointCount();
    String json = "[";
    for (uint8_t i = 0; i < count; i++) {
        const Waypoint* wp = axisManager->getWaypoint(i);
        if (wp == nullptr) continue;
        if (i > 0) json += ",";
        json += "{";
        json += "\"index\":" + String(i) + ",";
        json += "\"name\":\"" + String(wp->name) + "\",";
        json += "\"joints\":[" + String(wp->joints[0], 1) + "," + String(wp->joints[1], 1) + "," +
                String(wp->joints[2], 1) + "," + String(wp->joints[3], 1) + "," +
                String(wp->joints[4], 1) + "," + String(wp->joints[5], 1) + "],";
        json += "\"time\":" + String(wp->moveTimeSec, 1) + ",";
        json += "\"dwell\":" + String(wp->dwellTimeMs);
        json += "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void WebServerManager::handleWaypointAdd() {
    String name = server.hasArg("name") ? server.arg("name") : "Point";
    float joints[NUM_MOTORS] = {0};

    if (server.hasArg("joints")) {
        String jStr = server.arg("joints");
        char buf[128];
        jStr.toCharArray(buf, sizeof(buf));
        char* token = strtok(buf, ", ");
        int p = 0;
        while (token != nullptr && p < NUM_MOTORS) {
            joints[p++] = atof(token);
            token = strtok(nullptr, ", ");
        }
    } else {
        axisManager->getAllAngles(joints);
    }

    float moveTime = server.hasArg("time") ? server.arg("time").toFloat() : 2.0f;
    uint16_t dwell = server.hasArg("dwell") ? (uint16_t)server.arg("dwell").toInt() : 500;

    bool added = axisManager->addWaypoint(name.c_str(), joints, moveTime, dwell);
    if (added) {
        server.send(200, "text/plain", "WAYPOINT_ADDED");
    } else {
        server.send(400, "text/plain", "WAYPOINT_LIMIT_REACHED");
    }
}

void WebServerManager::handleWaypointClear() {
    axisManager->clearWaypoints();
    server.send(200, "text/plain", "WAYPOINTS_CLEARED");
}

void WebServerManager::handleWaypointStart() {
    bool loop = server.hasArg("loop") ? (server.arg("loop").toInt() == 1) : false;
    axisManager->startSequence(loop);
    server.send(200, "text/plain", "SEQUENCE_STARTED");
}

void WebServerManager::handleWaypointPause() {
    axisManager->pauseSequence();
    server.send(200, "text/plain", "SEQUENCE_PAUSED");
}

void WebServerManager::handleWaypointStop() {
    axisManager->stopSequence();
    server.send(200, "text/plain", "SEQUENCE_STOPPED");
}

void WebServerManager::handleWifiScan() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void WebServerManager::handleWifiSave() {
    if (server.hasArg("ssid")) {
        String ssid = server.arg("ssid");
        String pass = server.hasArg("pass") ? server.arg("pass") : "";

        prefs.begin("wifi", false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        prefs.end();

        Serial.printf("[WIFI] Da luu Wi-Fi: '%s'. Dang thu ket noi...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());

        server.send(200, "text/plain", "WIFI_SAVED");
    } else {
        server.send(400, "text/plain", "Missing ssid param");
    }
}

void WebServerManager::handleWifiClear() {
    prefs.begin("wifi", false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();
    WiFi.disconnect();
    Serial.println("[WIFI] Da xoa thong tin Wi-Fi da luu.");
    server.send(200, "text/plain", "WIFI_CLEARED");
}

void WebServerManager::handleSystemReboot() {
    server.send(200, "text/plain", "REBOOTING");
    delay(500);
    ESP.restart();
}

void WebServerManager::handle() {
    server.handleClient();
}
