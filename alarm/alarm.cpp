// alarm/alarm.cpp
#include "alarm.h"
#include <fstream>
#include <string>

const std::string Sensoralarm::LED_DIR = "/sys/class/leds/am62-sk:green:heartbeat/";

bool Sensoralarm::writeFile(const std::string& path, const std::string& value)
{
    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << value;
    return bool(ofs);
}

bool Sensoralarm::writeInt(const std::string& path, int v)
{
    return writeFile(path, std::to_string(v));
}

void Sensoralarm::setOff()
{
    writeFile(LED_DIR + "trigger", "none");
    writeInt(LED_DIR + "brightness", 0);
}

void Sensoralarm::setOn()
{
    writeFile(LED_DIR + "trigger", "none");
    writeInt(LED_DIR + "brightness", 1);
}

void Sensoralarm::blinkSlow()
{
    writeFile(LED_DIR + "trigger", "timer");
    writeInt(LED_DIR + "delay_on", 200);
    writeInt(LED_DIR + "delay_off", 800);
}

void Sensoralarm::blinkFast()
{
    writeFile(LED_DIR + "trigger", "timer");
    writeInt(LED_DIR + "delay_on", 100);
    writeInt(LED_DIR + "delay_off", 100);
}

void Sensoralarm::updateSensortemp(int newsensorTemp)
{
    sensorTemp = newsensorTemp;
    ledStatusChange();
}

int Sensoralarm::getLedStatus()
{
    return ledStatus;
}

int Sensoralarm::getSensorTemp()
{
    return sensorTemp;
}

void Sensoralarm::ledStatusChange()
{
    if (sensorTemp <= 39) {
        ledStatus = 0;
        setOff();
    } else if (sensorTemp <= 59) {
        ledStatus = 1;
        blinkSlow();
    } else if (sensorTemp <= 79) {
        ledStatus = 2;
        blinkFast();
    } else {
        ledStatus = 3;
        setOn();
    }
}
