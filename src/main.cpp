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
#include <Wire.h>
#include "SensirionI2CSen5x.h"
#include "rw_flash.h"
#include "webserver_settings.h"
#include "Sensor_sen5x.h"

const bool FORCE_START_WEBSERVER = false;
// const bool FORCE_START_WEBSERVER=true;

// for sen5x needed  ???
#define MAXBUF_REQUIREMENT 48
#if (defined(I2C_BUFFER_LENGTH) &&                 \
     (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || \
    (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT)
#define USE_PRODUCT_INFO
#endif

#define SDA1 21
#define SCL1 22

#define LED_STATUS 5

// ---  Prototypes  ---

void connect_wifi();
void mqtt_callback(char *topic, byte *message, unsigned int length);
void mqtt_reconnect();
void writeFile(fs::FS &fs, const char *path, const char *message);
String readFile(fs::FS &fs, const char *path);
String get_DeviceAddress_as_String(DeviceAddress deviceAddress);

// Globel variables

int strike_counter = 0;
const int max_strike_counter = 3;

Sensor_sen5x sen5x;

TwoWire I2Cone = TwoWire(0);

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
String flash_TempOffset;

PubSubClient mqtt_client(espClient);

const char *mqtt_topic;
long lastPoll_ms = 0;
long poll_interval_ms = 6000;
float tempOffset = 0.0f;

//--------------------------------------------------------------------------------

void setup(void)
{
  delay(500);
  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }

  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW);

  pinMode(18, OUTPUT); // LED=ON => Configuration Mode with Configuration-Website
  digitalWrite(18, LOW);

  pinMode(21, INPUT); // For dectecting configurtion mode with starting webserver
  // pinMode(23, INPUT_PULLUP);
  delay(150);
  digitalRead(21);
  delay(50);

  if (digitalRead(21) || FORCE_START_WEBSERVER)
  {
    Serial.print("\n\nStarting webserver for device-configuration.\n\n");
    digitalWrite(18, LOW);
    start_webserver(); // Start Hotspot with Configuration-Website

    while (1)
    {
      Serial.print("\n\nError: Config-Website exit with error!");
      delay(1000);
    }
  }

  //---  init sen5x  ----------------------------------------------------------

  Serial.print("\n\nInit Sensor-Module.\n\n");
  digitalWrite(LED_STATUS, HIGH);
  Wire.begin();
  sen5x.init(Wire);

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
  flash_TempOffset = readFile(SPIFFS, "/flash_TempOffset.txt");
  

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
  Serial.print("\nTempOffset:");
  Serial.print(flash_TempOffset);

  const char *mqtt_server = flash_MqttBroker.c_str();

  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(1024);
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

  try
  {
    tempOffset = flash_TempOffset.toFloat();
  }
  catch (...)
  {
    Serial.print("\nError get TempOffset\n\nSet to 0.0°C");
    tempOffset = 0.0f;
  }

  Serial.print("\nTempOffset is: ");
  Serial.print(tempOffset);

  sen5x.set_temp_offset(tempOffset);
  sen5x.print_sen5x_info();
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

  long now_ms = millis();
  if (now_ms - lastPoll_ms > poll_interval_ms) // e.q. 5000ms
  {
    lastPoll_ms = now_ms;

    char tempString[8];
    Serial.print("\nIP-Adress: ");
    Serial.print(WiFi.localIP());
    Serial.print("\n");
    String ip_string = WiFi.localIP().toString();

    if (ip_string == "0.0.0.0")
    {
      Serial.print("\n\nipAdress=\"0.0.0.0\"\n\nRESTART DEVICE !!!\n\n");
      ESP.restart();
    }

    Serial.print("\nDevice-Hostname: ");
    Serial.print(WiFi.getHostname());

    // ---  Build MQTT-JSON-STRING  -------------------------------------------

    String json_string = "{\"networkDevice\":\"";
    json_string.concat(mqtt_client_id);

    json_string.concat("\",\"mac\":\"");
    json_string.concat(WiFi.macAddress());

    json_string.concat("\",\"ip\":\"");
    json_string.concat(ip_string);

    json_string.concat("\",\"tagName\":\"");
    json_string.concat(flash_TagName);
    json_string.concat("\",");

    json_string.concat(sen5x.get_sen5x_Data_String());

    json_string.concat("}\0");

    // ---  Send MQTT-JSON-STRING  --------------------------------------------

    bool mqtt_pub_result = mqtt_client.publish(mqtt_topic, json_string.c_str());

    Serial.print("\n\nMQTT-JSON-String: \n\n");
    Serial.print(json_string);

    Serial.print("\n\nMQTT-Broker: ");
    Serial.print(flash_MqttBroker);

    Serial.print("\n\nTopic:  ");
    Serial.print(mqtt_topic);

    if (mqtt_pub_result)
    {
      Serial.print("\n\nSend result (MQTT):  success !\n");
    }
    else
    {
      Serial.print("\n\nSend FAILED !\n\n");
    }

    Serial.print("\n- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
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
// ---  Handle incomming MQTT-Messages ----------------------------------------
//-----------------------------------------------------------------------------
// {"set_new_topic":"The Topic2","set_new_tempOffset":1.2,"set_new_tagName":"The new TagName"}

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

  StaticJsonDocument<200> jsonDocument;

  DeserializationError deserializationError = deserializeJson(jsonDocument, messageTemp);

  if (deserializationError)
  {
    Serial.print("\ndeserializeJson failed with code:\n");
    Serial.print(deserializationError.c_str());
    return;
  }
  // --- respond on json content ---

  if (jsonDocument.containsKey("set_new_tagName"))
  {
    const char *new_tagName = jsonDocument["set_new_tagName"];
    flash_TagName = String(new_tagName);
    writeFile(SPIFFS, "/flash_TagName.txt", flash_TagName.c_str());
    Serial.print("\nnew Tagname was set: ");
    Serial.print(flash_TagName);
  };

//=

  if (jsonDocument.containsKey("set_new_topic"))
  {
    const char *new_Topic = jsonDocument["set_new_topic"];
    flash_MqttTopic = String(new_Topic);
    writeFile(SPIFFS, "/flash_MqttTopic.txt", flash_MqttTopic.c_str());
    Serial.print("\nnew Topic was set: ");
    Serial.print(flash_MqttTopic);
  };

//=

  if (jsonDocument.containsKey("set_new_tempOffset"))
  {
    Serial.print("\nReceived new TempOffset \n");

    const char* new_tempOffset =  jsonDocument["set_new_tempOffset"];



    //writeFile(SPIFFS, "/flash_TagName.txt", new_tempOffset.c_str());
    delay(3000);
    flash_TempOffset = new_tempOffset;
    Serial.print("\nWrite new TempOffset \n");
    delay(3000);
    // writeFile(SPIFFS, "/flash_TagName.txt", flash_TempOffset.c_str());

    // writeFile(SPIFFS, "/flash_TagName.txt", String(new_tempOffset));

    Serial.print("\nNew TempOffset was set: ");
    delay(3000);
    //Serial.print(flash_TempOffset);
    try
    {
    //  tempOffset = flash_TempOffset.toFloat();
    }
    catch (...)
    {
      Serial.print("\nError get TempOffset\n\nSet to 0.0°C");
      tempOffset = 0.0f;
    }

   
  }
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
      bool result=mqtt_client.subscribe(subscribe_Topic.c_str());
      if (result==false){mqtt_reconnect();}

      String hostname = WiFi.getHostname();
      subscribe_Topic = "sensors/" + hostname;
      Serial.print("\nMQTT-Subscripe to:  ");
      Serial.print(subscribe_Topic);
      Serial.print("\n");
      result=mqtt_client.subscribe(subscribe_Topic.c_str());
      if (result==false){mqtt_reconnect();}
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println("\nTry again in 5 seconds");
      delay(5000);
    }
  }
} //--- End mqtt_reconnect ---
  //-----------------------------------------------------------------------------

/*

{
  "networkDevice": "esp32-11C969",
  "mac": "C4:4F:33:11:C9:69",
  "ip": "192.168.178.210",
  "tagName": "THE TAGNAME",
  "productName": "SEN54",
  "serialNumber": "0042EE441A129F53",
  "hw_version": "4.0",
  "fw_version": "2.0",
  "protocol_version": "1.0",
  "temp_offset": "3.40",
  "temp_C": 20,
  "hum": 60.06,
  "hum_abs_g_m3": 10.38,
  "voc_index": 74,
  "massCon_Pm1p0": 6.6,
  "massCon_Pm2p5": 7.3,
  "massCon_Pm4p0": 7.6,
  "massCon_Pm10p0": 7.8
}

*/