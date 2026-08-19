#include "sensor.h"

Sensor::Sensor() {
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        filtered_angles[i] = 0.0f;
        initialized[i] = false;
        sensor_error[i] = false;
    }
    dataMutex = nullptr;
    i2cMutex = nullptr;
    taskHandle = nullptr;
    taskRunning = false;
}

Sensor::~Sensor() {
    if (taskHandle != nullptr) {
        taskRunning = false;
        vTaskDelete(taskHandle);
    }
    if (dataMutex != nullptr) {
        vSemaphoreDelete(dataMutex);
    }
    if (i2cMutex != nullptr) {
        vSemaphoreDelete(i2cMutex);
    }
}

void Sensor::setPCAChannel(uint8_t channel) {
    Wire.beginTransmission(PCA_ADDR);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

void Sensor::configureAS5600() {
    uint16_t conf = 0;
    conf |= ((uint16_t)AS5600_CONF_PM   & 0x03) << 0;
    conf |= ((uint16_t)AS5600_CONF_HYST & 0x03) << 2;
    conf |= ((uint16_t)AS5600_CONF_OUTS & 0x03) << 4;
    conf |= ((uint16_t)AS5600_CONF_PWMF & 0x03) << 6;
    conf |= ((uint16_t)AS5600_CONF_SF   & 0x03) << 8;
    conf |= ((uint16_t)AS5600_CONF_FTH  & 0x07) << 10;

    uint8_t highByte = (conf >> 8) & 0xFF;
    uint8_t lowByte  = conf & 0xFF;

    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(AS5600_CONF_REG);
    Wire.write(highByte);
    Wire.write(lowByte);
    Wire.endTransmission();
}

uint16_t Sensor::readRaw() {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(AS5600_ANGLE_REG);
    if (Wire.endTransmission(false) != 0) return 0xFFFF;

    if (Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) == 2) {
        uint8_t h = Wire.read();
        uint8_t l = Wire.read();
        return (((uint16_t)h & 0x0F) << 8) | l;
    }
    return 0xFFFF;
}

float Sensor::filter(uint8_t ch, uint16_t raw) {
    float new_angle = (raw / 4096.0f) * 360.0f;

    if (!initialized[ch]) {
        filtered_angles[ch] = new_angle;
        initialized[ch] = true;
        return filtered_angles[ch];
    }

    float delta = new_angle - filtered_angles[ch];
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;

    filtered_angles[ch] += delta * ALPHA;

    if (filtered_angles[ch] < 0.0f) filtered_angles[ch] += 360.0f;
    if (filtered_angles[ch] >= 360.0f) filtered_angles[ch] -= 360.0f;

    return filtered_angles[ch];
}

void Sensor::scanOnce() {
    if (i2cMutex == nullptr) return;
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(SENSOR_I2C_MUTEX_TIMEOUT_MS)) != pdTRUE) return;

    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        setPCAChannel(i);
        uint16_t raw = readRaw();

        if (raw > 4095) {
            sensor_error[i] = true;
            continue;
        }
        sensor_error[i] = false;

        float angle = filter(i, raw);

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(SENSOR_I2C_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            filtered_angles[i] = angle;
            xSemaphoreGive(dataMutex);
        }
    }

    xSemaphoreGive(i2cMutex);
}

void Sensor::taskEntry(void* param) {
    Sensor* self = static_cast<Sensor*>(param);
    self->taskLoop();
}

void Sensor::taskLoop() {
    const TickType_t period = pdMS_TO_TICKS(
        (SENSOR_TASK_PERIOD_MS > 0) ? SENSOR_TASK_PERIOD_MS : 2
    );
    TickType_t lastWake = xTaskGetTickCount();

    while (taskRunning) {
        scanOnce();

        bool allInError = true;
        for (uint8_t i = 0; i < NUM_SENSORS; i++) {
            if (!sensor_error[i]) {
                allInError = false;
                break;
            }
        }

        if (allInError) {
            vTaskDelay(pdMS_TO_TICKS(50));
            lastWake = xTaskGetTickCount();
        } else {
            vTaskDelayUntil(&lastWake, period);
        }
    }

    vTaskDelete(nullptr);
}

void Sensor::begin(uint8_t coreID, uint8_t priority, uint32_t period_ms) {
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);
    Wire.setTimeOut(10);

    dataMutex = xSemaphoreCreateMutex();
    i2cMutex = xSemaphoreCreateMutex();

    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        setPCAChannel(i);
        configureAS5600();
        delay(2);
    }

    taskRunning = true;
    xTaskCreatePinnedToCore(
        taskEntry,
        "SensorScanTask",
        SENSOR_TASK_STACK_SIZE,
        this,
        priority,
        &taskHandle,
        coreID
    );
}

float Sensor::getAngle(uint8_t ch) {
    if (ch >= NUM_SENSORS) return -1.0f;

    float value = filtered_angles[ch];
    if (dataMutex != nullptr && xSemaphoreTake(dataMutex, 0) == pdTRUE) {
        value = filtered_angles[ch];
        xSemaphoreGive(dataMutex);
    }
    return value;
}

bool Sensor::isSensorOK(uint8_t ch) {
    if (ch >= NUM_SENSORS) return false;
    return !sensor_error[ch];
}

AS5600Diag Sensor::getDiagnostics(uint8_t ch) {
    AS5600Diag diag = {0};
    diag.readSuccess = false;

    if (ch >= NUM_SENSORS || i2cMutex == nullptr) return diag;

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        setPCAChannel(ch);

        // 1. Đọc STATUS (0x0B)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_STATUS_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1) == 1) {
            diag.status = Wire.read();
            diag.magnetDetected = (diag.status & 0x20) != 0; // Bit 5: MD
            diag.magnetTooLow   = (diag.status & 0x10) != 0; // Bit 4: ML
            diag.magnetTooHigh  = (diag.status & 0x08) != 0; // Bit 3: MH
            diag.readSuccess = true;
        }

        // 2. Đọc AGC (0x1A)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_AGC_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1) == 1) {
            diag.agc = Wire.read();
        }

        // 3. Đọc MAGNITUDE (0x1B, 0x1C)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_MAG_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) == 2) {
            uint8_t h = Wire.read();
            uint8_t l = Wire.read();
            diag.magnitude = (((uint16_t)h & 0x0F) << 8) | l;
        }

        // 4. Đọc RAW ANGLE (0x0C, 0x0D)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_RAW_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) == 2) {
            uint8_t h = Wire.read();
            uint8_t l = Wire.read();
            diag.rawAngle = (((uint16_t)h & 0x0F) << 8) | l;
        }

        // 5. Đọc ANGLE (0x0E, 0x0F)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_ANGLE_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) == 2) {
            uint8_t h = Wire.read();
            uint8_t l = Wire.read();
            diag.angleReg = (((uint16_t)h & 0x0F) << 8) | l;
        }

        // 6. Đọc ZPOS (0x01, 0x02)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_ZPOS_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) == 2) {
            uint8_t h = Wire.read();
            uint8_t l = Wire.read();
            diag.zpos = (((uint16_t)h & 0x0F) << 8) | l;
        }

        // 7. Đọc MPOS (0x03, 0x04)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_MPOS_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) == 2) {
            uint8_t h = Wire.read();
            uint8_t l = Wire.read();
            diag.mpos = (((uint16_t)h & 0x0F) << 8) | l;
        }

        // 8. Đọc MANG (0x05, 0x06)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_MANG_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2) == 2) {
            uint8_t h = Wire.read();
            uint8_t l = Wire.read();
            diag.mang = (((uint16_t)h & 0x0F) << 8) | l;
        }

        // 9. Đọc ZMCO (0x00)
        Wire.beginTransmission(AS5600_ADDR);
        Wire.write(AS5600_ZMCO_REG);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1) == 1) {
            diag.zmco = Wire.read() & 0x03;
        }

        xSemaphoreGive(i2cMutex);
    }

    return diag;
}