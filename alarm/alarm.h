// alarm/alarm.h
#pragma once
#include <string>

class Sensoralarm
{
public:
    void updateSensortemp(int newsensorTemp);
    int getLedStatus();
    int getSensorTemp();
private:
    void ledStatusChange();
    static constexpr const char* LED_DIR = "/sys/class/leds/am62-sk:green:heartbeat/";
    bool writeFile(const std::string& path, const std::string& value);
    bool writeInt(const std::string& path, int v);
    void setOff();
    void setOn();
    void blinkSlow();
    void blinkFast();
    int ledStatus = 0;
    int sensorTemp = 20;
};