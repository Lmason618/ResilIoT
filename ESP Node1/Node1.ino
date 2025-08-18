#include <WiFi.h>
#include <time.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

#define HALL_PIN 14
#define DHT_PIN 4
#define SOIL_PIN 33
#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

// RTC so it persists after deep sleep
RTC_DATA_ATTR int totalRain = 0;
RTC_DATA_ATTR time_t lastResetTime = 0;
RTC_DATA_ATTR unsigned long lastTipMillis = 0;

volatile int tipCount = 0;
unsigned long bootTime = 0;
unsigned long lastReportTime = 0;

void IRAM_ATTR onTip() {
  tipCount++;
  lastTipMillis = millis();
}

void connectToWiFi() {
  WiFi.begin("Puchen13a", "Dropzone3");
  while (WiFi.status() != WL_CONNECTED) delay(300);
  configTime(0, 0, "pool.ntp.org");
}

void printSensorReport() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilValue = analogRead(SOIL_PIN);

  time_t now = time(NULL);
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  Serial.println("------ 60s Sensor Report ------");
  Serial.printf("Time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Serial.printf("Temperature: %.1f °C\n", temperature);
  Serial.printf("Humidity: %.1f %%\n", humidity);
  Serial.printf("Soil Moisture (Raw): %d\n", soilValue);
  Serial.printf("Rainfall Rate (last min): %d mm/min\n", tipCount);
  Serial.printf("Total Rainfall (since 9AM): %d mm\n", totalRain);
  Serial.println("-------------------------------");

  totalRain += tipCount;
  tipCount = 0;

  if (timeinfo.tm_hour == 9 && now - lastResetTime > 3600) {
    Serial.println("------ 24hr Rain Summary ------");
    Serial.printf("Total Rainfall in last 24h: %d mm\n", totalRain);
    Serial.println("-------------------------------");

    totalRain = 0;
    lastResetTime = now;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();
  pinMode(SOIL_PIN, INPUT);
  pinMode(HALL_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), onTip, RISING);

  connectToWiFi();
  delay(1000);

  bootTime = millis();
  lastReportTime = bootTime;

  printSensorReport();  // report on wake

void loop() {
  unsigned long now = millis();

  // Report again every 60s while awake
  if (now - lastReportTime >= 60000) {
    printSensorReport();
    lastReportTime = now;
  }

  // If no magnet swings for 10s, go to sleep
  if (now - lastTipMillis > 10000 && now - bootTime > 10000) {
    Serial.println("No rain activity for 10s. Sleeping...");

    esp_sleep_enable_timer_wakeup(60 * 1000000ULL);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)HALL_PIN, 1);
    delay(100);
    esp_deep_sleep_start();
  }

  delay(100);
}
