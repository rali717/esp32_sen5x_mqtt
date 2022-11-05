#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include "rw_flash.h"
#include "webserver_settings.h"

AsyncWebServer server(80);

const char *soft_ap_ssid = "Config-DS18B20";
const char *soft_ap_password = "12345678";

const char *hostname = "config";

const char *PARAM_SSID = "flash_SSID";
const char *PARAM_PASSWORD = "flash_Password";
const char *PARAM_MQTT_BROKER = "flash_MqttBroker";
const char *PARAM_MQTT_USERNAME = "flash_MqttUserName";
const char *PARAM_MQTT_PASSWORD = "flash_MqttPassword";
const char *PARAM_MQTT_TOPIC = "flash_MqttTopic";
const char *PARAM_POLL_INTERVAL = "flash_PollInterval";
const char *PARAM_TAGNAME = "flash_TagName";

const char *PARAM_REBOOT = "REBOOT_DEVICE";

// HTML web page: Network-Settings
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>     ESP32-Network-Settings     </title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script>
    function submitMessage() {
      <!--   alert("Saved value to ESP SPIFFS"); -->

      setTimeout(function(){ document.location.reload(false); }, 500);   
    }
  </script>
</head>
  
<body>
<b></b> <b></b> <b></b>
  <h1>ESP32-Network-Settings:</h1> <b></b> 
  <b></b> <b></b> <b></b>
<hr>
<hr>
  <form action="/get" target="hidden-form">
    <h3><b><u>Network-Name (SSID):</u> &nbsp;&nbsp;&nbsp;  <br>%flash_SSID%  </b> <br>
    <input type="text" name="flash_SSID">
    <input type="submit" value="Update" onclick="submitMessage()"> <h3>
  </form>

<h3>
  <form action="/get" target="hidden-form">
    <b><u>Password:</u> &nbsp;&nbsp;&nbsp; <br>%flash_Password%  </b> <br>
    <input type="text" name="flash_Password">
    <input type="submit" value="Update" onclick="submitMessage()">
  </form><br>
</h3>

<hr>

<h3>
  <form action="/get" target="hidden-form">
    <b><u>MQTT-Broker-Name/IP:</u> &nbsp;&nbsp;&nbsp; <br>%flash_MqttBroker%  </b> <br>
    <input type="text" name="flash_MqttBroker">
    <input type="submit" value="Update" onclick="submitMessage()">
  </form><br>
</h3>

<h3>
  <form action="/get" target="hidden-form">
    <b><u>MQTT-Username:</u> &nbsp;&nbsp;&nbsp; <br>%flash_MqttUserName%  </b> <br>
    <input type="text" name="flash_MqttUserName">
    <input type="submit" value="Update" onclick="submitMessage()">
  </form><br>
</h3>

<h3>
  <form action="/get" target="hidden-form">
    <b><u>MQTT-Password:</u> &nbsp;&nbsp;&nbsp; <br>%flash_MqttPassword%  </b> <br>
    <input type="text" name="flash_MqttPassword">
    <input type="submit" value="Update" onclick="submitMessage()">
  </form><br>
</h3>

<h3>
  <form action="/get" target="hidden-form">
    <b><u>MQTT-Topic (eq. sensors/DS18B20):</u> &nbsp;&nbsp;&nbsp; <br>%flash_MqttTopic%  </b> <br>
    <input type="text" name="flash_MqttTopic">
    <input type="submit" value="Update" onclick="submitMessage()">
  </form><br>
</h3>
<hr>

<h3>
  <form action="/get" target="hidden-form">
    <b><u>Poll-Interval [sec]  (>=5):</u> &nbsp;&nbsp;&nbsp; <br>%flash_PollInterval%  </b> <br>
    <input type="text" name="flash_PollInterval">
    <input type="submit" value="Update" onclick="submitMessage()">
  </form><br>
</h3>
<hr>

<h3>
  <form action="/get" target="hidden-form">
    <b><u>TagName:</u> &nbsp;&nbsp;&nbsp; <br>%flash_TagName%  </b> <br>
    <input type="text" name="flash_TagName">
    <input type="submit" value="Update" onclick="submitMessage()">
  </form><br>
</h3>

<hr>
<hr>
<h2> <b> For activating the new settings, please repower the unit or press the "Reboot Device" button! </b> </h2>

<h3>
  <form action="/get" target="hidden-form">

    <button type="submit" value="Button-Text" name="REBOOT_DEVICE" onclick="submitMessage()" >Reboot Device</button>
    
  </form><br>
</h3>

  <iframe style="display:none" name="hidden-form"></iframe>
</body>
</html>)rawliteral";

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

String processor(const String &var)
{
  // Serial.println(var);
  if (var == "flash_SSID")
  {
    return readFile(SPIFFS, "/flash_SSID.txt");
  }
  else if (var == "flash_Password")
  {
    // return readFile(SPIFFS, "/flash_Password.txt");
    return " * * * * * * *";
  }
  else if (var == "flash_MqttBroker")
  {
    return readFile(SPIFFS, "/flash_MqttBroker.txt");
  }
  else if (var == "flash_MqttUserName")
  {
    return readFile(SPIFFS, "/flash_MqttUserName.txt");
  }
  else if (var == "flash_MqttPassword")
  {
    return " * * * * * * *";
    // return readFile(SPIFFS, "/flash_MqttPassword.txt");
  }
  else if (var == "flash_MqttTopic")
  {
    return readFile(SPIFFS, "/flash_MqttTopic.txt");
  }
  else if (var == "flash_PollInterval")
  {
    return readFile(SPIFFS, "/flash_PollInterval.txt");
  }
  else if (var == "flash_TagName")
  {
    return readFile(SPIFFS, "/flash_TagName.txt");
  }

  return String();
}

void start_webserver()
// void setup()
{
  delay(1000);
  Serial.begin(115200);

#ifdef ESP32
  if (!SPIFFS.begin(true))
  {
    Serial.println("\n\nAn Error has occurred while mounting SPIFFS\n\n");
    return;
  }
#endif

  // --- Setup 2 Networkpaths for reaching config-web-site ---
  // --- Hotspot: 192.168.0.1
  // --- Last stored Wifi-Network

  String flash_SSID = readFile(SPIFFS, "/flash_SSID.txt");
  String flash_Password = readFile(SPIFFS, "/flash_Password.txt");
  const char *ssid = flash_SSID.c_str();
  const char *password = flash_Password.c_str();

  IPAddress local_ip(192, 168, 0, 1);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_MODE_APSTA);

  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(soft_ap_ssid, soft_ap_password);

  WiFi.begin(ssid, password); // connect to last stored wifi-network

  delay(500);
  Serial.print("\n\nHostname: \t");
  const String hostname = WiFi.getHostname();
  Serial.print(hostname);
  Serial.print("\nHotspot-Adress:\t");
  Serial.print(WiFi.softAPIP());

  Serial.print("\nWifi-Address: \t");
  Serial.print(WiFi.localIP());
  Serial.print("\n\n");
  // ---

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html, processor); });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String inputMessage;


    if (request->hasParam(PARAM_SSID)) {
      Serial.print("\n\n\n PARAM_SSID -- W R I T E   F I L E");
      inputMessage = request->getParam(PARAM_SSID)->value();
      Serial.print("\n\n\n W R I T E   F I L E");
      writeFile(SPIFFS, "/flash_SSID.txt", inputMessage.c_str());
    }

    else if (request->hasParam(PARAM_PASSWORD)) {
      inputMessage = request->getParam(PARAM_PASSWORD)->value();
      writeFile(SPIFFS, "/flash_Password.txt", inputMessage.c_str());
    }

    else if (request->hasParam(PARAM_MQTT_BROKER)) {
      inputMessage = request->getParam(PARAM_MQTT_BROKER)->value();
      writeFile(SPIFFS, "/flash_MqttBroker.txt", inputMessage.c_str());
    }

    else if (request->hasParam(PARAM_MQTT_USERNAME)) {
      inputMessage = request->getParam(PARAM_MQTT_USERNAME)->value();
      writeFile(SPIFFS, "/flash_MqttUserName.txt", inputMessage.c_str());
    }

    else if (request->hasParam(PARAM_MQTT_PASSWORD)) {
      inputMessage = request->getParam(PARAM_MQTT_PASSWORD)->value();
      writeFile(SPIFFS, "/flash_MqttPassword.txt", inputMessage.c_str());
    }

    else if (request->hasParam(PARAM_MQTT_TOPIC)) {
      inputMessage = request->getParam(PARAM_MQTT_TOPIC)->value();
      writeFile(SPIFFS, "/flash_MqttTopic.txt", inputMessage.c_str());
    }

    else if (request->hasParam(PARAM_POLL_INTERVAL)) {
      inputMessage = request->getParam(PARAM_POLL_INTERVAL)->value();
      writeFile(SPIFFS, "/flash_PollInterval.txt", inputMessage.c_str());
    }

    else if (request->hasParam(PARAM_TAGNAME)) {
      inputMessage = request->getParam(PARAM_TAGNAME)->value();
      writeFile(SPIFFS, "/flash_TagName.txt", inputMessage.c_str());
    }

    else if (request->hasParam(PARAM_REBOOT)) {
      Serial.print("\n\nREBOOT !\n\n");
      ESP.restart();
    }

    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage); });
  server.onNotFound(notFound);
  server.begin();
  // loop2();

  int LED = 18;
  bool led_status = LOW;
  int brightness = 0;
  int fadeAmount = 5;

  int freq = 2048; // 2048;
  const int ledChannel = 0;
  const int resolution = 8;

  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(LED, ledChannel);

  while (1)
  {
    analogWrite(LED, 0);
    Serial.print("\nWeb-Server is maybe alive\n");

    for (int b = 1; b <= (255 + 1); b = b + 10)
    {
      analogWrite(LED, b);
      delay(5);
    }

    analogWrite(LED, 0);
    delay(500);

    for (int b = 1; b <= (255 + 1); b = b + 10)
    {
      analogWrite(LED, b);
      delay(5);
    }

    analogWrite(LED, 0);
    delay(1000);
  }
}
