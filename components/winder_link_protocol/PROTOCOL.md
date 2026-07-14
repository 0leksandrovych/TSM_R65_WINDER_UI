# Winder Link Protocol Contract

## 1. Purpose

`winder_link_protocol` is the shared binary protocol contract between the HMI UI controller and the main machine controller.

The component is intentionally neutral. It defines how bytes become protocol frames, how primitive payload fields are encoded, which shared message IDs exist, and which constants both sides must use.

## 2. Layer Responsibility

This component owns:

```text
bytes <-> frame
primitive payload read/write helpers
shared message IDs
shared protocol constants
```

This component does not own:

```text
UART driver
FreeRTOS task
LVGL
HMI screens
HMI controller client
machine runtime logic
motor control
job validation logic
```

## 3. Frame Format

All frames use this byte layout:

```text
Field     Type     Value / meaning
--------  -------  -----------------------------------------------
SOF1      uint8    0xAA
SOF2      uint8    0x55
VERSION   uint8    0x01
TYPE      uint8    message type
SEQ       uint16   little-endian sequence number
LENGTH    uint16   little-endian payload length
PAYLOAD   uint8[]  0..WINDER_LINK_MAX_PAYLOAD_SIZE bytes
CRC16     uint16   little-endian CRC-16/CCITT-FALSE
```

The CRC algorithm is CRC-16/CCITT-FALSE:

```text
Polynomial : 0x1021
Initial    : 0xFFFF
RefIn/Out  : false
Final XOR  : 0x0000
```

CRC covers exactly:

```text
VERSION, TYPE, SEQ_L, SEQ_H, LENGTH_L, LENGTH_H, PAYLOAD...
```

`SOF1` and `SOF2` are not included in the CRC.

## 4. Endianness

All multi-byte integer fields in the frame header and payload are little-endian.

## 5. Message Type Ranges

HMI -> main controller:

```text
0x01  WINDER_LINK_MSG_PING
0x02  WINDER_LINK_MSG_GET_CAPABILITIES
0x03  WINDER_LINK_MSG_VALIDATE_JOB
0x04  WINDER_LINK_MSG_START_JOB
0x05  WINDER_LINK_MSG_START_HOMING
0x06  WINDER_LINK_MSG_ABORT_HOMING
0x07  WINDER_LINK_MSG_PAUSE_JOB
0x08  WINDER_LINK_MSG_RESUME_JOB
0x09  WINDER_LINK_MSG_STOP_JOB
0x0A  WINDER_LINK_MSG_RESET_ALARM
0x0B  WINDER_LINK_MSG_RESET_UNWOUND_COUNTER
0x0C  WINDER_LINK_MSG_SET_SPEED_OVERRIDE
0x0D  WINDER_LINK_MSG_APPLY_EDGE_TRIM
0x0E  WINDER_LINK_MSG_GET_TELEMETRY
```

Main controller -> HMI:

```text
0x80  WINDER_LINK_MSG_PONG
0x81  WINDER_LINK_MSG_HEARTBEAT
0x82  WINDER_LINK_MSG_CAPABILITIES_SNAPSHOT
0x83  WINDER_LINK_MSG_STATE_SNAPSHOT
0x84  WINDER_LINK_MSG_COMMAND_ACCEPTED
0x85  WINDER_LINK_MSG_COMMAND_REJECTED
0x86  WINDER_LINK_MSG_JOB_VALIDATION_RESULT
0x87  WINDER_LINK_MSG_ALARM_EVENT
```

## 6. Sequence Number Rule

The HMI assigns `SEQ` for HMI -> controller commands.

The controller may assign its own `SEQ` for unsolicited controller -> HMI messages.

`COMMAND_ACCEPTED` and `COMMAND_REJECTED` payloads must reference the original command using:

```text
original_seq
original_type
```

## 7. ACK / REJECT Payload Contract

Planned payload for `WINDER_LINK_MSG_COMMAND_ACCEPTED`:

```text
original_seq    uint16 little-endian
original_type   uint8
```

Planned payload for `WINDER_LINK_MSG_COMMAND_REJECTED`:

```text
original_seq    uint16 little-endian
original_type   uint8
reason_code     uint16 little-endian
```

The base contract does not include string reason text. Reason text can be added later only as a bounded optional field if explicitly needed.

## 8. Job Command Payload Contract

Payload for `WINDER_LINK_MSG_START_JOB`:

```text
param_count   uint8
repeated param_count times:
  param_id    uint16 little-endian
  scaled_value int32 little-endian
```

`scaled_value` is a scaled integer, not a raw float. The exact scale is
defined by `link_param_id_t` in winder_link_contract.h, not by a wire byte.
No mode_id or value_type field is sent - the controller currently supports
a single job command with no mode dispatch.

`WINDER_LINK_MSG_VALIDATE_JOB` is not implemented by the controller yet;
this payload mapping does not apply to it until controller support exists.

## 9. State Snapshot Payload Contract

Payload for `WINDER_LINK_MSG_GET_TELEMETRY`:  (empty)

Payload for `WINDER_LINK_MSG_STATE_SNAPSHOT`:

```text
field_count    uint8
repeated field_count times:
  field_id     uint16 little-endian
  scaled_value int32 little-endian
```

Implemented field IDs:

```text
1  LINK_FIELD_MACHINE_STATE         enum/code, scale x1
2  LINK_FIELD_JOB_MASTER_SPEED      float,     scale x100  -> centi-rps
3  LINK_FIELD_JOB_WINDING_PITCH     float,     scale x100  -> centi-mm
4  LINK_FIELD_JOB_TARGET_LENGTH     float,     scale x1000 -> mm
5  LINK_FIELD_JOB_SHIFT_EVERY       uint,      scale x1    -> layers
6  LINK_FIELD_JOB_RIGHT_EDGE_SHIFT  float,     scale x100  -> centi-mm
```

## 10. Numeric Encoding Rules

No raw C structs are sent over the link.

No pointers are sent over the link.

No runtime strings are sent in normal state snapshots.

Prefer enum/code values over text.

Prefer scaled integers over floats.

Examples:

```text
2.50 rps   -> 250 centi-rps
85.0%      -> 850 permille
120.5 mm   -> 1205 deci-mm if such scale is chosen
```

## 11. Maximum Payload Rule

The current maximum payload is:

```text
WINDER_LINK_MAX_PAYLOAD_SIZE = 256
```

The limit is bounded to keep the embedded implementation simple and predictable:

```text
no heap allocation
predictable memory usage
protection against malformed frames
simple embedded decoder
```

Large future messages, such as capabilities snapshots, must either:

```text
fit within this limit
be explicitly chunked
use a more compact schema
```

No implicit oversized payloads are allowed.

## 12. Versioning Rule

The current protocol version is `0x01`.

Unsupported versions are rejected by the decoder.

Payload formats must not be changed incompatibly without versioning or new message type IDs.

## 13. Non-Goals

This document does not define UART pins, baudrate, task priorities, motor behavior, LVGL behavior, or machine safety policy.
