# mBot mCore I2C Interface

This document describes the current I2C protocol exposed by the `mbot-firmware` project.
It is intended to be precise enough for another AI agent or developer to implement a compatible I2C master client.

## Hardware role

- **mCore / mBot firmware**: I2C slave
- **StackChan / ESP32 side**: I2C master
- **I2C slave address**: `0x10`
- **Recommended bus speed**: `100 kHz`

## Current physical device map

| Device | Connection |
|---|---|
| StackChan ↔ mBot I2C | **M5Stack CoreS3 HY2.0 Port A** (red): **SDA = GPIO2** (yellow), **SCL = GPIO1** (white), **5 V + GND** → mBot **Port 1** RJ25 SDA/SCL breakout. **Not** the internal I2C (GPIO12/11). If the LED matrix stays dark at boot, confirm **5 V** on Port A. |
| StackChan (internal I2C) | On-board PMIC, touch, codec, camera SCCB on GPIO12/11 — **do not** wire mBot here. |
| Servo | Port 1 via Me RJ25 Adapter, Slot 2 |
| Line follower | Port 2 |
| Ultrasonic sensor | Port 3 |
| 8x16 LED matrix | Port 4 |
| Motors | mCore motor outputs M1 / M2 |
| RGB LEDs | mCore onboard LEDs |
| Buzzer | mCore onboard buzzer |

## Connection semantics

The mCore firmware considers the master "connected" after it receives the first valid protocol packet.

Boot behavior:

1. LED matrix shows `BOOT`
2. Servo self-check runs: center -> left 10 degrees -> right 10 degrees -> center
3. Motors perform a short forward/backward test pulse
4. Buzzer beeps once
5. LED matrix shows `I2C?`
6. RGB LED blinks quickly while waiting for the first valid packet
7. After first valid packet, LED matrix shows `OK` briefly and then clears

## Packet format

All requests and responses use the same frame format:

```text
[START][LEN][CMD][PAYLOAD...][CRC]
```

| Field | Size | Description |
|---|---:|---|
| `START` | 1 byte | Always `0xAA` |
| `LEN` | 1 byte | Number of bytes from `CMD` through `CRC`, inclusive |
| `CMD` | 1 byte | Command identifier |
| `PAYLOAD` | 0..24 bytes | Command-specific data |
| `CRC` | 1 byte | CRC-8 over `LEN`, `CMD`, and `PAYLOAD` |

### Size limits

- Maximum packet size: `32` bytes
- Maximum payload size: `24` bytes
- I2C buffer size: `32` bytes

### CRC

- Algorithm: CRC-8
- Polynomial: `0x07`
- Initial value: `0x00`
- Input bytes: all bytes from `LEN` through the last payload byte
- The `START` byte is **not** included in the CRC

Pseudo-code:

```cpp
uint8_t crc = 0x00;
for each byte in [LEN, CMD, PAYLOAD...] {
    crc ^= byte;
    repeat 8 times {
        crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
    }
}
```

## Master transaction pattern

Typical request/response flow:

1. Master writes one complete request frame to slave address `0x10`
2. mCore processes the frame in `loop()`
3. Master performs an I2C read from slave address `0x10`
4. mCore returns the most recent response frame

The firmware intentionally does not execute business logic inside the I2C receive interrupt.

## Commands

| Command | Hex | Request payload | Response payload |
|---|---:|---|---|
| Set motors | `0x01` | `left:int8`, `right:int8` | none |
| Get distance | `0x02` | none | `distance_cm:uint16_be` |
| Display text | `0x03` | ASCII bytes | none |
| Clear display | `0x04` | none | none |
| Get status | `0x05` | none | `healthy:uint8`, `packet_count:uint16_be` |
| Ping | `0x06` | none | none |
| Display bitmap | `0x07` | `width:uint8`, `bitmap:bytes...` | none |
| Stop motors | `0x08` | none | none |
| Set RGB LED | `0x09` | `red:uint8`, `green:uint8`, `blue:uint8` | none |
| Play tone | `0x0A` | `frequency_hz:uint16_be`, `duration_ms:uint16_be` | none |
| Get line follower | `0x0B` | none | `state:uint8` |
| Set servo angle | `0x0C` | `angle:uint8` | none |

## Command details

### `0x01` Set motors

Request payload:

```text
left:int8
right:int8
```

Expected range:

- `-100..100`

The firmware maps this to the mCore motor driver range internally.

Example: forward at 50% on both motors

```text
AA 04 01 32 32 CRC
```

### `0x02` Get distance

Response payload:

```text
distance_cm:uint16_be
```

Example response payload for `42 cm`:

```text
00 2A
```

### `0x03` Display text

Request payload:

- raw ASCII text bytes
- maximum length: `24` bytes

The current firmware renders text on the LED matrix using the Makeblock matrix library.

### `0x04` Clear display

No request payload.

### `0x05` Get status

Response payload:

```text
healthy:uint8
packet_count:uint16_be
```

Meaning:

- `healthy = 1` currently indicates normal firmware status
- `packet_count` is the count of valid processed packets since boot

### `0x06` Ping

No request payload.

Useful for:

- connection establishment
- health checks
- triggering the boot-state transition from `I2C?` to `OK`

### `0x07` Display bitmap

Request payload:

```text
width:uint8
bitmap:bytes...
```

The bitmap bytes are passed to `MeLEDMatrix::drawBitmap(...)`.

### `0x08` Stop motors

No request payload.

Also feeds the motor watchdog timer.

### `0x09` Set RGB LED

Request payload:

```text
red:uint8
green:uint8
blue:uint8
```

Each component is `0..255`.

### `0x0A` Play tone

Request payload:

```text
frequency_hz:uint16_be
duration_ms:uint16_be
```

If `frequency_hz == 0`, the buzzer is stopped.

### `0x0B` Get line follower

Response payload:

```text
state:uint8
```

State values from the Makeblock line follower:

| Value | Meaning |
|---:|---|
| `0x00` | Sensor 1 in, Sensor 2 in |
| `0x01` | Sensor 1 in, Sensor 2 out |
| `0x02` | Sensor 1 out, Sensor 2 in |
| `0x03` | Sensor 1 out, Sensor 2 out |

### `0x0C` Set servo angle

Request payload:

```text
angle:uint8
```

Accepted range:

- `0..180`

Values above `180` are clamped to `180`.

## Response format

Responses repeat the request command in the `CMD` field.

Examples:

### Successful ping response

```text
AA 02 06 CRC
```

### Distance response with 42 cm

```text
AA 04 02 00 2A CRC
```

## Error handling

The current firmware validates incoming packets and internally tracks these status codes:

| Code | Meaning |
|---:|---|
| `0x00` | OK |
| `0x01` | Bad packet |
| `0x02` | Bad length |
| `0x03` | Bad CRC |
| `0x04` | Unknown command |

Important current behavior:

- invalid packets are logged to serial debug output
- invalid packets do **not** currently generate a separate I2C error response frame

## Motor watchdog

- Timeout: `500 ms`
- If no motor command refreshes the watchdog within the timeout, the firmware stops the motors automatically
- `SET_MOTORS` and `STOP_MOTORS` both feed the watchdog

## Suggested minimal master startup sequence

1. Initialize I2C master
2. Send `PING`
3. Read the ping response
4. Optionally send `GET_STATUS`
5. Begin normal control loop

## Example minimal transactions

### Ping

Request:

```text
AA 02 06 CRC
```

Response:

```text
AA 02 06 CRC
```

### Read ultrasonic distance

Request:

```text
AA 02 02 CRC
```

Response:

```text
AA 04 02 HH LL CRC
```

### Set servo to 90 degrees

Request:

```text
AA 03 0C 5A CRC
```

Response:

```text
AA 02 0C CRC
```

### Read line follower

Request:

```text
AA 02 0B CRC
```

Response:

```text
AA 03 0B STATE CRC
```

## Implementation notes for AI agents

- Treat all multi-byte integers as **big-endian**
- Keep payloads within the documented size limits
- Always verify CRC on both requests and responses
- Do not assume an immediate response is available before the mCore main loop has processed the request
- A short delay or retry loop between write and read may be needed on the master side
- Use `PING` as the safest first command during boot
- The servo on the RJ25 adapter is a **PWM servo on Slot 2**, not an I2C servo
- The mCore is the I2C slave; StackChan / ESP32 should be the I2C master
