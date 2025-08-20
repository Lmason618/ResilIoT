#include <WiFi.h>
#include <time.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <LoRa.h>

#define HALL_PIN 14
#define DHT_PIN 4
#define SOIL_PIN 33
#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

#define LORA_SS   5
#define LORA_RST  27
#define LORA_DIO0 26
#define LORA_FREQ 434E6  // freq 434Mhz

// config vars
const int reportInterval = 60;     // s between reports
const int idleSleepDelay = 10;     // s to wait after last tip before sleep
const float rainfallPerTip = 0.2;  // mm of rain per tip

// RTC so it persists after deep sleep
RTC_DATA_ATTR int totalTipCount = 0;
RTC_DATA_ATTR int tipCountLastMinute = 0;
RTC_DATA_ATTR time_t lastResetTime = 0;
RTC_DATA_ATTR int lastHallState = -1;
RTC_DATA_ATTR unsigned long lastTipMillis = 0;

unsigned long bootMillis = 0;
unsigned long lastReportMillis = 0;

void IRAM_ATTR onTip() {
  tipCountLastMinute++;
  totalTipCount++;
  lastTipMillis = millis();
}

void connectToWiFi() {
  WiFi.begin("Puchen13a", "Dropzone3");
  while (WiFi.status() != WL_CONNECTED) delay(300);
  configTime(0, 0, "pool.ntp.org");
}

void sendLoRaReport(float temperature, float humidity, int soilValue, float rainLastMinute, float rainTotal) {
  LoRa.beginPacket();
  LoRa.printf("Temp:%.1fC,Hum:%.1f%%,Soil:%d,RainRate:%.1fmm,TotalRain:%.1fmm",
              temperature, humidity, soilValue, rainLastMinute, rainTotal);
  LoRa.endPacket();
}

void printSensorReport() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilValue = analogRead(SOIL_PIN);

  time_t now = time(NULL);
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  float rainLastMinute = tipCountLastMinute * rainfallPerTip;
  float rainTotal = totalTipCount * rainfallPerTip;

  Serial.println("60s Sensor Report");
  Serial.printf("Time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Serial.printf("Temperature: %.1f °C\n", temperature);
  Serial.printf("Humidity: %.1f %%\n", humidity);
  Serial.printf("Soil Moisture (Raw): %d\n", soilValue);
  Serial.printf("Rainfall Rate (last min): %d mm/min\n", rainLastMinute);
  Serial.printf("Total Rainfall (since 9AM): %d mm\n", rainTotal);
  Serial.println("");

 // Send via LoRa
  sendLoRaReport(temperature, humidity, soilValue, rainLastMinute, rainTotal);


  tipCountLastMinute = 0;

  if (timeinfo.tm_hour == 9 && now - lastResetTime > 3600) {
    Serial.println("24hr Rain Summary");
    Serial.printf("Total Rainfall in last 24h: %d mm\n", rainTotal);
    Serial.println("");

    totalTipCount = 0;
    lastResetTime = now;
  }
}

void goToDeepSleep() {
  Serial.println("Preparing to sleep");

  int currentState = digitalRead(HALL_PIN);
  if (lastHallState == -1) {
    lastHallState = currentState;
  }

  // Wake on !lastHallState
  int wakeLevel = (lastHallState == 0) ? 1 : 0;
  Serial.printf("Last Hall state: %d, waking on level: %d\n", lastHallState, wakeLevel);
  lastHallState = currentState;

  esp_sleep_enable_timer_wakeup(reportInterval * 1000000ULL); // Wake on time
  esp_sleep_enable_ext0_wakeup((gpio_num_t)HALL_PIN, wakeLevel); // wake on magnet change

  Serial.println("Sleeping ");
  delay(200);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

 LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125000);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);


  dht.begin();
  pinMode(SOIL_PIN, INPUT);
  pinMode(HALL_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), onTip, CHANGE);

  connectToWiFi();
  delay(1000);

  bootMillis = millis();
  lastReportMillis = bootMillis;

// If just booted, ensure lastTipMillis is valid
  if (lastTipMillis == 0) lastTipMillis = bootMillis;

  printSensorReport();  // report on wake

void loop() {
  unsigned long nowMillis = millis();

  // Report every reportInterval seconds
   if (nowMillis - lastReportMillis >= (unsigned long)reportInterval * 1000) {
     printSensorReport();
     lastReportMillis = nowMillis;
   }

  // sleeps if no tips for idleSleepDelay seconds
    if ((nowMillis - lastTipMillis >= (unsigned long)idleSleepDelay * 1000) && tipCountLastMinute == 0) {
      goToDeepSleep();
    }

}
