# SpriteFace SF2 UDP protocol

The PC accepts IPv4 UDP datagrams up to **2,048 bytes** on port **11574**. Every datagram, including pairing replies, uses this binary envelope:

```text
ASCII "SF2" (3 bytes) || random nonce (12 bytes) || AES-128-GCM ciphertext || tag (16 bytes)
```

The 32-hex-digit pairing token is decoded into the 16-byte AES key. The ASCII `SF2` prefix is authenticated as additional data. A fresh, cryptographically random 12-byte nonce is generated for **each packet**, in either direction. The PC rejects plaintext, older wire formats, wrong keys, modified packets and oversized packets without processing their JSON. The token appears in the QR or manual pairing input; it never appears in cleartext UDP traffic. Keep the QR and token private. Rotate the token when pairing with a different device.

The decrypted content is UTF-8 JSON. Its `v` field remains `1` to preserve the face-value schema, but the wire envelope and QR URI are **version 2**. A face frame contains:

```json
{"v":1,"type":"frame","token":"0123456789abcdef0123456789abcdef","client":"11111111111111111111111111111111","server":"22222222222222222222222222222222","seq":42,"tracking":true,"confidence":1,"head":{"yaw":0.1,"pitch":0,"roll":0},"eyes":{"left":1,"right":0.8},"mouth":{"open":0.4,"smile":0.2}}
```

The values above are examples only. The `token`, `client` and `server` fields are **inside the authenticated ciphertext**. `client` and `server` are fresh 128-bit random challenges for the current connection. The server validates them before accepting a frame. `seq` is an unsigned 32-bit counter that increases modulo 2^32 in that connection. Duplicate or older sequences are rejected even after the previous frame expires. New connections require a new challenge exchange.

| Field | Convention |
| --- | --- |
| `tracking` | Face available; false resets features to neutral |
| `confidence` | 0–1 availability indicator; mobile clients currently use 1/0 |
| `head` | Radians, unmirrored input; `Rz(roll) * Ry(yaw) * Rx(pitch)` |
| `eyes.left/right` | Subject's anatomical left/right; 0 closed, 1 open |
| `mouth.open/smile` | 0–1 expression weights |

MediaPipe clients normalize rotation-matrix columns, then calculate `yaw = asin(-R20)`, `pitch = atan2(R21,R22)` and `roll = atan2(R10,R00)`. The PC uses local monotonic receive time; it does not compare clocks across devices. A stale face smoothly returns to neutral. The local OpenSeeFace adapter listens on **127.0.0.1:11573** for OpenSeeFace's separate binary protocol, then sends encrypted SF2 datagrams to the PC receiver. Do not point OpenSeeFace directly at port 11574.

## Pairing

The PC displays a fresh token and a QR URI:

```text
spriteface://pair?v=2&host=192.168.1.10&port=11574&token=0123456789abcdef0123456789abcdef
```

Use the PC's reachable LAN IPv4 address. The sender creates a fresh `client` challenge and sends an encrypted `{"v":1,"type":"hello","token":"...","client":"..."}` from the socket used for frames. The PC replies on that socket with an encrypted `ready` containing the matching `client` and a fresh `server` challenge. While another sender has delivered a frame within 800 ms, the PC instead returns encrypted `busy`. Invalid packets receive no reply. A retry of the same hello keeps its challenge; a new connection gets a new server challenge. Senders repeat hello every second to recover after a PC restart and use the latest server challenge in subsequent frames. A replayed old hello can elicit a new ready, but its old frames cannot match the new server challenge.

The receiver pins the active IPv4 address and UDP source port. Another sender may pair after 800 ms without an accepted frame. The token is a shared key, so anyone who obtains it can pair; transport encryption does not prevent use of a disclosed QR/token. The listener is intended for direct PC and phone use on a LAN, with no router port forwarding.
