#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// Defining UART pins used for Arduino Uno communication
#define RX1_PIN 16 // Connects to the Arduino Uno TX divider circuit
#define TX1_PIN 17 // Connects to the Arduino Uno RX divider circuit

// Wi-Fi network credentials - ESP32 and Laptop should be on same network
const char* WIFI_SSID = "CyberSecurity Competition";
const char* WIFI_PASSWORD = "Cybersec2025";

// Global variables to hold parsed data from Arduino
float temperature = 0.0;
float humidity = 0.0;
int airQuality = 0;

// Actuator and environemntal values
String airQualityStatus = "Unknown";
String controlMode = "AUTO";
String servoMode = "UNKNOWN";
int servoAngle = 0;

// Defining Mosquitto MQTT broker address and port
// Use the Windows Laptop IPv4 address to relay ESP data to Mosquitto through MQTT
const char* MQTT_BROKER = "192.168.1.110";
const int MQTT_PORT = 8883;

// Username and password for MQTT authentication
const char* MQTT_USERNAME = "esp32_device";
const char* MQTT_PASSWORD = "#GR295tn";

// The MQTT topic that Ditto connection is already subscribed to
// ESP32 publishes sensor data using telemtry
const char* MQTT_TELEMETRY_TOPIC = "indoor/ditto";

// ESP32 receives Ditto commands using command
const char* MQTT_COMMAND_TOPIC = "indoor/commands";

// Creating Wi-Fi + MQTT clients objects
WiFiClientSecure secureClient;
MqttClient mqttClient(secureClient);

// Adding ca.crt public certificate
const char* ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDEzCCAfugAwIBAgIUQMmDSwixY0nQLMbTKpZ1yHCIF3MwDQYJKoZIhvcNAQEL
BQAwGTEXMBUGA1UEAwwOSW9ULURULVJvb3QtQ0EwHhcNMjYwODIwMTczNjAwWhcN
MzYwODE3MTczNjAwWjAZMRcwFQYDVQQDDA5Jb1QtRFQtUm9vdC1DQTCCASIwDQYJ
KoZIhvcNAQEBBQADggEPADCCAQoCggEBAMSGu5vdDI6auXNJF2TpPikS73vVXOTe
Vyd/k24CzfsdC9S35zGXcdbiEqhWSwK4Vpi7ySs2Gtb0UBv0nJCn+pMbSBTVnY7L
Ygs+q7HXu82ppzL4GYksw5nNoPmVNtjYdMJB3/KmmykYcTKXxZzjS1R1ZxrixXoQ
lIvcmBPOxOlZj3B7vfvX9bTgNfZXLHxVfiCm26va/1lfQEYboVWumcQtVW454G+X
kEYAm3FG6q57sjjpYkleVciPtJmffnHIvDrZL/5P1adRVs2R8gaA2aJ6wz5hU7uZ
a/3kQleMIx/sjkHvPMWHGz2oTtuYtvoiHTJZjrJcT5RSQX9abtouhE8CAwEAAaNT
MFEwHQYDVR0OBBYEFGltSbLZcIvhy5AfWl6sBprlhHaHMB8GA1UdIwQYMBaAFGlt
SbLZcIvhy5AfWl6sBprlhHaHMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL
BQADggEBACEAOwudVSA/O4dvpqeRmZ2o1BGpQU7bleyUjAEV5PaPvcpSVia4eSRf
3w7Uo243lkNSKMZaXTQCQ61H5Y/IYct+k2FQDN8CiirvLRqL2k/pe8GgjVNNLN2i
zreOCT2B5pDiwwEPnz11kROcN6uPVqmG5roTo90Fy1LW+cpl9zjXGw5cuCamICD+
eqzyJcJYTce3axsL7vZCyikiq81D5Z28twnVm8mbc1OOYDOJQUJmYOm0VB6so/N4
u+yI763CSPak1vGVEKVk9YoOZimZE3dePyZgb+B2S8wZgkUbNoDE8OGrcTb5d1tE
IlwDfNndn/nicDrtPSB+UmfWThsXGL0=
-----END CERTIFICATE-----
)EOF";

// FUNCTION: CONNECT ESP32 TO WI-FI
void connectWiFi() {

  // Display target Wi-Fi network
  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  // Configure ESP32 as a Wi-Fi station
  WiFi.mode(WIFI_STA);
  // Begin connection to configured Wi-Fi network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Track connection attempts
  int attempts = 0;

  // Retry Wi-Fi connection for a limited number of attempts
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  // Confirm successful Wi-Fi connection
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("Wi-Fi connected successfully!");

    // Display assigned ESP32 IP address
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    // Report Wi-Fi connection failure
    Serial.println();
    Serial.println("Wi-Fi connection failed.");
  }
}


// FUNCTION: CONNECT TO MQTT BROKER
void connectMQTT() {

  // Display target MQTT broker
  Serial.println();
  Serial.print("Connecting to MQTT broker: ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  // Retry connection until MQTT broker becomes available
  while (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
    Serial.print("MQTT connection failed. Error code: ");

    Serial.println(mqttClient.connectError());

    Serial.println("Retrying MQTT in 2 seconds...");

    delay(2000);
  }

  // Confirm successful MQTT connection
  Serial.println("MQTT broker connected successfully!");

  // Subscribe to command topic from Ditto
  Serial.print("Subscribing to command topic: ");
  Serial.println(MQTT_COMMAND_TOPIC);

  int subscribeResult = mqttClient.subscribe(MQTT_COMMAND_TOPIC);

  // Confirm successful topic subscription
  if (subscribeResult) {
    Serial.println("Command topic subscribed successfully!");
  }
  else {
    Serial.println("ERROR: Command topic subscription failed!");
  }
}


// FUNCTION: CREATE & PUBLISH DITTO JSON
void publishToDitto() {

  // Create JSON payload using Eclipse Ditto Protocol format
  String payload;
  // Reserve memory to reduce repeated String allocation
  payload.reserve(700);

  // Build Ditto telemetry message containing environment and actuator state
  payload =
    "{"
      "\"topic\":\"org.environment/indoor-monitor-001/things/twin/commands/modify\","
      "\"headers\":{"
        "\"content-type\":\"application/json\","
        "\"response-required\":false"
      "},"

      "\"path\":\"/features\","
      "\"value\":{"
        "\"environment\":{"
          "\"properties\":{"
          "\"temperature\":" + String(temperature, 2) + ","

          "\"humidity\":" + String(humidity, 2) + ","

          "\"airQualityRaw\":" + String(airQuality) + ","

          "\"airQualityStatus\":\"" + airQualityStatus + "\""
          "}"
        "},"

        "\"ventilation\":{"
          "\"properties\":{"

          "\"controlMode\":\"" + controlMode + "\","
          
          "\"servoAngle\":" + String(servoAngle) + ","

          "\"fanStatus\":\"" + servoMode + "\""
          "}"
        "}"
      "}"
    "}";

  
  // Display generated JSON payload
  Serial.println();
  Serial.println("===== MQTT JSON Payload =====");
  Serial.println(payload);
  Serial.println("=============================");

  // Start publishing message to telemetry topic
  mqttClient.beginMessage(MQTT_TELEMETRY_TOPIC);

  // Write JSON payload into MQTT message
  mqttClient.print(payload);

  // Complete MQTT publish operation
  int result =
    mqttClient.endMessage();

  // Confirm successful MQTT publication
  if (result) {
    Serial.println("MQTT message published successfully!");
  }
  else {
    Serial.println("ERROR: MQTT publish failed!");
  }
}


// FUNCTION: PARSE DITTO COMMAND AND FORWARD TO ARDUINO
void forwardCommandToArduino(String payload) {

  // Locate and extract control mode field in command payload
  int modeKey = payload.indexOf("\"mode\"");
  
  // Stop if command does not contain a mode
  if (modeKey == -1) {
    Serial.println("ERROR: Command has no mode");
    return;
  }

  // Locate mode value boundaries inside JSON payload
  int modeColon = payload.indexOf(':', modeKey);
  int modeQuoteStart = payload.indexOf('"', modeColon + 1);
  int modeQuoteEnd = payload.indexOf('"', modeQuoteStart + 1);

  // Validate mode field formatting
  if (modeColon == -1 || modeQuoteStart == -1 || modeQuoteEnd == -1) {
    Serial.println("ERROR: Invalid mode format");
    return;
  }

  // Extract control mode value from JSON payload
  String mode = payload.substring(modeQuoteStart + 1, modeQuoteEnd);

  // Remove whitespace and standardise mode text
  mode.trim();
  mode.toUpperCase();


  // Handle automatic control command
  if (mode == "AUTO") {
    // Forward AUTO command to Arduino Uno
    Serial1.println("SERVO,AUTO");
    Serial.println("Forwarded to Arduino: SERVO,AUTO");
    return;
  }

  // Handle manual control command
  if (mode == "MANUAL") {
    // Locate requested servo angle in JSON payload
    int angleKey = payload.indexOf("\"angle\"");
    if (angleKey == -1) {
      Serial.println("ERROR: MANUAL command has no angle");
      return;
    }

    // Locate start and end of angle value
    int angleColon = payload.indexOf(':', angleKey);
    int angleEnd = payload.indexOf(',', angleColon + 1);

    // If angle is the last JSON value
    if (angleEnd == -1) {
      angleEnd = payload.indexOf('}', angleColon + 1);
    }

    // Extract servo angle as text
    String angleString = payload.substring(angleColon + 1, angleEnd);
    angleString.trim();

    // Convert requested servo angle to integer
    int requestedAngle = angleString.toInt();

    // Validate servo angle against physical limits
    if (requestedAngle < 0 || requestedAngle > 180) {
      Serial.println("ERROR: Servo angle must be 0-180");
      return;
    }

    // Forward validated manual servo command to Arduino
    // Example:SERVO,MANUAL,90
    Serial1.print("SERVO,MANUAL,");
    Serial1.println(requestedAngle);

    Serial.print("Forwarded to Arduino: SERVO,MANUAL,");
    Serial.println(requestedAngle);

    return;
  }
  Serial.println("ERROR: Unknown control mode");
}


// FUNCTION: RECEIVE MQTT COMMAND FROM DITTO
void onMqttMessage(int messageSize) {

  Serial.println();
  Serial.println("===== MQTT COMMAND RECEIVED =====");

  // Show MQTT topic on which command was received
  Serial.print("Topic: ");
  Serial.println(mqttClient.messageTopic());

  // Create buffer for complete MQTT command payload
  String commandPayload = "";

  // Read all bytes from incoming MQTT message
  while (mqttClient.available()) {

    char c = (char)mqttClient.read();

    commandPayload += c;
  }

  commandPayload.trim();

  // Display received MQTT command
  Serial.print("Payload: ");
  Serial.println(commandPayload);

  // Display received MQTT message size
  Serial.print("Message size: ");
  Serial.println(messageSize);

  Serial.println("=================================");
  
  // Send command onwards to Arduino
  forwardCommandToArduino(commandPayload);

  Serial.println();
}


// FUNCTION: Time Sync
void syncTimeForTLS() {

  Serial.println("Synchronizing time for TLS certificate validation...");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);

  unsigned long startTime = millis();

  while (now < 1700000000 && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println();

  if (now >= 1700000000) {
    Serial.println("Time synchronized successfully.");

  }
  else {
    Serial.println("WARNING: Time synchronization failed.");
  }
}


//FUNCTION: SETUP
void setup() {

  // Start USB serial communication for debugging
  Serial.begin(115200);

  delay(1000);
  
  // Initialize UART Communication Serial 1 to talk to Arduino Uno at 9600 baud
  Serial1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);

  // Display ESP32 gateway startup message
  Serial.println();
  Serial.println("ESP32-S3 Environmental Receiver Starting...");

  // Connect ESP32 to Wi-Fi network
  connectWiFi();

  // Confirm UART and Wi-Fi receiver availability
  Serial.println("ESP32-S3 Environmental Receiver Online...");
  
  // Make sure certificate validity can be checked
  syncTimeForTLS();

  // Trust our IoT-DT Root CA
  secureClient.setCACert(ROOT_CA);

  // Assign unique MQTT client identifier
  mqttClient.setId("esp32-indoor-monitor-001");

  // Usename and password assigned to MQTT client for broker authentication
  mqttClient.setUsernamePassword(MQTT_USERNAME,MQTT_PASSWORD);

  // Increase payload capacity for Ditto telemetry JSON Messages
  mqttClient.setTxPayloadSize(1024);

  // Register callback function for incoming MQTT commands
  mqttClient.onMessage(onMqttMessage);

  // Connect ESP32 to Mosquitto MQTT broker
  connectMQTT();
  Serial.println();
  Serial.println("ESP32-S3 Environmental MQTT Gateway Online!");
}

//FUNCTION: LOOP
void loop() {
  // MQTT requires poll() to be called regularly
  // process incoming MQTT traffic and maintain the MQTT connection
  mqttClient.poll();

  // Reconnect Wi-Fi if lost
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection lost. Reconnecting...");
    connectWiFi();
  }

  // MQTT reconnect
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected!");
    connectMQTT();
  }

  // Process telemetry when Arduino UART data becomes available
  if (Serial1.available() > 0) {

    // Read one complete CSV telemetry packet
    String dataPacket = Serial1.readStringUntil('\n');
    dataPacket.trim(); // Remove whitespace

    // Locate separators in Arduino telemetry packet of the CSV String
    // 7-field Arduino packet: temperature,humidity,airQuality,airQualityStatus, controlMode,servoAngle,servoMode
    int comma1 = dataPacket.indexOf(',');
    int comma2 = dataPacket.indexOf(',', comma1 + 1);
    int comma3 = dataPacket.indexOf(',', comma2 + 1);
    int comma4 = dataPacket.indexOf(',', comma3 + 1);
    int comma5 = dataPacket.indexOf(',', comma4 + 1);
    int comma6 = dataPacket.indexOf(',', comma5 + 1);

    // Continue only when all expected CSV fields are present
    if (comma1 != -1 && comma2 != -1 && comma3 != -1 && comma4 != -1 && comma5 != -1 && comma6 != -1) {
      
      // Split substrings based on comma positions
      String tStr = dataPacket.substring(0, comma1); //Extract temperature field
      String hStr = dataPacket.substring(comma1 + 1, comma2); //Extract humidity field
      String aqStr = dataPacket.substring(comma2 + 1, comma3); // Extract raw air quality field
      String aqStatusStr = dataPacket.substring(comma3 + 1, comma4); // Extract air quality status field
      String cmStr = dataPacket.substring(comma4 + 1, comma5); // Extract actuator control mode field
      String saStr = dataPacket.substring(comma5 + 1, comma6); // Extract servo angle field
      String smStr = dataPacket.substring(comma6 + 1); // Extract servo operating state field

      // Convert strings to numeric data types
      temperature = tStr.toFloat();
      humidity = hStr.toFloat();
      airQuality = aqStr.toInt();
      servoAngle = saStr.toInt();

      // Store text-based environmental and actuator states
      airQualityStatus = aqStatusStr;
      controlMode = cmStr;
      servoMode = smStr;

      // Display parsed results to the ESP32 Serial Monitor
      Serial.println("=== New Data Received from Arduino Uno ===");
      Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" °C");
      Serial.print("Humidity:    "); Serial.print(humidity); Serial.println(" %");
      Serial.print("Air Quality: "); Serial.println(airQuality);
      Serial.print("Air Quality Status: "); Serial.println(airQualityStatus);
      Serial.print("Control Mode: "); Serial.println(controlMode);
      Serial.print("Servo Angle: "); Serial.print(servoAngle); Serial.println(" °");
      Serial.print("Servo Mode: "); Serial.println(servoMode);
      Serial.println("==================================");

      // Publish latest physical state to MQTT and Eclipse Ditto
      publishToDitto();
    }
    else {
      Serial.println("ERROR: Invalid Arduino data packet");
    }
  }
}