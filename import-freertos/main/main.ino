// Libraries for the sensors, Wi-Fi, server, and tasks
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "MQ2Sensor.h"
#include "AlertController.h"
#include <FS.h>
#include "SD_MMC.h" // Changed this line
const char* jquery_min_js = "/jquery.min.js";
// Replace with your network credentials
const char *ssid = "Maryas";
const char *password = "11111111";
const char* echarts_min_js = "/echarts.min.js";
// Pins for the BMP280, MQ-2, LED, buzzer, and fire panel
Adafruit_BMP280 bmp280;
const int mq2Pin = 34;
const int ledPin = 17;
const int buzzerPin = 32;
const int firePanelPin = 16;

// SD card pin configuration for ESP32-WROVER
int mosiPin = 23;
int misoPin = 19;
int sckPin = 18;
int csPin = 5;

// Temperature threshold in degrees Celsius
const float TEMPERATURE_THRESHOLD = 26.0;

// Notification intervals in milliseconds
const unsigned long FIRE_INTERVAL = 5 * 1000;
const unsigned long MAINTENANCE_INTERVAL = 20 * 60 * 1000;
const unsigned long NORMAL_INTERVAL = 60 * 60 * 1000;

// Instances of the MQ2Sensor and AlertController classes
MQ2Sensor mq2(mq2Pin);
AlertController alertController(ledPin, buzzerPin);

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

// Update status task
// This task runs in the background and periodically checks the status of the MQ-2 sensor and BMP280 sensor.
// It updates the current status of the system based on the sensor readings and logs any status changes to the SD card.
void updateStatusTask(void *parameter) {
  for (;;) {
    float temperature = bmp280.readTemperature();
    bool mq2Value = mq2.read();
    bool highTemperature = temperature > TEMPERATURE_THRESHOLD;

    if (highTemperature) {
      if (mq2Value) {
        currentStatus = Fire;
        alertController.setAlertMode(Flashing);
      } else {
        currentStatus = Caution;
        alertController.setAlertMode(Flashing);
      }
    } else {
      if (mq2Value) {
        currentStatus = Maintenance;
        alertController.setAlertMode(Steady);
      } else {
        currentStatus = Normal;
        alertController.setAlertMode(Off);
      }
    }

    // Call setAlert with true to trigger the flashing when alertMode is set to Flashing.
    alertController.setAlert(true);

    logToSDCard("Temperature: " + String(temperature) + " °C - Smoke: " + (mq2Value ? "Yes" : "No"));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
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

// Function to generate the HTML content for the main page
String generateHtmlContent() {
  String html;
  html.reserve(1500);
  html = "<!DOCTYPE html>"
         "<html>"
         "<head>"
         "<style>"
         "body { font-family: Arial, sans-serif; text-align: center; }"
         "h1 { color: #4CAF50; }"
         "h2 { color: #333; }"
         ".gauge { width: 100%; height: 300px; display: inline-block; }"
         ".smoke-indicator { display: inline-block; width: 50px; height: 50px; border-radius: 50%; background-color: #ccc; }"
         ".smoke-detected { background-color: #f00 !important; }"
         "</style>"
         "<script src=\"/jquery.min.js\"></script>"
         "<script src=\"/echarts.min.js\"></script>"
         "<script>"
         "var temperatureGauge, fireStatusImage, ledStatusImage, statusText, smokeIndicator;"
         "  setInterval(function() {"
         "    $.getJSON(\"/status\", function(data) {"
         "      updateGauges(data.temperature);"
         "      updateSmokeIndicator(data.smokeDetected);"
         "      updateImages(data.smokeDetected, data.highTemperature);"
         "      updateStatus(data.status);"
         "    });"
         "  }, 1000);"

         "function createGauges() {"
         "  temperatureGauge = echarts.init(document.getElementById('temperature-gauge'));"
         "  temperatureGauge.setOption({"
         "    series: [{"
         "      type: 'gauge',"
         "      detail: {formatter: '{value} °C', fontSize: 24, offsetCenter: [0, '60%']},"
         "      data: [{value: 0}],"
         "      axisLine: {lineStyle: {width: 20}}"
         "    }]"
         "  });"
         "}"
         "function updateGauges(temperature) {"
         "  temperatureGauge.setOption({"
         "    series: [{"
         "      data: [{value: temperature}]"
         "    }]"
         "  });"
         "}"
         "function updateSmokeIndicator(smokeDetected) {"
         "  smokeIndicator.classList.toggle('smoke-detected', smokeDetected);"
         "}"
         "function updateImages(smokeDetected, highTemperature) {"
         "  fireStatusImage.style.display = highTemperature ? 'block' : 'none';"
         "}"
         "function updateStatus(status) {"
         "  statusText.innerHTML = 'System Status: ' + status;"
         "}"
         "$(document).ready(function(){"
         "  createGauges();"
         "  fireStatusImage = document.getElementById('fire-status-image');"
         "  smokeIndicator = document.getElementById('smoke-indicator');"
         "  statusText = document.getElementById('status-text');"
         "});"
         "</script>"
         "</head>"
         "<body>"
         "<h1>Fire Detection System</h1>"
         "<div class=\"gauge\" id=\"temperature-gauge\"></div>"
         "<h2>Temperature Sensor</h2>"
         "<div id=\"smoke-indicator\" class=\"smoke-indicator\"></div>"
         "<h2>Smoke Detector</h2>"
         "<img id=\"fire-status-image\" src=\"/high-temp.jpg\" width=\"300\" height=\"200\" style=\"display:none;\">"
         "<p id=\"status-text\">System Status: </p>"
         "</body>"
         "</html>";
  return html;
}

// The setup function initializes the serial communication, Wi-Fi connection, SD card, BMP280 sensor, pins, and starts the server and FreeRTOS tasks.
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

  // Print the IP address
  IPAddress local_IP = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(local_IP);

  // Initialize the SD card
  if (!SD_MMC.begin()) {
    Serial.println("SD card initialization failed!");
    return;
  }


  // Initialize the BMP280 sensor
  if (!bmp280.begin(0x76)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }

  // Configure the fire panel pin
  pinMode(firePanelPin, OUTPUT);
  digitalWrite(firePanelPin, LOW);

  // Configure LED and buzzer pins
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buzzerPin, HIGH);
  server.on("/high-temp.jpg", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SD_MMC, "/high-temp.jpg", "image/jpeg");
  });

  // Serve the sensor values and current status
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    String html = generateHtmlContent();
    request->send(200, "text/html", html);
  });
  server.on(echarts_min_js, HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SD_MMC, echarts_min_js, "application/javascript");
  });
  server.on(jquery_min_js, HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SD_MMC, jquery_min_js, "application/javascript");
  });
  server.on(jquery_min_js, HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SD_MMC, jquery_min_js, "application/javascript");
  });
  // Replace the existing /status route with the new code here
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest * request) {
    bool mq2Value = mq2.read();
    float temperature = bmp280.readTemperature();
    bool highTemperature = temperature > TEMPERATURE_THRESHOLD;
    String status;
    switch (currentStatus) {
      case Normal: status = "Normal"; break;
      case Maintenance: status = "Maintenance"; break;
      case Caution: status = "Caution"; break;
      case Fire: status = "Fire"; break;
    }
    String jsonResponse = String("{\"smokeDetected\":") + (mq2Value ? "true" : "false") +
                          ",\"highTemperature\":" + (highTemperature ? "true" : "false") +
                          ",\"temperature\":" + String(temperature) +
                          ",\"status\":\"" + status + "\"}";
    request->send(200, "application/json", jsonResponse);
  });


  // Start the server
  server.begin();
  // Create FreeRTOS tasks
  xTaskCreate(updateStatusTask, "Update Status Task", 4096, NULL, 1, NULL);
}

// Function to log messages to the SD card
void logToSDCard(const String &message) {
  File logFile = SD_MMC.open("/log.txt", FILE_APPEND); // Changed this line
  if (logFile) {
    logFile.println(message);
    logFile.close();
  } else {
    Serial.println("Error opening log file.");
  }
}

// The loop function remains empty since all tasks are now running on FreeRTOS tasks.
void loop() {
  // The loop function remains empty since all tasks are now running on FreeRTOS tasks
}
