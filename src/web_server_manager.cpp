#include "web_server_manager.h"
#include <StreamString.h>

// Global thread-safe log buffer for Web Console streaming
static const uint8_t LOG_BUFFER_SIZE = 35;
static String logRingBuffer[LOG_BUFFER_SIZE];
static uint32_t globalLogSeq = 0;
static SemaphoreHandle_t logBufferMutex = nullptr;

void sysLog(const String& msg) {
    Serial.println(msg);
    if (logBufferMutex == nullptr) {
        logBufferMutex = xSemaphoreCreateMutex();
    }
    if (logBufferMutex != nullptr && xSemaphoreTake(logBufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        logRingBuffer[globalLogSeq % LOG_BUFFER_SIZE] = msg;
        globalLogSeq++;
        xSemaphoreGive(logBufferMutex);
    }
}

void sysLogf(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    String s = String(buf);
    s.trim();
    if (s.length() > 0) {
        sysLog(s);
    } else {
        Serial.print(buf);
    }
}

WebServerManager::WebServerManager(MultiAxisManager* mam, Sensor* s)
    : axisManager(mam), sensor(s), server(WEB_SERVER_PORT), bootTimeMs(millis()) {}

// ---------------------------------------------------------------------------
// Helpers — centralise CORS + Content-Type headers
// ---------------------------------------------------------------------------
void WebServerManager::sendJson(int code, const String& json) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.sendHeader("Connection", "close");
    server.send(code, "application/json", json);
}

void WebServerManager::sendJsonError(int code, const char* msg) {
    String j = "{\"error\":\"";
    j += msg;
    j += "\"}";
    sendJson(code, j);
}

void WebServerManager::begin(const char* apSsid, const char* apPass) {
    bootTimeMs = millis();

    // 1. Initialise Dual Wi-Fi (AP + STA)
    Serial.println("[WIFI] Initialising dual Wi-Fi (AP + STA)...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid, apPass);

    IPAddress apIp = WiFi.softAPIP();
    Serial.println("----------------------------------------------------------");
    Serial.printf(" >> AP WIFI SSID : %s\n", apSsid);
    // Do NOT log the password to production serial output
    Serial.printf(" >> AP WEB UI IP : http://%s\n", apIp.toString().c_str());

    // 2. Connect to saved home Wi-Fi from NVS (non-blocking — result checked later)
    prefs.begin("wifi", true);
    if (prefs.isKey("ssid")) {
        String savedSSID = prefs.getString("ssid", "");
        String savedPass = prefs.getString("pass", "");
        if (savedSSID.length() > 0) {
            Serial.printf(" >> Connecting to saved Wi-Fi: '%s'...\n", savedSSID.c_str());
            WiFi.begin(savedSSID.c_str(), savedPass.c_str());
        }
    }
    prefs.end();
    Serial.println("----------------------------------------------------------");

    // 3. Start mDNS
    if (MDNS.begin(DEFAULT_MDNS_HOST)) {
        MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        Serial.printf(" >> mDNS URL     : http://%s.local\n\n", DEFAULT_MDNS_HOST);
    }

    // 4. Register routes and start server
    setupRoutes();
    server.begin();
    Serial.printf("[WEB] Web server started on port %d\n", WEB_SERVER_PORT);
}

void WebServerManager::setupRoutes() {
    // CORS preflight
    server.onNotFound([this]() {
        if (server.method() == HTTP_OPTIONS) {
            server.sendHeader("Access-Control-Allow-Origin", "*");
            server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
            server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
            server.send(204);
        } else {
            sendJsonError(404, "Not found");
        }
    });

    server.on("/", HTTP_GET, [this]() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server.on("/api/diagnostics", HTTP_GET, [this]() { handleDiagnostics(); });

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
    server.on("/api/motor/autodir", HTTP_POST, [this]() { handleMotorAutoDir(); });
    server.on("/api/motor/settings", HTTP_POST, [this]() { handleMotorSettings(); });
    server.on("/api/motor/maxvel", HTTP_POST, [this]() { handleMotorMaxVel(); });

    // Multi-Axis routes
    server.on("/api/all/goto", HTTP_POST, [this]() { handleAllGoto(); });
    server.on("/api/all/stop", HTTP_POST, [this]() { handleAllStop(); });
    server.on("/api/all/home", HTTP_POST, [this]() { handleAllHome(); });
    server.on("/api/all/zero", HTTP_POST, [this]() { handleAllZero(); });
    server.on("/api/all/autodir", HTTP_POST, [this]() { handleAllAutoDir(); });
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
    server.on("/api/cli", HTTP_POST, [this]() { handleCli(); });

    // HTTP OTA firmware upload: POST /api/ota/update  (multipart binary)
    server.on("/api/ota/update", HTTP_POST,
        [this]() { handleOtaUpload(); },
        [this]() {
            // Called for each chunk of the uploaded file
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("[OTA-HTTP] Update start: %s (%u bytes)\n",
                              upload.filename.c_str(), upload.totalSize);
                axisManager->emergencyStopAll();
                if (!Update.begin(HTTP_OTA_MAX_SIZE_BYTES, U_FLASH)) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
                Serial.printf("[OTA-HTTP] Written: %u bytes\r", upload.totalSize);
            } else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) {
                    Serial.printf("\n[OTA-HTTP] Update success (%u bytes). Rebooting...\n",
                                  upload.totalSize);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );
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

    String json;
    json.reserve(1200);
    json = "{";
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

    // Axes details - ONLY READ IN-MEMORY CACHED FIELDS (0ms I/O latency)
    json += "\"axes\":[";
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        MotionController* mc = axisManager->getController(i);
        Motor* m = axisManager->getMotor(i);

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
        json += "\"totalStroke\":" + String(mc ? mc->getTotalStroke() : 0.0f, 2) + ",";
        json += "\"limitLeft\":" + String(mc ? mc->getLimitLeft() : 0.0f, 2) + ",";
        json += "\"limitRight\":" + String(mc ? mc->getLimitRight() : 0.0f, 2) + ",";
        json += "\"as5600_ok\":" + String(sensor->isSensorOK(i) ? "true" : "false") + ",";
        json += "\"speed\":" + String(mc ? mc->getSpeed() : DEFAULT_STEP_INTERVAL_US) + ",";
        json += "\"currentSpeed\":" + String(m ? m->getCurrentInterval() : DEFAULT_STEP_INTERVAL_US) + ",";
        json += "\"current\":" + String(mc ? mc->getCurrentMa() : DEFAULT_NORMAL_CURRENT) + ",";
        json += "\"homing_current\":" + String(mc ? mc->getHomingCurrentMa() : DEFAULT_HOMING_CURRENT) + ",";
        json += "\"gearRatio\":" + String(mc ? mc->getGearRatio() : DEFAULT_GEAR_RATIO, 2) + ",";
        json += "\"closedLoopHold\":" + String((mc && mc->getClosedLoopHold()) ? "true" : "false") + ",";
        json += "\"dirInvert\":" + String((mc && mc->getDirInvert()) ? "true" : "false");
        json += "}";
    }

    json += "],\"log_seq\":" + String(globalLogSeq) + ",\"logs\":[";
    if (logBufferMutex != nullptr && xSemaphoreTake(logBufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        uint32_t count = (globalLogSeq < LOG_BUFFER_SIZE) ? globalLogSeq : LOG_BUFFER_SIZE;
        uint32_t startIdx = (globalLogSeq > count) ? (globalLogSeq - count) : 0;
        for (uint32_t k = 0; k < count; k++) {
            if (k > 0) json += ",";
            String l = logRingBuffer[(startIdx + k) % LOG_BUFFER_SIZE];
            l.replace("\\", "\\\\");
            l.replace("\"", "\\\"");
            l.replace("\n", " ");
            l.replace("\r", "");
            json += "\"" + l + "\"";
        }
        xSemaphoreGive(logBufferMutex);
    }
    json += "]}";
    sendJson(200, json);
}

void WebServerManager::handleDiagnostics() {
    String json;
    json.reserve(1024);
    json = "{\"diagnostics\":[";
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
        Motor* m = axisManager->getMotor(i);
        AS5600Diag diag = sensor->getDiagnostics(i);
        TMC2209Diag tmcDiag = m ? m->getDriverStatus() : TMC2209Diag{0};

        if (i > 0) json += ",";
        json += "{";
        json += "\"axis\":" + String(i) + ",";
        json += "\"as5600_ok\":" + String(diag.readSuccess ? "true" : "false") + ",";
        json += "\"magnet_optimal\":" + String(diag.magnetOptimal ? "true" : "false") + ",";
        json += "\"agc\":" + String(diag.agc) + ",";
        json += "\"magnitude\":" + String(diag.magnitude) + ",";
        json += "\"uart_ok\":" + String(tmcDiag.uartOk ? "true" : "false") + ",";
        json += "\"driver_version\":" + String(tmcDiag.driverVersion) + ",";
        json += "\"over_temp\":" + String(tmcDiag.overTemp ? "true" : "false") + ",";
        json += "\"over_temp_warn\":" + String(tmcDiag.overTempWarning ? "true" : "false") + ",";
        json += "\"short_gnd_a\":" + String(tmcDiag.shortToGndA ? "true" : "false") + ",";
        json += "\"short_gnd_b\":" + String(tmcDiag.shortToGndB ? "true" : "false") + ",";
        json += "\"open_load_a\":" + String(tmcDiag.openLoadA ? "true" : "false") + ",";
        json += "\"open_load_b\":" + String(tmcDiag.openLoadB ? "true" : "false") + ",";
        json += "\"sg_result\":" + String(tmcDiag.sgResult);
        json += "}";
    }
    json += "]}";
    sendJson(200, json);
}

void WebServerManager::handleMotorGoto() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    if (server.hasArg("angle")) {
        float angle = server.arg("angle").toFloat();
        axisManager->setJointTarget(axis, angle);
        sendJson(200, "{\"status\":\"OK\"}");
    } else {
        sendJsonError(400, "Missing angle");
    }
}

void WebServerManager::handleMotorJog() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    if (server.hasArg("delta")) {
        float delta = server.arg("delta").toFloat();
        axisManager->jogJoint(axis, delta);
        sendJson(200, "{\"status\":\"OK\"}");
    } else {
        sendJsonError(400, "Missing delta");
    }
}

void WebServerManager::handleMotorStep() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
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
    sendJson(200, "{\"status\":\"OK\"}");
}

void WebServerManager::handleMotorRun() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
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
    sendJson(200, "{\"status\":\"OK\"}");
}

void WebServerManager::handleMotorStop() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->stopJoint(axis);
    sendJson(200, "{\"status\":\"STOPPED\"}");
}

void WebServerManager::handleMotorEnable() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    if (server.hasArg("en")) {
        bool en = (server.arg("en").toInt() == 1 || server.arg("en").equalsIgnoreCase("true"));
        axisManager->setJointDriverEnabled(axis, en);
        sendJson(200, "{\"status\":\"OK\"}");
    } else {
        sendJsonError(400, "Missing en param");
    }
}

void WebServerManager::handleMotorHome() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->triggerJointHome(axis);
    sendJson(200, "{\"status\":\"HOMING_STARTED\"}");
}

void WebServerManager::handleMotorZero() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->triggerJointZero(axis);
    sendJson(200, "{\"status\":\"ZERO_SET\"}");
}

void WebServerManager::handleMotorCalib() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->triggerJointCalib(axis);
    sendJson(200, "{\"status\":\"CALIB_STARTED\"}");
}

void WebServerManager::handleMotorCalibClear() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    MotionController* mc = axisManager->getController(axis);
    if (mc) mc->clearCalibration();
    sendJson(200, "{\"status\":\"CALIB_CLEARED\"}");
}

void WebServerManager::handleMotorAutoDir() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    axisManager->triggerJointAutoDir(axis);
    sendJson(200, "{\"status\":\"AUTODIR_STARTED\"}");
}

void WebServerManager::handleMotorSettings() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    MotionController* mc = axisManager->getController(axis);
    if (mc) {
        if (server.hasArg("hold")) mc->setClosedLoopHold(server.arg("hold").toInt() == 1);
        if (server.hasArg("invert")) mc->setDirInvert(server.arg("invert").toInt() == 1);
        if (server.hasArg("speed")) mc->setSpeed((uint32_t)server.arg("speed").toInt());
        if (server.hasArg("curr")) mc->setCurrent((uint16_t)server.arg("curr").toInt());
        if (server.hasArg("home_curr")) mc->setHomingCurrent((uint16_t)server.arg("home_curr").toInt());
        if (server.hasArg("sg_th")) mc->setStallThreshold((uint8_t)server.arg("sg_th").toInt());
        if (server.hasArg("gear")) mc->setGearRatio(server.arg("gear").toFloat());
        if (server.hasArg("lim_min") && server.hasArg("lim_max")) {
            mc->setLimits(server.arg("lim_min").toFloat(), server.arg("lim_max").toFloat());
        }
    }
    sendJson(200, "{\"status\":\"SETTINGS_SAVED\"}");
}

void WebServerManager::handleMotorMaxVel() {
    uint8_t axis = 0;
    if (!parseAxis(axis)) {
        sendJsonError(400, "Missing or invalid axis (0..5)");
        return;
    }
    if (!server.hasArg("deg_per_sec")) {
        sendJsonError(400, "Missing deg_per_sec parameter");
        return;
    }
    float degPerSec = server.arg("deg_per_sec").toFloat();
    axisManager->setMaxVelocity(axis, degPerSec);
    sendJson(200, "{\"status\":\"MAXVEL_SAVED\",\"axis\":" + String(axis) + ",\"max_vel\":" + String(axisManager->getMaxVelocity(axis), 2) + "}");
}

void WebServerManager::handleAllGoto() {
    float targets[NUM_MOTORS] = {0};
    bool hasTargets = false;
    float moveTime = 0.0f;

    if (server.hasArg("time")) {
        moveTime = server.arg("time").toFloat();
    }

    if (server.hasArg("angles")) {
        String anglesStr = server.arg("angles");
        int parsed = 0;
        char buf[128];
        anglesStr.toCharArray(buf, sizeof(buf));
        char* token = strtok(buf, ", ");
        while (token != nullptr && parsed < NUM_MOTORS) {
            targets[parsed++] = atof(token);
            token = strtok(nullptr, ", ");
        }
        if (parsed == NUM_MOTORS) hasTargets = true;
    } else if (server.hasArg("j1") || server.hasArg("j0")) {
        for (uint8_t i = 0; i < NUM_MOTORS; i++) {
            String key1 = "j" + String(i + 1);
            String key0 = "j" + String(i);
            if (server.hasArg(key1)) {
                targets[i] = server.arg(key1).toFloat();
            } else if (server.hasArg(key0)) {
                targets[i] = server.arg(key0).toFloat();
            }
        }
        hasTargets = true;
    }

    if (hasTargets) {
        axisManager->setTargetAnglesSync(targets, moveTime, true);
        sendJson(200, "{\"status\":\"ALL_GOTO_STARTED\"}");
    } else {
        sendJsonError(400, "Missing angles or j1..j6");
    }
}

void WebServerManager::handleAllStop() {
    axisManager->emergencyStopAll();
    sendJson(200, "{\"status\":\"ALL_STOPPED\"}");
}

void WebServerManager::handleAllHome() {
    axisManager->triggerAllHome();
    sendJson(200, "{\"status\":\"ALL_HOMING_STARTED\"}");
}

void WebServerManager::handleAllZero() {
    axisManager->triggerAllZero();
    sendJson(200, "{\"status\":\"ALL_ZERO_SET\"}");
}

void WebServerManager::handleAllAutoDir() {
    axisManager->triggerAllAutoDir();
    sendJson(200, "{\"status\":\"ALL_AUTODIR_STARTED\"}");
}

void WebServerManager::handleAllEnable() {
    bool en = true;
    if (server.hasArg("en")) {
        en = (server.arg("en").toInt() == 1 || server.arg("en").equalsIgnoreCase("true"));
    }
    axisManager->setAllDriversEnabled(en);
    sendJson(200, "{\"status\":\"ALL_ENABLE_SET\"}");
}

void WebServerManager::handleIKGoto() {
    if (!server.hasArg("x") || !server.hasArg("y") || !server.hasArg("z")) {
        sendJsonError(400, "Missing x, y, z coordinates");
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
        sendJson(200, "{\"status\":\"IK_TARGET_ACCEPTED\"}");
    } else {
        sendJsonError(422, "IK_TARGET_UNREACHABLE");
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
    sendJson(200, json);
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
    sendJson(200, json);
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
        sendJson(200, "{\"status\":\"WAYPOINT_ADDED\"}");
    } else {
        sendJsonError(400, "WAYPOINT_LIMIT_REACHED");
    }
}

void WebServerManager::handleWaypointClear() {
    axisManager->clearWaypoints();
    sendJson(200, "{\"status\":\"WAYPOINTS_CLEARED\"}");
}

void WebServerManager::handleWaypointStart() {
    bool loop = server.hasArg("loop") ? (server.arg("loop").toInt() == 1) : false;
    axisManager->startSequence(loop);
    sendJson(200, "{\"status\":\"SEQUENCE_STARTED\"}");
}

void WebServerManager::handleWaypointPause() {
    axisManager->pauseSequence();
    sendJson(200, "{\"status\":\"SEQUENCE_PAUSED\"}");
}

void WebServerManager::handleWaypointStop() {
    axisManager->stopSequence();
    sendJson(200, "{\"status\":\"SEQUENCE_STOPPED\"}");
}

void WebServerManager::handleWifiScan() {
    // Use async scan to avoid blocking the web server task for 2-5 seconds.
    // First call triggers the scan and returns immediately with an empty list;
    // the client should poll again after ~3 s to get results.
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true /*async*/);
        sendJson(202, "[]");    // 202 Accepted: scan started, retry later
        return;
    }
    if (n == WIFI_SCAN_RUNNING) {
        sendJson(202, "[]");    // Scan still in progress
        return;
    }

    // Scan done — build result
    String json = "[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"";
        json += WiFi.SSID(i);
        json += "\",\"rssi\":";
        json += String(WiFi.RSSI(i));
        json += ",\"enc\":";
        json += String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1 : 0);
        json += "}";
    }
    json += "]";
    WiFi.scanDelete();  // Free scan memory
    sendJson(200, json);
}

void WebServerManager::handleWifiSave() {
    if (!server.hasArg("ssid")) {
        sendJsonError(400, "Missing ssid param");
        return;
    }
    String ssid = server.arg("ssid");
    String pass = server.hasArg("pass") ? server.arg("pass") : "";

    if (ssid.length() == 0 || ssid.length() > 32) {
        sendJsonError(400, "SSID must be 1–32 characters");
        return;
    }

    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    Serial.printf("[WIFI] Saved Wi-Fi: '%s'. Attempting connection...\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());

    sendJson(200, "{\"status\":\"WIFI_SAVED\"}");
}

void WebServerManager::handleWifiClear() {
    prefs.begin("wifi", false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();
    WiFi.disconnect();
    Serial.println("[WIFI] Cleared saved Wi-Fi credentials.");
    sendJson(200, "{\"status\":\"WIFI_CLEARED\"}");
}

void WebServerManager::handleSystemReboot() {
    sendJson(200, "{\"status\":\"REBOOTING\"}");
    delay(500);
    ESP.restart();
}

void WebServerManager::handleOtaUpload() {
    // This handler is called after the upload lambda finishes
    if (Update.hasError()) {
        String err = "{\"status\":\"OTA_FAILED\",\"error\":\"";
        StreamString ss;
        Update.printError(ss);
        err += ss.readString();
        err += "\"}";
        sendJson(500, err);
    } else {
        sendJson(200, "{\"status\":\"OTA_OK\"}");
        delay(1000);
        ESP.restart();
    }
}

void WebServerManager::handleCli() {
    if (server.hasArg("cmd")) {
        String cmd = server.arg("cmd");
        String resp = "OK";
        if (commandHandler) {
            resp = commandHandler(cmd);
        }
        // Escape quotes/newlines for json
        resp.replace("\"", "\\\"");
        resp.replace("\n", "\\n");
        resp.replace("\r", "");
        sendJson(200, "{\"status\":\"OK\",\"response\":\"" + resp + "\"}");
    } else {
        sendJsonError(400, "Missing cmd param");
    }
}

void WebServerManager::handle() {
    server.handleClient();
}
