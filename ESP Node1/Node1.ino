#include <WiFi.h>
#include <time.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <LoRa.h>
#include <Preferences.h>
#include <ChaChaPoly.h>

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
RTC_DATA_ATTR uint32_t nonceCounter = 0;  // persistent nonce part

unsigned long bootMillis = 0;
unsigned long lastReportMillis = 0;

// Soil calibration
const int soilDry = 3400; //  0% saturation
const int soilWet = 1456; // 100% saturation

// IDs
const uint8_t DEST_ID = 0x01; // send to Pi
const uint8_t SRC_ID  = 0x02; // this node

// Encryption key (32 bytes, for now just 0x00 to 0x1f)
const uint8_t CHACHA_KEY[32] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
  0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};

ChaChaPoly chacha;

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

int mapSoilSaturation(int rawValue) {
  if (rawValue >= soilDry) return 0;
  if (rawValue <= soilWet) return 100;
  return map(rawValue, soilDry, soilWet, 0, 100);
}

// uses preferences for nonce counter
void sendEncryptedMessage(uint8_t dest, uint8_t src, const String &message) {
  prefs.begin("lora", false);
  nonceCounter = prefs.getUInt("nonce", 0) + 1;
  prefs.putUInt("nonce", nonceCounter);
  prefs.end();

  uint8_t nonce[12];
  memcpy(nonce, &nonceCounter, sizeof(nonceCounter));
  uint64_t microsNow = micros();
  memcpy(nonce + 4, &microsNow, sizeof(microsNow));

  int plaintext_len = 2 + message.length();
  uint8_t plaintext[plaintext_len];
  plaintext[0] = dest;
  plaintext[1] = src;
  memcpy(&plaintext[2], message.c_str(), message.length());

  chacha.setKey(CHACHA_KEY, sizeof(CHACHA_KEY));
  chacha.setIV(nonce, sizeof(nonce));

  uint8_t ciphertext[plaintext_len];
  uint8_t tag[16];
  chacha.encrypt(ciphertext, plaintext, plaintext_len);
  chacha.computeTag(tag, sizeof(tag));

  LoRa.beginPacket();
  LoRa.write(nonce, sizeof(nonce));
  LoRa.write(ciphertext, plaintext_len);
  LoRa.write(tag, sizeof(tag));
  LoRa.endPacket();

  Serial.printf("Encrypted message sent to dest %d from src %d: %s\n", dest, src, message.c_str());
}

void printAndSendSensorReport() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilRaw = analogRead(SOIL_PIN);
  int soilSat = mapSoilSaturation(soilRaw);

  float rainLastMinute = tipCountLastMinute * rainfallPerTip;
  float rainTotal = totalTipCount * rainfallPerTip;

  time_t now = time(NULL);
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  Serial.println("------ 60s Sensor Report ------");
  Serial.printf("Time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Serial.printf("Temp: %.1f °C\n", temperature);
  Serial.printf("Hum: %.1f %%\n", humidity);
  Serial.printf("Soil Sat: %d %%\n", soilSat);
  Serial.printf("Rain/min: %.2f mm\n", rainLastMinute);
  Serial.printf("Rain total: %.2f mm\n", rainTotal);
  Serial.println("-------------------------------");

  String payload = String((int)temperature) + "," +
                   String((int)humidity) + "," +
                   String(soilSat) + "," +
                   String(rainLastMinute, 2) + "," +
                   String(rainTotal, 2);

  sendEncryptedMessage(DEST_ID, SRC_ID, payload);

  tipCountLastMinute = 0;

  // reset at 9AM
  if (timeinfo.tm_hour == 9 && now - lastResetTime > 3600) {
    totalTipCount = 0;
    lastResetTime = now;
  }
}

void goToDeepSleep() {
  Serial.println("Preparing to sleep");

  int currentState = digitalRead(HALL_PIN);
  if (lastHallState == -1)  lastHallState = currentState;
  int wakeLevel = (lastHallState == 0) ? 1 : 0;
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

  prefs.begin("lora", false);
    if (!prefs.isKey("nonce")) prefs.putUInt("nonce", 0);
    prefs.end();

    chacha.setKey(CHACHA_KEY, sizeof(CHACHA_KEY));  // Set key

    printAndSendSensorReport();

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
