// ----------------------------------------------------------------------------
// ---   OneWire DS18B20 to MQTT   --------------------------------------------
// ----------------------------------------------------------------------------

#include <Arduino.h>
#include <OneWire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include "rw_flash.h"
#include "webserver_settings.h"

// Prototypes
void connect_wifi();
void mqtt_callback(char *topic, byte *message, unsigned int length);
void mqtt_reconnect();
String get_DeviceAddress_as_String(DeviceAddress deviceAddress);
String readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);


WiFiClient espClient;
String mqtt_client_id;

String flash_SSID;
String flash_Password;
String flash_MqttBroker;
String flash_MqttUserName;
String flash_MqttPassword;
String flash_MqttTopic;
String flash_PollInterval;
String flash_TagName;

PubSubClient mqtt_client(espClient);

const char *mqtt_topic;
long lastPoll_ms = 0;
long poll_interval_ms = 6000;

#define TEMPERATURE_PRECISION 12 // 9
#define ONE_WIRE_BUS_PIN 19
#define LED_STATUS 5
OneWire oneWire_Bus(ONE_WIRE_BUS_PIN);
DallasTemperature sensors_OneWireBus(&oneWire_Bus);

int deviceCount = 0;

// DeviceAddress oneWire_DeviceAddress;
// float tempC = 0.0f;

//--------------------------------------------------------------------------------

void setup(void)
{
  Serial.begin(115200);

  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW);

  pinMode(18, OUTPUT); // LED=ON => Configuration Mode with Configuration-Website
  digitalWrite(18, LOW);

  pinMode(21, INPUT); // For dectecting configurtion mode with starting webserver
  // pinMode(23, INPUT_PULLUP);
  delay(150);
  digitalRead(21);
  delay(50);

  if (digitalRead(21))
  {
    Serial.print("\n\nStarting webserver for device-configuration.\n\n");
    digitalWrite(18, LOW);
    // start_configuration_mode();
    start_webserver(); // Start Hotspot with Configuration-Website

    while (1)
    {
      Serial.print("\n\nError: Config-Website exit with error!");
      delay(1000);
    }
  }

  Serial.print("\n\nStart in meassurement mode.\n\n");
  digitalWrite(LED_STATUS, HIGH);

  if (!SPIFFS.begin(true))
  {
    Serial.println("\n\nInternal Flash could not be mounted!\n\nReboot !\n\n\n\n");
    return;
  }

  flash_SSID = readFile(SPIFFS, "/flash_SSID.txt");
  flash_Password = readFile(SPIFFS, "/flash_Password.txt");
  flash_MqttBroker = readFile(SPIFFS, "/flash_MqttBroker.txt");
  flash_MqttUserName = readFile(SPIFFS, "/flash_MqttUserName.txt");
  flash_MqttPassword = readFile(SPIFFS, "/flash_MqttPassword.txt");
  flash_MqttTopic = readFile(SPIFFS, "/flash_MqttTopic.txt");
  flash_PollInterval = readFile(SPIFFS, "/flash_PollInterval.txt");
  flash_TagName = readFile(SPIFFS, "/flash_TagName.txt");

  // int flash_Poll_Interval = 5;

  Serial.print("\nTagName:");
  Serial.print(flash_TagName);
  Serial.print("\nSSID:");
  Serial.print(flash_SSID);
  Serial.print("\nPassword:");
  Serial.print(" * * * * * ");
  Serial.print("\nMqttBroker:");
  Serial.print(flash_MqttBroker);
  Serial.print("\nMqttUserName:");
  Serial.print(flash_MqttUserName);
  Serial.print("\nMqttPassword:");
  Serial.print(" * * * * * ");
  Serial.print("\nMqttTopic:");
  Serial.print(flash_MqttTopic);
  Serial.print("\nPollInterval [s]:");
  Serial.print(flash_PollInterval);

  const char *mqtt_server = flash_MqttBroker.c_str();

  mqtt_client.setServer(mqtt_server, 1883);
  // mqtt_client.setServer("rabu.fritz.box", 1883);
  // mqtt_client.setServer("127.0.0.1", 1883);
  mqtt_client.setKeepAlive(60);
  mqtt_topic = flash_MqttTopic.c_str();

  mqtt_client.setCallback(mqtt_callback);

  try
  {
    poll_interval_ms = 1000 * (flash_PollInterval.toInt());
  }
  catch (...)
  {
    Serial.print("\nError get poll_intervall as int\n\nSet to 6000ms");
    poll_interval_ms = 6000;
  }

  Serial.print("\nPoll_intervall [ms] is: ");
  Serial.print(poll_interval_ms);

}
//-----------------------------------------------------------------------------

void loop(void) // Main-Loop
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connect_wifi();
    mqtt_client_id = WiFi.getHostname();
    Serial.print(mqtt_client_id);
  }

  if (!mqtt_client.connected())
  {
    Serial.println("\nMQTT-Broker is not connected\n");
    mqtt_reconnect();
  }

  mqtt_client.loop();

  // WiFiClient client = server.available();

  long now_ms = millis();
  if (now_ms - lastPoll_ms > poll_interval_ms) // e.q. 5000ms
  {
    lastPoll_ms = now_ms;

    char tempString[8];

    Serial.print("\nIP address: ");
    Serial.print(WiFi.localIP());

    String ip_string = WiFi.localIP().toString();

    Serial.print("\nDevice-Hostname: ");
    Serial.print(WiFi.getHostname());

    String json_string = "{\"DeviceName\":\"";
    json_string.concat(mqtt_client_id);

    json_string.concat("\",\"mac\":\"");
    json_string.concat(WiFi.macAddress());

    json_string.concat("\",\"ip\":\"");
    json_string.concat(ip_string);

    json_string.concat("\",\"TagName\":\"");
    json_string.concat(flash_TagName);

    json_string.concat("\",\"SensorType\":\"DS18B20\"");
    

    sensors_OneWireBus.begin();
    delay(10);

    deviceCount = sensors_OneWireBus.getDeviceCount();

    Serial.print("\n\n");
    Serial.print(deviceCount, DEC);
    Serial.print("  Devices found:\n");

    delay(20);

    for (int i = 0; i < deviceCount; i++)
    {
      DeviceAddress oneWire_DeviceAddress;
      sensors_OneWireBus.getAddress(oneWire_DeviceAddress, i);
      sensors_OneWireBus.setResolution(oneWire_DeviceAddress, TEMPERATURE_PRECISION);
      delay(100);
      sensors_OneWireBus.requestTemperatures();
      delay(100);
      // sensors_OneWireBus.getAddress(oneWire_DeviceAddress, i);

      String deviceAdress_String = get_DeviceAddress_as_String(oneWire_DeviceAddress);

      Serial.print("\nSensor ");
      Serial.print(i + 1);
      Serial.print(":  Read from Device: ");
      Serial.print(deviceAdress_String);
      delay(100);
      float tempC = 0.0f;
      tempC = sensors_OneWireBus.getTempC(oneWire_DeviceAddress);
      if (tempC == -127)
      {
        Serial.print("\n\nError in Temperatur value!\nSensor connection faulty?\n\n");
        return;
      }

      Serial.print("  the Temperature: ");
      Serial.print(tempC);

      json_string.concat(",\"");
      json_string.concat(deviceAdress_String);
      json_string.concat("\":");

      dtostrf(tempC, 1, 2, tempString);

      json_string.concat(tempString);
    }

    json_string.concat("}\0");
       
    bool mqtt_pub_result = mqtt_client.publish(mqtt_topic, json_string.c_str());

    Serial.print("\n\nMQTT-JSON-String: \n\n");
    Serial.print(json_string);
    Serial.print("\n\nTopic:  ");
    Serial.print(mqtt_topic);
    Serial.print("\n\nSend with Result: ");
    Serial.print(mqtt_pub_result);
        
    Serial.print("\n\n- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
  }
} //===========================================================================
//--- End of loop -------------------------------------------------------------
//=============================================================================

void connect_wifi()
{
  delay(500);
  Serial.print("\n\nConnecting to ");

  String flash_SSID = readFile(SPIFFS, "/flash_SSID.txt");
  String flash_Password = readFile(SPIFFS, "/flash_Password.txt");

  const char *ssid = flash_SSID.c_str();
  const char *password = flash_Password.c_str();
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    Serial.print("\n");
    Serial.print("\nWiFi connected");
    Serial.print("\n\nIP address: ");
    Serial.print(WiFi.localIP());
    Serial.print("\n");
  }
}

//-----------------------------------------------------------------------------

void mqtt_callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: \n");
  Serial.print(topic);
  Serial.print("\nMessage: \n");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  //  Serial.print(messageTemp);


  StaticJsonDocument<200> jsonDocument;
  
  DeserializationError deserializationError = deserializeJson(jsonDocument,messageTemp);
   
  if (deserializationError)
  {
    Serial.print("\ndeserializeJson failed with code:\n");
    Serial.print(deserializationError.c_str()); 
    return;
  }
  
  const char* char_text = jsonDocument["new_tagname"];
  flash_TagName = String(char_text);

  Serial.print("New Tagname is:");
  Serial.print(flash_TagName);
  Serial.print("\n");

  writeFile(SPIFFS, "/flash_TagName.txt", flash_TagName.c_str());
  return;

} //--- End of mqtt_callback ---

//-----------------------------------------------------------------------------

void mqtt_reconnect()
{
  // Loop until we're reconnected
  while (!mqtt_client.connected())
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      connect_wifi();
      Serial.print("Attempting MQTT connection...");
    }

    const char *mqtt_user = flash_MqttUserName.c_str();
    const char *mqtt_password = flash_MqttPassword.c_str();

    if (mqtt_client.connect(mqtt_client_id.c_str()))
    {
      Serial.println("MQTT is connected");
      String subscribe_Topic = "sensors/" + WiFi.macAddress();
      Serial.print("\nMQTT-Subscripe to:  ");
      Serial.print(subscribe_Topic);
      Serial.print("\n");
      mqtt_client.subscribe(subscribe_Topic.c_str());
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println("\nTry again in 5 seconds");
      delay(5000);
    }
  }
} // End mqtt_reconnect

//-----------------------------------------------------------------------------

String get_DeviceAddress_as_String(DeviceAddress deviceAddress)
{
  String address_string = "";

  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 0x10)
    {
      address_string += "0";
    }
    address_string += String(deviceAddress[i], HEX);
  }
  address_string.toUpperCase();
  return address_string;
}

//-----------------------------------------------------------------------------
