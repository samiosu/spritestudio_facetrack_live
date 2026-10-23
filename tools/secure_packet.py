"""SF2 authenticated UDP envelope shared by the demo and OpenSeeFace bridge."""

from __future__ import annotations

import os
import re
import secrets

from cryptography.hazmat.primitives.ciphers.aead import AESGCM

MAGIC = b"SF2"


def challenge() -> str:
    return secrets.token_hex(16)


def valid_challenge(value: str) -> bool:
    return isinstance(value, str) and bool(re.fullmatch(r"[0-9a-f]{32}", value))


def _key(token: str) -> bytes:
    if not re.fullmatch(r"[0-9a-fA-F]{32}", token):
        raise ValueError("pairing token must be 32 hexadecimal characters")
    return bytes.fromhex(token)


def seal(plaintext: bytes, token: str) -> bytes:
    nonce = os.urandom(12)
    return MAGIC + nonce + AESGCM(_key(token)).encrypt(nonce, plaintext, MAGIC)


def open_packet(packet: bytes, token: str) -> bytes:
    if len(packet) < len(MAGIC) + 12 + 16 or not packet.startswith(MAGIC):
        raise ValueError("unsupported or unencrypted packet")
    nonce = packet[len(MAGIC):len(MAGIC) + 12]
    ciphertext = packet[len(MAGIC) + 12:]
    return AESGCM(_key(token)).decrypt(nonce, ciphertext, MAGIC)
