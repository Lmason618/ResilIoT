#include <SPI.h>
#include <LoRa.h>
#include <ChaChaPoly.h>
#include <esp_sleep.h>
#include <Preferences.h>

#define NSS   5
#define RST   27
#define DIO0  26

#define TRIG_PIN  12
#define ECHO_PIN  14
#define HIGH_WATER_PIN 4

struct RateSleep {
  float rate;
  uint32_t sleepTime;
};
 //rate for rate of rise
const float intervalSeconds = 60;

// Sleep times in seconds
const uint32_t SLEEP_HIGH = 5;
const uint32_t SLEEP_MID  = 30;
const uint32_t SLEEP_LOW  = 60;

// RoR thresholds (cm/min)
const float ROR_MID  = 10.0;
const float ROR_HIGH = 20.0;

ChaChaPoly chacha;
const uint8_t key[32] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
  0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};

// RTC variables so they survive deep sleep
RTC_DATA_ATTR uint32_t bootCounter = 0;
RTC_DATA_ATTR float lastDistance = -1;
RTC_DATA_ATTR uint32_t lastMeasurementTime = 0;
RTC_DATA_ATTR uint32_t nextSleepSeconds = SLEEP_LOW;

Preferences prefs;  // Preferences object for persistent storage
uint32_t nonceCounter = 0;  // will be loaded from prefs

float getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 60000);
  if (duration == 0) return -1;
  return duration * 0.0343 / 2.0;
}

void sendEncryptedMessage(uint8_t dest, uint8_t src, const String &message) {
  // Load nonceCounter from prefs and increment
  prefs.begin("lora", false);
  nonceCounter = prefs.getUInt("nonce", 0) + 1;
  prefs.putUInt("nonce", nonceCounter);
  prefs.end();

  uint8_t nonce[12];
  memcpy(nonce, &nonceCounter, sizeof(nonceCounter));
  uint64_t t = micros();
  memcpy(nonce + 4, &t, sizeof(t));

  int plaintext_len = 2 + message.length();
  uint8_t plaintext[plaintext_len];
  plaintext[0] = dest;
  plaintext[1] = src;
  memcpy(&plaintext[2], message.c_str(), message.length());

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

  Serial.printf("Encrypted message sent: %s\n", message.c_str());
}


// calculates rate of rise and how long the node should sleep
RateSleep rateOfRise(float lastDistance, float currentDistance, uint32_t lastTime, uint32_t currentTime, float intervalSeconds, bool highWater, float toleranceSeconds = 5.0) {
  RateSleep rs = {0, SLEEP_LOW};  // default: no valid rate, low sleep

  if (lastDistance >= 0 && lastTime > 0) {
    float elapsedSeconds = (currentTime - lastTime) / 1000.0;

    if (elapsedSeconds >= (intervalSeconds - toleranceSeconds) && elapsedSeconds <= (intervalSeconds + toleranceSeconds)) {
      float deltaCM = lastDistance - currentDistance;
      // Since elapsedSeconds ~ intervalSeconds i am using the delta
      rs.rate = deltaCM;
    } else {
      Serial.printf("Elapsed time %.2f outside range (%.2f ± %.2f), skipping rate calculation\n", elapsedSeconds, intervalSeconds, toleranceSeconds);
    }
  } else {
    Serial.println("No valid previous measurement");
  }

  if (!isnan(rs.rate)) {
    Serial.printf("Rate of Rise: %.2f cm per %.0f seconds\n", rs.rate, intervalSeconds);

    if (highWater) {
      Serial.println("High water trigger active → HIGH rate sleep");
      rs.sleepTime = SLEEP_HIGH;
    } else if (rs.rate >= ROR_HIGH) {
      rs.sleepTime = SLEEP_HIGH;
    } else if (rs.rate >= ROR_MID) {
      rs.sleepTime = SLEEP_MID;
    }
  } else {
    Serial.println("Rate invalid or not calculated → default LOW sleep");
  }

  return rs;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  bootCounter++;

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(HIGH_WATER_PIN, INPUT_PULLDOWN);


  LoRa.setPins(NSS, RST, DIO0);
  if (!LoRa.begin(434E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125000);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);

  chacha.setKey(key, sizeof(key));

  // Init Preferences and nonceCounter
  prefs.begin("lora", false);
  if (!prefs.isKey("nonce")) {
    prefs.putUInt("nonce", 0);
    nonceCounter = 0;
  } else {
    nonceCounter = prefs.getUInt("nonce", 0);
  }
  prefs.end();

  float distance = getDistanceCM();
  uint32_t now = millis();
  Serial.printf("Distance: %.2f cm\n", distance);

  bool highWater = (digitalRead(HIGH_WATER_PIN) == HIGH);

  RateSleep rs = rateOfRise(lastDistance, distance, lastMeasurementTime, now, intervalSeconds, highWater);
  nextSleepSeconds = rs.sleepTime;

  lastDistance = distance;
  lastMeasurementTime = now;

  // Create "distance,rate,highWater" message
  String message = String(distance, 1) + "," + String(rs.rate, 1) + "," + String((int)highWater);

  // Send to Pi (to pi at 0x01), from slave (ID=3)
  sendEncryptedMessage(0x01, 3, message);

  Serial.printf("Sleeping for %u seconds...\n", nextSleepSeconds);

  esp_sleep_enable_timer_wakeup((uint64_t)nextSleepSeconds * 1000000ULL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)HIGH_WATER_PIN, 1);

  esp_deep_sleep_start();
}

void loop() {}
