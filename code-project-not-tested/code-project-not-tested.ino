#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>

// Replace with your network credentials
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";

// MQ-11 digital pin
const int mq11Pin = 32;

// Define temperature threshold
const float TEMPERATURE_THRESHOLD = 65.0;

const unsigned long FIRE_INTERVAL = 5 * 1000; //5 seconds
const unsigned long MAINTENANCE_INTERVAL = 20 * 60 * 1000; //20 minutes
const unsigned long NORMAL_INTERVAL = 60 * 60 * 1000; //1 hour

// LED and buzzer pins
const int ledPin = 33;
const int buzzerPin = 34;
const int firePanelPin = 35;
const int chipSelect = 14;

// BMP-280
Adafruit_BMP280 bmp;

// Create an instance of the server
AsyncWebServer server(80);

// Status and protection levels
enum Status {
  Normal,
  Maintenance,
  Caution,
  Fire
};

Status currentStatus = Normal;
unsigned long lastNotificationTime = 0;

// Function to determine status
void updateStatus() {
  int mq11Value = digitalRead(mq11Pin);
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure();
  Status previousStatus = currentStatus;

  if (mq11Value == HIGH || temperature > TEMPERATURE_THRESHOLD) {
    currentStatus = Caution;
    if (mq11Value == HIGH && temperature > TEMPERATURE_THRESHOLD) {
      currentStatus = Fire;
    }
  } else {
    currentStatus = Normal;
  }

  // Send a signal to the fire panel and log the change if the status has changed
  if (currentStatus != previousStatus) {
    if (currentStatus == Fire) {
      digitalWrite(firePanelPin, HIGH);
      logToSDCard("Fire detected");
    } else {
      digitalWrite(firePanelPin, LOW);
    }
    logToSDCard("Status changed to: " + String(currentStatus));
  }
}

// Function to handle notifications
void sendNotification() {
  // Implement your notification sending logic here
  // This will be dependent on the cloud service you use
}

// Function to handle notifications based on status and time intervals
bool shouldSendNotification() {
  static unsigned long lastNotificationTime = 0;
  unsigned long currentTime = millis();
  unsigned long interval = 0;

  if (currentStatus == Fire) {
    interval = FIRE_INTERVAL;
  } else if (currentStatus == Maintenance) {
    interval = MAINTENANCE_INTERVAL;
  } else if (currentStatus == Normal) {
    interval = NORMAL_INTERVAL;
  }

  if (currentTime - lastNotificationTime >= interval) {
    lastNotificationTime = currentTime;
    return true;
  }

  return false;
}

void logToSDCard(const String& message) {
  File logFile = SD.open("/log.txt", FILE_APPEND);
  if (logFile) {
    logFile.println(message);
    logFile.close();
  } else {
    Serial.println("Error opening log file.");
  }
}

// Function to generate HTML content
String generateHtmlContent() {
  String html;
  html.reserve(200);
  html = "<html><body>";
  html += "MQ-11: " + String(digitalRead(mq11Pin)) + "<br>";
  html += "Temperature: " + String(bmp.readTemperature()) + " &#8451;<br>";
  html += "Pressure: " + String(bmp.readPressure() / 100) + " hPa<br>";
  html += "Status: ";
  switch (currentStatus) {
    case Normal: html += "Normal"; break;
    case Maintenance: html += "Maintenance"; break;
    case Caution: html += "Caution"; break;
    case Fire: html += "Fire"; break;
  }
  html += "</body></html>";
  return html;
}

void setup() {
  // Start the serial communication
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  pinMode(chipSelect, OUTPUT);
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }

  // Configure the fire panel pin
  pinMode(firePanelPin, OUTPUT);
  digitalWrite(firePanelPin, LOW);

  // Initialize the BMP-280 sensor
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP-280 sensor, check wiring!");
    while (1);
  }

  // Configure the MQ-11 digital pin
  pinMode(mq11Pin, INPUT);

  // Configure LED and buzzer pins
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  // Serve the sensor values and current status
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest * request) {
    String html = generateHtmlContent();
    request->send(200, "text/html", html);
  });

  // Start the server
  server.begin();
}

void loop() {
  updateStatus();

  if (currentStatus == Caution || currentStatus == Fire) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }

  if (currentStatus == Fire) {
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(buzzerPin, LOW);
  }

  if (shouldSendNotification()) {
    sendNotification();
  }
}
