#ifndef SENSOR_SEN5X_H
#define SENSOR_SEN5X_H

#include <Arduino.h>
#include "SensirionI2CSen5x.h"

class Sensor_sen5x
{
private:
    bool debug = false;
    char errorMessage[256];

    String productName = "";
    String serialNumber = "";
    String hw_version = "";
    String fw_version = "";
    String protocol_version = "";

    float tempOffset=0.0f;

    float massConcentrationPm1p0;
    float massConcentrationPm2p5;
    float massConcentrationPm4p0;
    float massConcentrationPm10p0;
    float ambientHumidity;
    float ambientTemperature;
    float vocIndex;
    float noxIndex;

public:
    SensirionI2CSen5x sen5x;
    Sensor_sen5x(); 
    
    void debug_msg(bool value);
    bool init(TwoWire& i2cBus);

    bool set_temp_offset (float value);
    String getSerialNumber();
    bool read_sen5x_info();
    void print_sen5x_info();

    String get_sen5x_Data_String();
    float  get_absolute_hum_g_m3(float temp, float rel_hum);

};

#endif