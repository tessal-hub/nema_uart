#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "config.h"
#include "multi_axis_manager.h"
#include "sensor.h"
#include "web_ui.h"

class WebServerManager {
private:
    MultiAxisManager* axisManager;
    Sensor* sensor;
    WebServer server;
    Preferences prefs;
    unsigned long bootTimeMs;

    void setupRoutes();
    bool parseAxis(uint8_t& outAxis);

    // API Handlers
    void handleStatus();
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
    void handleMotorSettings();

    void handleAllGoto();
    void handleAllStop();
    void handleAllHome();
    void handleAllZero();
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

public:
    WebServerManager(MultiAxisManager* mam, Sensor* s);
    void begin(const char* apSsid = DEFAULT_AP_SSID, const char* apPass = DEFAULT_AP_PASS);
    void handle();
};

#endif // WEB_SERVER_MANAGER_H
