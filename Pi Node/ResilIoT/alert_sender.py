# alert_sender.py

import os
import socket
import struct
import threading
from datetime import datetime
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305

# -----------------------
# Key: Replace in production code.
# -----------------------
WIFI_CHACHA_KEY = bytes([
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
])

# Path to persist the 8-byte counter
COUNTER_FILE_PATH = os.path.expanduser("~/.resiliot_nonce_counter")

# Network defaults
UDP_PORT = 5005
BROADCAST_ADDR = '<broadcast>'
DEFAULT_WIFI_INTERFACE = 'wlan0'

# Internal lock for file operations (in case multiple threads call send simultaneously)
_file_lock = threading.Lock()


def _read_counter_from_file(path: str) -> int:

    if not os.path.exists(path):
        return None
    try:
        with open(path, "rb") as f:
            b = f.read(8)
            if len(b) != 8:
                return None
            return int.from_bytes(b, byteorder="big")
    except Exception:
        return None


def _write_counter_to_file(path: str, counter: int) -> None:
    """Write the 8-byte big-endian counter atomically."""
    temp = f"{path}.tmp"
    with open(temp, "wb") as f:
        f.write(counter.to_bytes(8, byteorder="big"))
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp, path)


def _get_and_increment_persistent_counter(path: str) -> int:
    """
    Safely read the stored counter, increment it, persist, and return the previous value.
    If no counter exists, seed it with a random 8-byte value.
    """
    with _file_lock:
        current = _read_counter_from_file(path)
        if current is None:
            # seeds with an unpredictable start to avoid accidental reuse
            current = int.from_bytes(os.urandom(8), byteorder="big")
        next_val = (current + 1) & ((1 << 64) - 1)
        _write_counter_to_file(path, next_val)
        return current


def _build_nonce() -> bytes:

    seconds = int(datetime.utcnow().timestamp()) & 0xFFFFFFFF
    counter_val = _get_and_increment_persistent_counter(COUNTER_FILE_PATH)
    nonce = struct.pack(">I", seconds) + counter_val.to_bytes(8, byteorder="big")
    assert len(nonce) == 12
    return nonce


def encrypt_alert_message(plaintext: str, key: bytes = WIFI_CHACHA_KEY) -> bytes:
    """
    Returns bytes = nonce (12) || ciphertext || tag (16)
    """
    if not isinstance(plaintext, str):
        raise TypeError("plaintext must be a str")
    aead = ChaCha20Poly1305(key)
    nonce = _build_nonce()
    ciphertext_and_tag = aead.encrypt(nonce, plaintext.encode("utf-8"), associated_data=None)
    return nonce + ciphertext_and_tag


def send_encrypted_alert_broadcast(alert_text: str,
                                   wifi_interface: str = DEFAULT_WIFI_INTERFACE,
                                   port: int = UDP_PORT,
                                   timeout_s: float = 1.0):
    """
    Encrypts and broadcasts alert_text as a UDP packet. Also
    attempts to bind the socket to the specified interface.
    If that fails (not run as root or not supported), it will still send and the OS routing table will choose interface.
    """
    payload = encrypt_alert_message(alert_text)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        # Trys binding to the wireless device so packet goes out via wlan0
        try:

            sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, wifi_interface.encode() + b'\0')
        except Exception:
            # Not fatal: fallback to default route
            pass

        sock.settimeout(timeout_s)
        sock.sendto(payload, (BROADCAST_ADDR, port))
    finally:
        try:
            sock.close()
        except Exception:
            pass
