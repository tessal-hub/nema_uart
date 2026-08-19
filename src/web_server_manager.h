#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "multi_axis_manager.h"
#include "sensor.h"
#include "web_ui.h"

class WebServerManager {
private:
    MultiAxisManager* axisManager;
    Sensor* sensor;
    WebServer server;
    Preferences prefs;

    void setupRoutes();
    void handleStatus();

    // Single-Motor Endpoints
    void handleMotorGoto();
    void handleMotorJog();
    void handleMotorStep();
    void handleMotorRun();
    void handleMotorStop();
    void handleMotorEnable();
    void handleMotorHome();
    void handleMotorCalib();
    void handleMotorCalibClear();
    void handleMotorSettings();

    // Multi-Axis Endpoints
    void handleAllGoto();
    void handleAllStop();

    // Wi-Fi Endpoints
    void handleWifiScan();
    void handleWifiSave();
    void handleWifiClear();

    uint8_t parseAxis();

public:
    WebServerManager(MultiAxisManager* mam, Sensor* s);

    void begin(const char* apSsid = "NEMA-STEPPER-CONTROLLER", const char* apPass = "12345678");
    void handle();
};

#endif // WEB_SERVER_MANAGER_H
