#include "web_server_manager.h"

WebServerManager::WebServerManager(MultiAxisManager* mam, Sensor* s)
    : axisManager(mam), sensor(s), server(80) {}

void WebServerManager::begin(const char* apSsid, const char* apPass) {
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
    if (MDNS.begin("nema")) {
        Serial.println(" >> mDNS URL     : http://nema.local\n");
    }

    // 4. Đăng ký các routes và khởi động server
    setupRoutes();
    server.begin();
    Serial.println("[WEB] Web Server da khoi chay tai port 80!");
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
    server.on("/api/motor/calib", HTTP_POST, [this]() { handleMotorCalib(); });
    server.on("/api/motor/calib_clear", HTTP_POST, [this]() { handleMotorCalibClear(); });
    server.on("/api/motor/settings", HTTP_POST, [this]() { handleMotorSettings(); });

    // Multi-Axis routes
    server.on("/api/all/goto", HTTP_POST, [this]() { handleAllGoto(); });
    server.on("/api/all/stop", HTTP_POST, [this]() { handleAllStop(); });

    // Wi-Fi routes
    server.on("/api/wifi/scan", HTTP_GET, [this]() { handleWifiScan(); });
    server.on("/api/wifi/save", HTTP_POST, [this]() { handleWifiSave(); });
    server.on("/api/wifi/clear", HTTP_POST, [this]() { handleWifiClear(); });
}

uint8_t WebServerManager::parseAxis() {
    if (server.hasArg("axis")) {
        int ax = server.arg("axis").toInt();
        if (ax >= 0 && ax < NUM_MOTORS) return (uint8_t)ax;
    }
    return 0;
}

void WebServerManager::handleStatus() {
    bool wifiConn = (WiFi.status() == WL_CONNECTED);
    String wifiIp = wifiConn ? WiFi.localIP().toString() : "";
    String apIp = WiFi.softAPIP().toString();

    String json = "{";
    json += "\"wifi_connected\":" + String(wifiConn ? "true" : "false") + ",";
    json += "\"wifi_ip\":\"" + wifiIp + "\",";
    json += "\"ap_ip\":\"" + apIp + "\",";
    json += "\"all_homed\":" + String(axisManager->areAllHomed() ? "true" : "false") + ",";
    json += "\"all_calibrated\":" + String(axisManager->areAllCalibrated() ? "true" : "false") + ",";
    json += "\"any_running\":" + String(axisManager->isAnyRunning() ? "true" : "false") + ",";
    json += "\"axes\":[";

    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        MotionController* mc = axisManager->getController(i);
        Motor* m = axisManager->getMotor(i);
        AS5600Diag diag = sensor->getDiagnostics(i);

        if (i > 0) json += ",";
        json += "{";
        json += "\"axis\":" + String(i) + ",";
        json += "\"currentAngle\":" + String(mc ? mc->getCurrentAngle() : 0.0f, 2) + ",";
        json += "\"targetAngle\":" + String(mc ? mc->getTargetAngle() : 0.0f, 2) + ",";
        json += "\"error\":" + String(mc ? mc->getError() : 0.0f, 2) + ",";
        json += "\"rawAngle\":" + String(sensor->getAngle(i), 2) + ",";
        json += "\"absAngle\":" + String(mc ? mc->getCorrectedAngle() : 0.0f, 2) + ",";
        json += "\"isHomed\":" + String((mc && mc->getIsHomed()) ? "true" : "false") + ",";
        json += "\"isCalibrated\":" + String((mc && mc->getIsCalibrated()) ? "true" : "false") + ",";
        json += "\"isRunning\":" + String((m && m->isRunning()) ? "true" : "false") + ",";
        json += "\"driver_enabled\":" + String((m && m->isEnabled()) ? "true" : "false") + ",";
        json += "\"inDeadband\":" + String((mc && mc->isInDeadband()) ? "true" : "false") + ",";
        json += "\"runaway_error\":" + String((mc && mc->isRunawayDetected()) ? "true" : "false") + ",";
        json += "\"uart_ok\":" + String((m && m->isUartOK()) ? "true" : "false") + ",";
        json += "\"driver_version\":" + String(m ? m->getDriverVersion() : 0) + ",";
        json += "\"totalStroke\":" + String(mc ? mc->getTotalStroke() : 0.0f, 2) + ",";
        json += "\"limitLeft\":" + String(mc ? mc->getLimitLeft() : 0.0f, 2) + ",";
        json += "\"limitRight\":" + String(mc ? mc->getLimitRight() : 0.0f, 2) + ",";
        json += "\"as5600_ok\":" + String(sensor->isSensorOK(i) ? "true" : "false") + ",";
        json += "\"agc\":" + String(diag.agc) + ",";
        json += "\"magnitude\":" + String(diag.magnitude) + ",";
        json += "\"speed\":" + String(mc ? mc->getSpeed() : DEFAULT_STEP_INTERVAL_US) + ",";
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
    uint8_t axis = parseAxis();
    if (server.hasArg("angle")) {
        float angle = server.arg("angle").toFloat();
        axisManager->setJointTarget(axis, angle);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing angle");
    }
}

void WebServerManager::handleMotorJog() {
    uint8_t axis = parseAxis();
    if (server.hasArg("delta")) {
        float delta = server.arg("delta").toFloat();
        axisManager->jogJoint(axis, delta);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing delta");
    }
}

void WebServerManager::handleMotorStep() {
    uint8_t axis = parseAxis();
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
    uint8_t axis = parseAxis();
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
    uint8_t axis = parseAxis();
    axisManager->stopJoint(axis);
    server.send(200, "text/plain", "STOPPED");
}

void WebServerManager::handleMotorEnable() {
    uint8_t axis = parseAxis();
    if (server.hasArg("en")) {
        bool en = (server.arg("en").toInt() == 1 || server.arg("en").equalsIgnoreCase("true"));
        axisManager->setJointDriverEnabled(axis, en);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing en param");
    }
}

void WebServerManager::handleMotorHome() {
    uint8_t axis = parseAxis();
    axisManager->triggerJointHome(axis);
    server.send(200, "text/plain", "HOMING_STARTED");
}

void WebServerManager::handleMotorCalib() {
    uint8_t axis = parseAxis();
    axisManager->triggerJointCalib(axis);
    server.send(200, "text/plain", "CALIB_STARTED");
}

void WebServerManager::handleMotorCalibClear() {
    uint8_t axis = parseAxis();
    MotionController* mc = axisManager->getController(axis);
    if (mc) mc->clearCalibration();
    server.send(200, "text/plain", "CALIB_CLEARED");
}

void WebServerManager::handleMotorSettings() {
    uint8_t axis = parseAxis();
    MotionController* mc = axisManager->getController(axis);
    if (mc) {
        if (server.hasArg("hold")) mc->setClosedLoopHold(server.arg("hold").toInt() == 1);
        if (server.hasArg("invert")) mc->setDirInvert(server.arg("invert").toInt() == 1);
        if (server.hasArg("speed")) mc->setSpeed((uint32_t)server.arg("speed").toInt());
        if (server.hasArg("curr")) mc->setCurrent((uint16_t)server.arg("curr").toInt());
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

void WebServerManager::handle() {
    server.handleClient();
}
