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
    bool writeFile(const std::string& path, const std::string& value);
    bool writeInt(const std::string& path, int v);

    void setOff();
    void setOn();
    void blinkSlow();
    void blinkFast();

    static const std::string LED_DIR;

    int ledStatus = 0;
    int sensorTemp = 20;
};
