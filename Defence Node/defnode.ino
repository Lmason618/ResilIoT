// ESP8266  receiver for ChaCha20-Poly1305 encrypted broadcasts

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Crypto.h>
#include <ChaChaPoly.h>

// Match the key used on the Pi (32 bytes)
static const uint8_t WIFI_CHACHA_KEY[32] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

WiFiUDP udp;
const unsigned int LOCAL_UDP_PORT = 5005;
const size_t MAX_PACKET = 1500;

ChaChaPoly chachapoly; // instance of ChaChaPoly class

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.printf("Connecting to %s\n", ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - cannot listen for UDP");
    return;
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  udp.begin(LOCAL_UDP_PORT);
  Serial.printf("Listening on UDP port %u\n", LOCAL_UDP_PORT);

  // initialize chachapoly key
  // API: setKey(const uint8_t *key, size_t len)
  chachapoly.setKey(WIFI_CHACHA_KEY, sizeof(WIFI_CHACHA_KEY));
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) {
    delay(50);
    return;
  }

  if (packetSize > MAX_PACKET) {
    Serial.printf("Oversized packet (%d), skipping\n", packetSize);
    return;
  }

  uint8_t buffer[MAX_PACKET];
  int len = udp.read(buffer, MAX_PACKET);
  if (len <= 0) {
    return;
  }

  // Expect at least 12 bytes nonce + 16 bytes tag (so plaintext can be zero length theoretically)
  if (len < 12 + 16) {
    Serial.println("Packet too short to be valid ChachaPoly data");
    return;
  }

  // Extract nonce and ciphertext+tag
  uint8_t nonce[12];
  memcpy(nonce, buffer, 12);
  uint8_t* c_and_tag = buffer + 12;
  size_t c_and_tag_len = len - 12;

  // Set IV/nonce on the ChaChaPoly instance
  // Note: method names can differ between Crypto library versions.
  // Typical API: setIV(const uint8_t *iv, size_t len)
  if (!chachapoly.setIV(nonce, sizeof(nonce))) {
    Serial.println("setIV failed");
    return;
  }

    // Prepare output buffer (plaintext length = ciphertext length - 16 bytes tag)
    size_t plaintext_len = (c_and_tag_len >= 16) ? (c_and_tag_len - 16) : 0;
    uint8_t plaintext[512]; // adjust as needed

    // Start decryption
    chachapoly.setIV(nonce, sizeof(nonce));
    chachapoly.decrypt(plaintext, c_and_tag, plaintext_len);

    // Verify tag: last 16 bytes of c_and_tag
    const uint8_t *tag = c_and_tag + plaintext_len;
    bool ok = chachapoly.checkTag(tag, 16);

    if (!ok) {
      Serial.println("Decryption/authentication failed (bad tag or key?)");
      return;
    }

    // Null-terminate for printing
    if (plaintext_len < sizeof(plaintext)) {
      plaintext[plaintext_len] = 0;
    } else {
      plaintext[sizeof(plaintext) - 1] = 0;
    }

    Serial.printf("Received %u-byte plaintext: %s\n", (unsigned)plaintext_len, (char*)plaintext);

}
