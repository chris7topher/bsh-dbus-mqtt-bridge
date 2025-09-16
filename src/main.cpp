#include <SoftwareSerial.h>
#include "CRC16.h"
// D-Bus logger definitions
#define BSHDBUSRX D5
#define BSHDBUSTX D6

#define BSHDBUS_MIN_FRAME_LENGTH 6
#define BSHDBUS_MAX_FRAME_LENGTH 32
#define BSHDBUS_READ_TIMEOUT 50
#define BSHDBUS_BUFFER_SIZE 128

SoftwareSerial dbus(BSHDBUSRX, BSHDBUSTX);
CRC16 crc(CRC16_XMODEM_POLYNOME, CRC16_XMODEM_INITIAL, CRC16_XMODEM_XOR_OUT, CRC16_XMODEM_REV_IN, CRC16_XMODEM_REV_OUT);

#include <vector>
std::vector<uint8_t> dbusBuffer;
unsigned long dbusLastRead = 0;
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define GET_CHIPID()  (ESP.getChipId())
#include <FS.h>
#include <ArduinoOTA.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <AutoConnect.h>

#define D5 (14) // D-Bus

#define PARAM_FILE      "/param.json"
#define AUX_SETTING_URI "/mqtt_setting"
#define AUX_SAVE_URI    "/mqtt_save"
#define AUX_TEST_URI    "/test"

typedef ESP8266WebServer  WiFiWebServer;

AutoConnect  portal;
AutoConnectConfig config;
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// Function definitions:
void redirect(String uri);

// MQTT settings
String mqttServer = "192.168.178.99";
String mqttPort = "1883";
String mqttUser = "";
String mqttPW = "";


// Declare AutoConnectElements for the /mqtt_setting page
ACStyle(style, "label+input,label+select{position:sticky;left:120px;width:230px!important;box-sizing:border-box;}");
ACText(header, "<h2>MQTT Settings</h2>", "text-align:center;color:#2f4f4f;padding:10px;");
ACInput(inMqttserver, mqttServer.c_str(), "MQTT-Server", "", "e.g. 192.168.172.99");
ACInput(inMqttport, mqttPort.c_str(), "MQTT Port", "", "e.g. 1883 or 1884");
ACInput(inMqttuser, mqttUser.c_str(), "MQTT User", "", "");
ACInput(inMqttpw, mqttPW.c_str(), "MQTT Password", "", "");
ACText(mqttState, "MQTT-State: none", "");
ACSubmit(save, "Save", AUX_SAVE_URI);
ACSubmit(discard, "Discard", "/");
ACElement(newline, "<hr>");

// Declare the custom web page /mqtt_setting containing the AutoConnectElements
AutoConnectAux mqtt_setting(AUX_SETTING_URI, "MQTT Settings", true, {
  style,
  header,
  newline,
  inMqttserver,
  inMqttport,
  newline,
  inMqttuser,
  inMqttpw,
  newline,
  mqttState,
  newline,
  save,
  discard
});


// Declare AutoConnectElements for the /mqtt_save page
ACText(caption2, "<h4>Parameters available as:</h4>", "text-align:center;color:#2f4f4f;padding:10px;");
ACText(parameters);
ACSubmit(back2config, "Back", AUX_SETTING_URI);

// Declare the custom web page /mqtt_save containing the AutoConnectElements
AutoConnectAux mqtt_save(AUX_SAVE_URI, "MQTT Settings", false, {
  caption2,
  parameters,
  back2config
});


bool mqttConnect() {
  String clientId = "SiemensWashingMachine-" + String(GET_CHIPID(), HEX);

  uint8_t retry = 3;
  while (!mqttClient.connected()) {
    if (mqttServer.length() <= 0)
      break;

    mqttClient.setServer(mqttServer.c_str(), atoi(mqttPort.c_str()));

    Serial.println(String("Attempting MQTT broker:") + mqttServer);

    if (mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPW.c_str())) {
      Serial.println("Established:" + clientId);
      mqttState.value = "MQTT-State: <b style=\"color: green;\">Connected</b>";
      return true;
    }
    else {
      Serial.println("Connection failed:" + String(mqttClient.state()));
      mqttState.value = "MQTT-State: <b style=\"color: red;\">Disconnected</b>";
      if (!--retry){
        break;
      }
  // Delay for 3000 ms (commented out)
    }
  }
  return false;
}

String loadParams() {
  File param = SPIFFS.open(PARAM_FILE, "r");
  if (param) {
    if(mqtt_setting.loadElement(param)){
      mqttServer = mqtt_setting["inMqttserver"].value;
      mqttServer.trim();
      mqttPort = mqtt_setting["inMqttport"].value;
      mqttPort.trim();
      mqttUser = mqtt_setting["inMqttuser"].value;
      mqttUser.trim();
      mqttPW = mqtt_setting["inMqttpw"].value;
      mqttPW.trim();
    }
    param.close();
  }
  else
    Serial.println(PARAM_FILE " open failed");
  return String("");
}

// Retrieve the value of each element entered on '/mqtt_setting'.
String saveParams(AutoConnectAux& aux, PageArgument& args) {
  mqttServer = inMqttserver.value;
  mqttServer.trim();

  mqttPort = inMqttport.value;
  mqttPort.trim();

  mqttUser = inMqttuser.value;
  mqttUser.trim();

  mqttPW = inMqttpw.value;
  mqttPW.trim();

  File param = SPIFFS.open(PARAM_FILE, "w");
  mqtt_setting.saveElement(param, { "inMqttserver", "inMqttport", "inMqttuser", "inMqttpw"});
  param.close();

  // Echo back saved parameters to the AutoConnectAux page.
  String echo = "Server: " + mqttServer + "<br>";
  echo += "Port: " + mqttPort + "<br>";
  echo += "User: " + mqttUser + "<br>";
  echo += "Password: " + mqttPW + "<br>";
  parameters.value = echo;
  mqttClient.disconnect();
  return String("");
}

void handleRoot() {
  redirect("/_ac");
}

void redirect(String uri) {
  WiFiWebServer&  webServer = portal.host();
  webServer.sendHeader("Location", String("http://") + webServer.client().localIP().toString() + uri);
  webServer.send(302, "text/plain", "");
  webServer.client().flush();
  webServer.client().stop();
}
void setup() {
  Serial.begin(115200);
  // Initialize D-Bus logger
  dbus.begin(9600); // 8N2 not supported, but works nevertheless
  delay(1000);

  SPIFFS.begin();
  config.title = "Siemens Wasching Machine To Mqtt";
  config.apid = "esp-siemens-washing-machine";
  config.psk  = "";
  config.portalTimeout = 100; // Unsure about the correct value for this parameter
  config.retainPortal = true;
  portal.config(config);
  loadParams();
  // Join the custom Web pages and register /mqtt_save handler
  portal.join({ mqtt_setting, mqtt_save });
  portal.on(AUX_SAVE_URI, saveParams);

  Serial.print("WiFi ");
  if (portal.begin()) {
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
    //Setup OTA Update
    ArduinoOTA.onStart([]() {
      Serial.println("Start OTA");
    });

    ArduinoOTA.onEnd([]() {
      Serial.println("End OTA");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
  }
  else {
    Serial.println("connection failed:" + String(WiFi.status()));
  }

  WiFiWebServer&  webServer = portal.host();
  webServer.on("/", handleRoot);
}

void loop() {
  // D-Bus logger: read and publish every complete message
  // Buffer-based D-Bus parsing (inspired by ESPhome)
  if (dbus.available()) {
    unsigned long currentMillis = millis();
    if (currentMillis - dbusLastRead > BSHDBUS_READ_TIMEOUT) {
      dbusBuffer.clear(); // Clear buffer if timeout exceeded
    }
    dbusLastRead = currentMillis;
    while (dbus.available() && dbusBuffer.size() < BSHDBUS_BUFFER_SIZE) {
      dbusBuffer.push_back(dbus.read());
    }
  }

  size_t lastValidIndex = 0;
  for (size_t pos = 0; pos < dbusBuffer.size(); ) {
    const uint8_t* frameData = dbusBuffer.data() + pos;
    if (dbusBuffer.size() - pos < 1) break;
    const uint16_t frameLen = 4 + frameData[0];
    if ((frameLen < BSHDBUS_MIN_FRAME_LENGTH) ||
        (frameLen > BSHDBUS_MAX_FRAME_LENGTH) ||
        (frameLen > (dbusBuffer.size() - pos))) {
      pos++;
      continue;
    }
    // Check CRC
    CRC16 frameCRC(CRC16_XMODEM_POLYNOME, CRC16_XMODEM_INITIAL, CRC16_XMODEM_XOR_OUT, CRC16_XMODEM_REV_IN, CRC16_XMODEM_REV_OUT);
    for (uint16_t i = 0; i < frameLen; i++) frameCRC.add(frameData[i]);
    if (frameCRC.calc()) {
      pos++;
      continue;
    }
    // Frame is valid!
    char hexBuffer[BSHDBUS_MAX_FRAME_LENGTH * 2 + 1];
    for (uint16_t i = 0; i < frameLen; i++) {
      sprintf(&hexBuffer[i * 2], "%02X", frameData[i]);
    }
    hexBuffer[frameLen * 2] = 0;
    mqttClient.publish("washingmachine/dbus", hexBuffer);
    pos += frameLen;
    // Skip ACK byte
    if ((pos < dbusBuffer.size()) && ((dbusBuffer[pos] == ((frameData[1] & 0xF0) | 0x0A)) || ((dbusBuffer[pos] == 0x1A) && (frameData[1] == 0x0F)))) {
      pos++;
    }
    lastValidIndex = pos;
  }
  if (lastValidIndex) {
    dbusBuffer.erase(dbusBuffer.begin(), dbusBuffer.begin() + lastValidIndex);
  } else if (dbusBuffer.size() > BSHDBUS_MAX_FRAME_LENGTH) {
    dbusBuffer.erase(dbusBuffer.begin(), dbusBuffer.end() - BSHDBUS_MAX_FRAME_LENGTH);
  }
  if (WiFi.status() == WL_CONNECTED) {
    // The following actions require WiFi connection
    ArduinoOTA.handle();
    if (!mqttClient.connected()) {
      mqttConnect();
    }
    mqttClient.loop();
  }

  portal.handleClient();
}
