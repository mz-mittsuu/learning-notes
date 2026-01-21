// sensortemp.cpp
#include <iostream>
#include <fstream>

int main()
{
    std::ifstream ifs("/sys/class/thermal/thermal_zone0/temp");
    if (!ifs) {
        std::cerr << "Failed to open thermal sensor file" << std::endl;
        return 1;
    }

    int temp_milli;
    ifs >> temp_milli;   // 例: 45000

    int sensortemp = temp_milli / 1000;  // ℃（整数）

    std::cout << "Temperature: " << sensortemp << " C" << std::endl;

    return 0;
}
