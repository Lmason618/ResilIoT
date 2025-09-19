from time import sleep, strftime
import sys
import os
import sqlite3
from SX127x.LoRa import *
from SX127x.LoRaArgumentParser import LoRaArgumentParser
from SX127x.board_config import BOARD
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305

# -----------------------
# CONFIGURATION
# -----------------------
MY_ADDRESS = 0x01

CHACHA_KEY = bytes([
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
])

DB_PATH = os.path.expanduser("~/ResilIoT/db/sensor_data.db")
# -----------------------

BOARD.setup()
parser = LoRaArgumentParser("Continuous LoRa receiver.")

def decrypt_message(payload_bytes):
    if len(payload_bytes) < 12 + 16:
        raise ValueError("Payload too short to contain nonce and tag")
    nonce = payload_bytes[:12]
    ciphertext_and_tag = payload_bytes[12:]
    chacha = ChaCha20Poly1305(CHACHA_KEY)
    plaintext = chacha.decrypt(nonce, ciphertext_and_tag, associated_data=None)
    dest = plaintext[0]
    src = plaintext[1]
    message = plaintext[2:].decode('utf-8', errors='ignore')
    return dest, src, message

def check_range(value, min_val, max_val):
    try:
        f = float(value)
        if f < min_val or f > max_val:
            return None
        return f
    except:
        return None

def log_error(timestamp, src, raw_data, reason):
    print(f"[ERROR LOG] {timestamp} Node {src}: {raw_data} ({reason})")

def append_data(row):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    try:
        c.execute("""
            INSERT INTO sensor_readings (
                timestamp, soil, temp, hum, rain, total_daily_rain,
                river, rate_of_rise, high_level_alert, sensor_id
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, row)
        conn.commit()
        print(f"[INFO] Inserted row: {row}")
    except sqlite3.IntegrityError:
        print(f"[WARN] Duplicate timestamp, row skipped: {row}")
    except Exception as e:
        print(f"[ERROR] DB insert failed: {e}")
    finally:
        conn.close()

class LoRaRcvCont(LoRa):
    def __init__(self, verbose=False):
        super(LoRaRcvCont, self).__init__(verbose)
        self.set_mode(MODE.SLEEP)
        self.set_dio_mapping([0] * 6)

    def on_rx_done(self):
        BOARD.led_on()
        self.clear_irq_flags(RxDone=1)
        payload = self.read_payload(nocheck=True)
        payload_bytes = bytes(payload)

        print(f"Payload length: {len(payload_bytes)}")
        print("Payload hex:", payload_bytes.hex())

        try:
            dest, src, text = decrypt_message(payload_bytes)
            print(f"[DEBUG] dest={dest}, src={src}, text={text}")
            if dest != MY_ADDRESS:
                print(f"[WARN] Ignored message to dest {dest}")
            else:
                fields = text.split(",")
                timestamp = strftime("%Y-%m-%d %H:%M:%S")

                # Node 2: temp, humidity, soil saturation, rain/min, total_daily_rain
                if src == 2:
                    if len(fields) != 5:
                        log_error(timestamp, src, text, "Unexpected number of fields")
                        return
                    temp = check_range(fields[0], -30.0, 50.0)
                    hum = check_range(fields[1], 0, 100)
                    soil = check_range(fields[2], 0, 100)
                    rain_min = check_range(fields[3], 0.0, 200.0)
                    total_rain = check_range(fields[4], 0.0, 300.0)
                    row = [timestamp, soil, temp, hum, rain_min, total_rain, None, None, None, src]
                    append_data(row)

                # Node 3: river height, rate of rise, high level alert
                elif src == 3:
                    if len(fields) != 3:
                        log_error(timestamp, src, text, "Unexpected number of fields")
                        return
                    river_height = check_range(fields[0], 0, 250)
                    rate_rise = check_range(fields[1], -250, 250)
                    high_alert = check_range(fields[2], 0, 1)
                    row = [timestamp, None, None, None, None, None, river_height, rate_rise, high_alert, src]
                    append_data(row)

                else:
                    log_error(timestamp, src, text, "Unknown node ID")

        except Exception as e:
            print(f"[ERROR] Decrypt or parse error: {e}")

        self.set_mode(MODE.SLEEP)
        self.reset_ptr_rx()
        BOARD.led_off()
        self.set_mode(MODE.RXCONT)

    def start(self):
        self.reset_ptr_rx()
        self.set_mode(MODE.RXCONT)
        while True:
            sleep(0.5)

if __name__ == "__main__":
    lora = LoRaRcvCont(verbose=False)
    args = parser.parse_args(lora)
    lora.set_mode(MODE.STDBY)
    lora.set_pa_config(pa_select=1)
    print(lora)
    assert lora.get_agc_auto_on() == 1

    # Start immediately without pressing enter
    lora.start()

