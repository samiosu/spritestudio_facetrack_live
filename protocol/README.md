# SpriteFace SF1 UDP protocol

The PC renderer accepts UTF-8 JSON UDP datagrams up to **2,048 bytes** on IPv4
port **11574**. The local OpenSeeFace adapter listens separately on loopback
port **11573**, using OpenSeeFace's binary protocol. Do not point OpenSeeFace
directly at the SF1 port.

```json
{"v":1,"type":"frame","token":"0123456789abcdef0123456789abcdef","seq":42,"tracking":true,"confidence":1,"head":{"yaw":0.1,"pitch":0,"roll":0},"eyes":{"left":1,"right":0.8},"mouth":{"open":0.4,"smile":0.2}}
```

The token above is a public example, not a production session token. The PC
creates a fresh 128-bit token for each session. Hexadecimal tokens are
canonicalized to lowercase. Fields are validated before
updating any state. Nonfinite values, invalid types, missing features, values
outside the ranges in `face-frame.schema.json`, unsupported versions and invalid
tokens are rejected. Unknown extension fields are ignored.

| Field | Convention |
| --- | --- |
| `seq` | Unsigned 32-bit sequence, increasing modulo 2^32; one sequence per sender |
| `tracking` | Face is available; false causes all features to become neutral |
| `confidence` | 0–1 availability/confidence indicator; current mobile clients use 1/0, not a calibrated confidence score |
| `head` | Radians, unmirrored input; Euler decomposition `Rz(roll) * Ry(yaw) * Rx(pitch)` |
| `eyes.left/right` | Subject's anatomical left/right; 0 closed, 1 open |
| `mouth.open/smile` | 0–1 expression weights |

MediaPipe clients normalize rotation-matrix columns, then calculate
`yaw = asin(-R20)`, `pitch = atan2(R21,R22)`, `roll = atan2(R10,R00)`.
The preview may be mirrored, but the image passed to inference is not. No device
timestamps are compared across machines. The PC uses a monotonic local receive
time. OpenSeeFace expression features are relative to its running calibration;
the adapter converts its official pose offsets to this convention.

Each accepted frame replaces the previous sample. Duplicates and older sequences
are discarded. A first active peer is pinned by its IPv4 address and source port.
Another peer can take over after **800 ms** without an accepted frame; this also
allows sequence reset after a stopped client restarts. Received `tracking:false`
frames remain live connection traffic. A lost face or stale connection decays
smoothly toward neutral on the rendering thread.

## Pairing

QR content (with a fresh real session token) is:

```text
spriteface://pair?v=1&host=192.168.1.10&port=11574&token=0123456789abcdef0123456789abcdef
```

Use the PC's reachable LAN IPv4 address, not `127.0.0.1` or `0.0.0.0`. Phones
need camera permission and any local-network permission required by their OS.
Initial scope is one phone and a PC on the same trusted LAN, using unicast UDP.

The client sends `{"v":1,"type":"hello","token":"..."}` from the same UDP
socket used for frames. A valid request receives `{"v":1,"type":"ready"}`.
If another active sender holds the session, the reply is `{"v":1,"type":"busy"}`.
Invalid requests receive no reply. The hello exchange does not refresh the face
sample. Clients must only accept a reply from their configured PC endpoint.

The token prevents accidental cross-session control. **UDP traffic is not
encrypted**, and pairing does not authenticate a device against a hostile LAN.
This listener is not intended for exposure to the public Internet. A future
remote transport can reuse these features with authenticated encryption.
