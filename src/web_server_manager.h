#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include "config.h"
#include "multi_axis_manager.h"
#include "sensor.h"
#include "web_ui.h"

// Thread-safe log buffer for Web Console & Serial streaming
void sysLog(const String& msg);
void sysLogf(const char* format, ...);

class WebServerManager {
private:
    MultiAxisManager* axisManager;
    Sensor* sensor;
    WebServer server;
    Preferences prefs;
    unsigned long bootTimeMs;

    void setupRoutes();
    bool parseAxis(uint8_t& outAxis);
    void sendJson(int code, const String& json);
    void sendJsonError(int code, const char* msg);

    // API Handlers
    void handleStatus();
    void handleDiagnostics();
    void handleMotorGoto();
    void handleMotorJog();
    void handleMotorStep();
    void handleMotorRun();
    void handleMotorStop();
    void handleMotorEnable();
    void handleMotorHome();
    void handleMotorZero();
    void handleMotorCalib();
    void handleMotorCalibClear();
    void handleMotorAutoDir();
    void handleMotorSettings();
    void handleMotorMaxVel();

    void handleAllGoto();
    void handleAllStop();
    void handleAllHome();
    void handleAllZero();
    void handleAllAutoDir();
    void handleAllEnable();

    void handleIKGoto();
    void handleIKPose();

    void handleWaypointList();
    void handleWaypointAdd();
    void handleWaypointClear();
    void handleWaypointStart();
    void handleWaypointPause();
    void handleWaypointStop();

    void handleWifiScan();
    void handleWifiSave();
    void handleWifiClear();
    void handleSystemReboot();
    void handleOtaUpload();        // HTTP OTA: POST /api/ota/update
    void handleCli();              // CLI execute: POST /api/cli

    std::function<String(const String&)> commandHandler;

public:
    WebServerManager(MultiAxisManager* axes, Sensor* sns);
    void setCommandHandler(std::function<String(const String&)> handler) { commandHandler = handler; }
    void begin(const char* apSSID = "NEMA-6AXIS-CONTROLLER", const char* apPassword = "");
    void handle();
};

#endif // WEB_SERVER_MANAGER_H
