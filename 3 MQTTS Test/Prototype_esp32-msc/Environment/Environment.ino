#include <Adafruit_Sensor.h>
// Include library for MQ135 Sensor
#include <MQ135.h>
// Include library for DHT Sensor
#include <DHT.h>
#include <DHT_U.h>
// Include library for Servo Motor
#include <Servo.h>
// Include library for Software Serial - Enables serial communication on digital pins
#include <SoftwareSerial.h>


// Defining DHT Sensor Type and Pin
#define DHTTYPE DHT22
#define DHTPIN 13
// Create DHT22 sensor object
DHT_Unified dht(DHTPIN, DHTTYPE);

// Defining MQ135 analog input pin
#define MQ135gas A0

// Defining Servo Motor control pin
#define SERVOPIN 9
// Create servo motor object
Servo servoMotor;


// Configure software UART pins for ESP32 communication
// RX = 5, TX = 6
SoftwareSerial espSerial(5, 6);

// Set default actuator control mode
String controlMode = "AUTO";

// Store current servo position and operating state
int currentServoAngle = 0;
String currentServoMode = "OFF";

// Store latest MQ135 air quality reading
int lastAirQuality = 0;

// Store timestamp of previous sensor reading
unsigned long previousSensorTime = 0;

// Set sensor sampling interval to 30 seconds
const unsigned long SENSOR_INTERVAL = 30000;


// FUNCTION: MOVE SERVO
void setServo(int angle, String modeName) {

  // Set servo angle to valid range
  angle = constrain(angle, 0, 180);

  // Attach servo only when movement is required
  if (!servoMotor.attached()) {
    servoMotor.attach(SERVOPIN);
  }
  // Move servo to defined position
  servoMotor.write(angle);

  // Move servo to requested position
  currentServoAngle = angle;
  currentServoMode = modeName;

  Serial.print("Servo moved to: ");
  Serial.print(angle);
  Serial.print(" degrees | Mode: ");
  Serial.println(modeName);

  // If servo is OFF, return position to zero then detach servo
  if (angle == 0) {
    delay(500);
    servoMotor.detach();
  }
}

// FUNCTION: AUTOMATIC SERVO CONTROL
void applyAutomaticServoControl() {

  if (lastAirQuality <= 300) {
    setServo(0, "OFF");
  }
  else if (lastAirQuality <= 500) {
    setServo(45, "LOW FAN");
  }
  else if (lastAirQuality <= 700) {
    setServo(90, "MILD FAN");
  }
  else {
    setServo(180, "HIGH FAN");
  }
}


// FUNCTION: RECEIVE COMMAND FROM ESP32
void checkESPCommand() {
  // Exit if no command has been received
  if (espSerial.available() <= 0) {
    return;
  }
  
  // Read one complete command from the ESP32
  String command = espSerial.readStringUntil('\n');
  // Remove whitespace
  command.trim();

  Serial.println();
  Serial.println("===== COMMAND RECEIVED FROM ESP32 =====");

  Serial.print("Command: ");
  Serial.println(command);

  // AUTO COMMAND - Switch actuator control to automatic mode
  if (command == "SERVO,AUTO") {
    controlMode = "AUTO";
    Serial.println("Control mode changed to AUTO");
    // Immediately restore automatic control
    applyAutomaticServoControl();
  }

  // MANUAL COMMAND -  Switch actuator control to manual mode
  else if (command.startsWith("SERVO,MANUAL,")) {
    // Extract requested servo angle from command
    String angleString = command.substring(String("SERVO,MANUAL,").length());
    // Convert requested angle to integer
    int requestedAngle = angleString.toInt();
    // Validate requested servo angle
    if (requestedAngle >= 0 && requestedAngle <= 180) {
      controlMode = "MANUAL";
      String manualMode;
      // Mark zero-degree position as OFF
      if (requestedAngle == 0) {
        manualMode = "OFF";
      }
      else {
        manualMode = "MANUAL";
      }
      
      // Move servo to requested manual position
      setServo(requestedAngle, manualMode);

      Serial.println("Manual Ditto command applied successfully.");
    }
    else {
      Serial.println("ERROR: Invalid servo angle.");
    }
  }

  else {
    Serial.println("ERROR: Unknown ESP32 command.");
  }

  Serial.println("========================================");
  Serial.println();
}


// FUNCTION: READ SENSORS + SEND TELEMETRY
void readSensors() {
  // Create sensor event structures for DHT22 readings
  sensors_event_t tempEvent;
  sensors_event_t humidityEvent;

  // Read current temperature value
  dht.temperature().getEvent(&tempEvent);
  float currentTemperature = tempEvent.temperature;

  // Read current humidity value
  dht.humidity().getEvent(&humidityEvent);
  float currentHumidity = humidityEvent.relative_humidity;

  // Stop processing if DHT22 reading fails
  if (isnan(currentTemperature) || isnan(currentHumidity)) {
    Serial.println("ERROR: DHT22 reading failed.");
    return;
  }

  // Read current raw air quality value from MQ135
  lastAirQuality = analogRead(MQ135gas);
  // Store descriptive air quality category
  String airQualityStatus;

  // Air Quality Classification based on predefined Air Quality Index (AQI) thresholds
  if (lastAirQuality <= 300) { //Good AQI
    airQualityStatus = "Good";
  }
  else if (lastAirQuality <= 500) { //Moderate AQI
    airQualityStatus = "Moderate";
  }
  else if (lastAirQuality <= 700) { //Poor AQI
    airQualityStatus = "Poor";
  }
  else { //Hazardous AQI
    airQualityStatus = "Hazardous";
  }

  // Automatically adjust servo only in AUTO mode
  if (controlMode == "AUTO") {
    applyAutomaticServoControl();
  }


  // Display sensor and actuator states
  Serial.println("===== Environmental Reading =====");

  Serial.print("Temperature: "); Serial.print(currentTemperature); Serial.println(" °C");

  Serial.print("Humidity: "); Serial.print(currentHumidity); Serial.println(" %");

  Serial.print("Air Quality: "); Serial.println(lastAirQuality);

  Serial.print("Air Quality Status: "); Serial.println(airQualityStatus);

  Serial.print("Control Mode: "); Serial.println(controlMode);

  Serial.print("Actual Servo Angle: "); Serial.println(currentServoAngle);

  Serial.print("Actual Servo Status: "); Serial.println(currentServoMode);

  // Send sensor and actuator telemetry to ESP32 as CSV data
  // environment: temperature, humidity, airQuality, airQualityStatus,
  // ventilation: controlMode, servoAngle, servoMode
  espSerial.print(currentTemperature); espSerial.print(",");

  espSerial.print(currentHumidity); espSerial.print(",");

  espSerial.print(lastAirQuality); espSerial.print(",");

  espSerial.print(airQualityStatus); espSerial.print(",");

  espSerial.print(controlMode); espSerial.print(",");

  espSerial.print(currentServoAngle); espSerial.print(",");

  espSerial.println(currentServoMode);

  // Display transmitted CSV packet
  Serial.print("Sent to ESP32: ");

  Serial.print(currentTemperature); Serial.print(",");

  Serial.print(currentHumidity); Serial.print(",");

  Serial.print(lastAirQuality); Serial.print(",");

  Serial.print(airQualityStatus); Serial.print(",");

  Serial.print(controlMode); Serial.print(",");

  Serial.print(currentServoAngle); Serial.print(",");

  Serial.println(currentServoMode);

  Serial.println("=================================");
}


// FUNCTION: SETUP
void setup() {
  // Start USB serial communication for debugging
  Serial.begin(115200);
  // Start software UART communication with ESP32
  espSerial.begin(9600);
  // Initialise DHT22 sensor
  dht.begin();
  // Configure MQ135 pin as analog input
  pinMode(MQ135gas,INPUT);
  // Initialise servo at zero position
  servoMotor.attach(SERVOPIN);
  servoMotor.write(0);

  delay(1000);
  // Detach servo to reduce jitter and unnecessary power use
  servoMotor.detach();

  // Display system startup message
  Serial.println("Indoor Environmental Monitoring");
  // Default control mode
  Serial.println("Control Mode: AUTO");

  // Allow hardware stabilisation
  delay(5000);

  // Force first sensor reading immediately
  previousSensorTime = millis() - SENSOR_INTERVAL;
}


// FUNCTION: LOOP
void loop() {

  // Check Ditto/ESP commands continuously from ESP32
    checkESPCommand();

  // Sensor sampling using system uptime without blocking command reception
  unsigned long currentTime = millis();

  // Read sensors once every 30 seconds
  if (currentTime - previousSensorTime >= SENSOR_INTERVAL) {
    // Update timestamp for the latest sampling cycle
    previousSensorTime = currentTime;
    // Acquire sensor readings and transmit telemetry
    readSensors();
  }
}