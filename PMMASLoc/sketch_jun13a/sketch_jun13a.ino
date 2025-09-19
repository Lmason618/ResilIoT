#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi beacon 
const char* ssid = "BeaconAP";
const char* password = "12345678";

// RSSI smoothing
float filteredRSSI = -60;
float alpha = 0.3;

// RSSI distance estimation
float txPower = -47;
float pathLossExponent = 2.0;

// Reconnect timing
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 5000; 

float estimateDistance(float rssi) {
  return pow(10.0, (txPower - rssi) / (10.0 * pathLossExponent));
}

float getFilteredRSSI(int rawRSSI) {
  filteredRSSI = alpha * rawRSSI + (1.0 - alpha) * filteredRSSI;
  return filteredRSSI;
}

const char* classifyProximity(float distance) {
  if (distance <= 5.0) return "HOT";
  else if (distance <= 14.0) return "WARM";
  else return "COLD";  
}

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 not found!");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println("STARTING...");
  display.display();

  delay(1000);

  WiFi.begin(ssid, password);
  lastReconnectAttempt = millis();
}

void loop() {
  int rawRSSI;
  float rssi, distance;
  const char* status;

  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > reconnectInterval) {
      Serial.println("Attempting to reconnect...");
      WiFi.begin(ssid, password);
      lastReconnectAttempt = now;
    }

    status = "NOTHING";
  } else {
    rawRSSI = WiFi.RSSI();
    rssi = getFilteredRSSI(rawRSSI);
    distance = estimateDistance(rssi);
    status = classifyProximity(distance);
  }


  // OLED display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (WiFi.status() == WL_CONNECTED) {
    display.print("RSSI: ");
    display.print(rssi, 1);
    display.println(" dBm");

    display.print("Dist: ");
    display.print(distance, 2);
    display.println(" m");
  } else {
    display.println("RSSI: --- dBm");
    display.println("Dist: --- m");
  }

  display.setTextSize(2);
  display.setCursor(0, 32);
  display.println(status);
  display.display();

  delay(1000);
}
