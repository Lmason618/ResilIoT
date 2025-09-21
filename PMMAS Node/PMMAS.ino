#include <WiFi.h>
#include <WiFiUdp.h>
#include <Crypto.h>
#include <ChaChaPoly.h>


#define BUZZER_PIN 0
#define BUTTON_PIN 1
#define HAPTIC_PIN 2
#define RED_LED    21
#define GREEN_LED  20
#define BLUE_LED   10


const char* ssid     = "ResilIoT_AP";
const char* password = "Password123";
const unsigned int LOCAL_UDP_PORT = 5005;
const size_t MAX_PACKET = 1500;


static const uint8_t WIFI_CHACHA_KEY[32] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
  0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};


WiFiUDP udp;
ChaChaPoly chachapoly;

enum AlertLevel { ALERT_NONE, ALERT_LOW, ALERT_MID, ALERT_HIGH };
AlertLevel currentAlert = ALERT_NONE;    // currently active alert
AlertLevel lastReceivedMessage = ALERT_NONE; // last message received
bool acknowledgedHigh = false;



void setLED(bool r, bool g, bool b) {
  digitalWrite(RED_LED,   r ? LOW : HIGH);
  digitalWrite(GREEN_LED, g ? LOW : HIGH);
  digitalWrite(BLUE_LED,  b ? LOW : HIGH);
}

void triggerHaptic(unsigned long duration) {
  digitalWrite(HAPTIC_PIN, HIGH);
  delay(duration);
  digitalWrite(HAPTIC_PIN, LOW);
}

void triggerBuzzer(unsigned long duration) {
  ledcAttach(BUZZER_PIN, 2000, 8); // 2kHz, 8-bit
  ledcWrite(BUZZER_PIN, 127);
  delay(duration);
  ledcWrite(BUZZER_PIN, 0);
  ledcDetach(BUZZER_PIN);
}

void startBeacon() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("BeaconAP", "12345678", 1, false, 1);
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // max allowed
  Serial.println("Beacon mode started");
}


void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(HAPTIC_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  setLED(0,0,0);

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println("\nWiFi connected");

  udp.begin(LOCAL_UDP_PORT);
  chachapoly.setKey(WIFI_CHACHA_KEY, sizeof(WIFI_CHACHA_KEY));
}


void loop() {
  // Handle button for High alert ack
  if (digitalRead(BUTTON_PIN) == LOW && currentAlert == ALERT_HIGH) {
    acknowledgedHigh = true;
    currentAlert = ALERT_NONE;
    setLED(0,0,0);
    Serial.println("High alert acknowledged");
  }

  // Check for incoming packets
  int packetSize = udp.parsePacket();
  if (packetSize > 0 && packetSize <= MAX_PACKET) {
    uint8_t buffer[MAX_PACKET];
    int len = udp.read(buffer, MAX_PACKET);
    if (len >= 12 + 16) {
      uint8_t nonce[12];
      memcpy(nonce, buffer, 12);
      uint8_t* c_and_tag = buffer + 12;
      size_t c_and_tag_len = len - 12;
      size_t plaintext_len = (c_and_tag_len >= 16) ? (c_and_tag_len - 16) : 0;
      uint8_t plaintext[256];
      chachapoly.setIV(nonce, sizeof(nonce));
      chachapoly.decrypt(plaintext, c_and_tag, plaintext_len);
      const uint8_t* tag = c_and_tag + plaintext_len;
      if (chachapoly.checkTag(tag, 16)) {
        plaintext[plaintext_len] = 0;
        String msg = String((char*)plaintext);
        Serial.println("Received: " + msg);

        AlertLevel newAlert = ALERT_NONE;
        if (msg == "Low")       newAlert = ALERT_LOW;
        else if (msg == "Mid")  newAlert = ALERT_MID;
        else if (msg == "High") newAlert = ALERT_HIGH;
        else if (msg == "None") newAlert = ALERT_NONE;

        // Only trig if new message is different from last
        if (newAlert != lastReceivedMessage) {
          lastReceivedMessage = newAlert;

          // Reset ack flag if new alert is not High
          if (newAlert != ALERT_HIGH) acknowledgedHigh = false;

          if (newAlert == ALERT_HIGH) {
            // Trigger High if not acknowledged
            if (!acknowledgedHigh) {
              currentAlert = ALERT_HIGH;
              startBeacon();
            }
          } else {

            currentAlert = newAlert;
          }
        }
      }
    }
  }

  // Handle alerts
  switch (currentAlert) {
    case ALERT_LOW:
      setLED(0,1,0); // green
      triggerHaptic(2000);
      triggerBuzzer(2000);
      setLED(0,0,0);
      currentAlert = ALERT_NONE;
      break;

    case ALERT_MID:
      setLED(1,1,0); // yellow
      for (int i=0;i<2;i++){
        triggerHaptic(1500);
        triggerBuzzer(1500);
        delay(500);
      }
      setLED(0,0,0);
      currentAlert = ALERT_NONE;
      break;

    case ALERT_HIGH:
      if (!acknowledgedHigh) {
        setLED(1,0,0); // red
        triggerHaptic(800);
        triggerBuzzer(800);
        delay(200);
      }
      break;

    default:
      break;
  }
}
