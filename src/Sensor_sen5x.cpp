#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <Wire.h>

#include "SensirionI2CSen5x.h"
#include "math.h"
#include "Sensor_sen5x.h"

// Constructor
Sensor_sen5x::Sensor_sen5x() {}

// Member functions
bool Sensor_sen5x::init(TwoWire &i2cBus)
{
    Serial.print("\nInit Sen5x-Device: \n");

    bool init_result = true;
    sen5x.begin(i2cBus);

    uint16_t error = sen5x.deviceReset();

    if (error)
    {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }

    if (!read_sen5x_info()) // update member-properties with sensor-values
    {
        return false;
    }

    error = sen5x.setTemperatureOffsetSimple(tempOffset);
    if (error)
    {
        Serial.print("\nError trying to execute setTemperatureOffsetSimple(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }
    else
    {
        Serial.print("\nTemperature Offset set to ");
        Serial.print(tempOffset);
        Serial.println(" deg. Celsius (SEN54/SEN55 only");
    }

    error = sen5x.startMeasurement();
    if (error)
    {
        Serial.print("Error trying to execute startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }

    return true; // <= init successfull
}

// ----------------------------------------------------------------------------

String Sensor_sen5x::get_sen5x_Data_String()
{
    String sen5x_json;
    // float nan_float_test = std::numeric_limits<float>::quiet_NaN();
    // Serial.print("\n\nget_sen5x_Data_String()\n");
    uint16_t error = sen5x.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
        noxIndex);

    if (error)
    {
        Serial.print("Error trying to execute readMeasuredValues(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return "";
    }
    else
    {
        sen5x_json.concat("\"productName\":\"");
        sen5x_json.concat(productName);
        sen5x_json.concat("\",");
//---
        sen5x_json.concat("\"serialNumber\":\"");
        sen5x_json.concat(serialNumber);
        sen5x_json.concat("\",");

        sen5x_json.concat("\"hw_version\":\"");
        sen5x_json.concat(hw_version);
        sen5x_json.concat("\",");

        sen5x_json.concat("\"fw_version\":\"");
        sen5x_json.concat(fw_version);
        sen5x_json.concat("\",");

        sen5x_json.concat("\"protocol_version\":\"");
        sen5x_json.concat(protocol_version);
        sen5x_json.concat("\",");

//---
        sen5x_json.concat("\"temp_offset\":");
        sen5x_json.concat(tempOffset);
        sen5x_json.concat(",");

        if (!isnan(ambientTemperature))
        {
            sen5x_json.concat("\"temp_C\":");
            sen5x_json.concat(ambientTemperature);
            sen5x_json.concat(",");
        }

        if (!isnan(ambientHumidity))
        {
            sen5x_json.concat("\"hum\":");
            sen5x_json.concat(ambientHumidity);
            sen5x_json.concat(",");
        }

        if (!isnan(ambientTemperature) || !isnan(ambientHumidity))
        {
            sen5x_json.concat("\"hum_abs_g_m3\":");
            sen5x_json.concat(get_absolute_hum_g_m3(ambientTemperature, ambientHumidity));
            sen5x_json.concat(",");
        }

        if (!isnan(vocIndex))
        {
            sen5x_json.concat("\"voc_index\":");
            sen5x_json.concat(vocIndex);
            sen5x_json.concat(",");
        }

        if (!isnan(noxIndex))
        {
            sen5x_json.concat("\"nox_index\":");
            sen5x_json.concat(noxIndex);
            sen5x_json.concat(",");
        }

        if (!isnan(massConcentrationPm1p0))
        {
            sen5x_json.concat("\"massCon_Pm1p0\":");
            sen5x_json.concat(massConcentrationPm1p0);
            sen5x_json.concat(",");
        }

        if (!isnan(massConcentrationPm2p5))
        {
            sen5x_json.concat("\"massCon_Pm2p5\":");
            sen5x_json.concat(massConcentrationPm2p5);
            sen5x_json.concat(",");
        }

        if (!isnan(massConcentrationPm4p0))
        {
            sen5x_json.concat("\"massCon_Pm4p0\":");
            sen5x_json.concat(massConcentrationPm4p0);
            sen5x_json.concat(",");
        }

        if (!isnan(massConcentrationPm10p0))
        {
            sen5x_json.concat("\"massCon_Pm10p0\":");
            sen5x_json.concat(massConcentrationPm10p0);
            sen5x_json.concat(",");
        }
        if (sen5x_json.length() > 1)
        {
            sen5x_json.remove(sen5x_json.length() - 1); // remove last ","  from string
        }
        return sen5x_json;
    }
}

// ----------------------------------------------------------------------------

void Sensor_sen5x::debug_msg(bool value)
{
    debug = value;
}

// ----------------------------------------------------------------------------

String Sensor_sen5x::getSerialNumber()
{
    uint16_t error;
    char errorMessage[256];
    unsigned char serialNumber[32];
    uint8_t serialNumberSize = 32;

    error = sen5x.getSerialNumber(serialNumber, serialNumberSize);
    if (error)
    {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return "";
    }
    else
    {
        if (debug)
        {
            Serial.print("SerialNumber:");
            Serial.println((char *)serialNumber);
        }
        return String((const char *)serialNumber);
    }
}

// ----------------------------------------------------------------------------
bool Sensor_sen5x::set_temp_offset (float value){
    
    this->tempOffset = value;
    
    uint16_t error = sen5x.setTemperatureOffsetSimple(tempOffset);
    
    if (error)
    {
        Serial.print("\nError trying to execute setTemperatureOffsetSimple(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }
    else
    {
        Serial.print("\nTemperature Offset set to ");
        Serial.print(tempOffset);
        Serial.println(" deg. Celsius (SEN54/SEN55 only");
        return true;
    }
}
// ----------------------------------------------------------------------------

bool Sensor_sen5x::read_sen5x_info()
{
    String sen5x_json;
    // float nan_float_test = std::numeric_limits<float>::quiet_NaN();
    // Serial.print("\n\nget_sen5x_Data_String()\n");

    char errorMessage[256];
    uint8_t firmwareMajor;
    uint8_t firmwareMinor;
    bool firmwareDebug;
    uint8_t hardwareMajor;
    uint8_t hardwareMinor;
    uint8_t protocolMajor;
    uint8_t protocolMinor;

    uint16_t error = sen5x.getVersion(firmwareMajor, firmwareMinor, firmwareDebug,
                                      hardwareMajor, hardwareMinor, protocolMajor,
                                      protocolMinor);

    if (error)
    {
        Serial.print("Error trying to get Sensor Infos: \n get_sen5x_Info()\n\n");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }

    this->hw_version = String(hardwareMajor);
    this->hw_version.concat(".");
    this->hw_version.concat(hardwareMinor);

    this->fw_version = String(firmwareMajor);
    this->fw_version.concat(".");
    this->fw_version.concat(firmwareMinor);

    this->protocol_version = String(protocolMajor);
    this->protocol_version.concat(".");
    this->protocol_version.concat(protocolMinor);

    unsigned char product_name[32];

    error = sen5x.getProductName(product_name, sizeof(product_name));

    if (error)
    {
        Serial.print("Error trying to execute getProductName(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }

    productName = String((char *)product_name);

    //---

    unsigned char serial_number[32];

    error = sen5x.getSerialNumber(serial_number, sizeof(serial_number));
    if (error)
    {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }
    serialNumber = String((char *)serial_number);

    //print_sen5x_info();

    return true;
}

// ----------------------------------------------------------------------------

void Sensor_sen5x::print_sen5x_info()
{
    Serial.print("\n\nSensor-Info:\n");

    Serial.print("\nProduct-Name: ");
    Serial.print(productName);

    Serial.print("\nSerial-Number: ");
    Serial.print(serialNumber);

    Serial.print("\nHardware-Version: ");
    Serial.print(hw_version);

    Serial.print("\nFirmware-Version: ");
    Serial.print(fw_version);

    Serial.print("\nProtocol-Version: ");
    Serial.print(protocol_version);

    Serial.print("\nTemp-Offset: ");
    Serial.print(tempOffset);

    Serial.print("\n\n");
}
// ----------------------------------------------------------------------------

float Sensor_sen5x::get_absolute_hum_g_m3(float temp, float rel_hum)
{
    float abs_hum = NAN;

    // https://carnotcycle.wordpress.com/2012/08/04/how-to-convert-relative-humidity-to-absolute-humidity/

    abs_hum = (float)((6.112 * exp(((17.67 * temp) / (243.5 + temp))) *
                       rel_hum * 2.1674) /
                      (273.15 + temp));

    return abs_hum;
}
