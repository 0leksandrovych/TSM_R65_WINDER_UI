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
0x09  WINDER_LINK_MSG_ABORT_JOB
0x0A  WINDER_LINK_MSG_RESET_ALARM
0x0B  WINDER_LINK_MSG_RESET_UNWOUND_COUNTER
0x0C  WINDER_LINK_MSG_SET_SPEED_OVERRIDE
0x0D  WINDER_LINK_MSG_APPLY_EDGE_TRIM
0x0E  WINDER_LINK_MSG_GET_TELEMETRY
0x0F  WINDER_LINK_MSG_RESET_JOB
0x10  WINDER_LINK_MSG_FINISH_JOB
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

`COMMAND_ACCEPTED` only confirms that the controller accepted the command for
processing. It does not report operation completion and must not cause the HMI
to synthesize a machine-state transition. Machine state changes come only from
a later `STATE_SNAPSHOT`.

The following commands have an empty payload:

```text
WINDER_LINK_MSG_START_HOMING
WINDER_LINK_MSG_ABORT_HOMING
WINDER_LINK_MSG_PAUSE_JOB
WINDER_LINK_MSG_RESUME_JOB
WINDER_LINK_MSG_ABORT_JOB
WINDER_LINK_MSG_FINISH_JOB
WINDER_LINK_MSG_RESET_JOB
```

### Job lifecycle command availability

`WINDER_LINK_MSG_ABORT_JOB` requests an immediate stop of the active job. It is
available while the machine is in motion: `LINK_MACHINE_STATE_ACCELERATING`,
`LINK_MACHINE_STATE_RUNNING`, or `LINK_MACHINE_STATE_STOPPING`. Its accepted
outcome is `LINK_MACHINE_STATE_FINISHED`.

`WINDER_LINK_MSG_FINISH_JOB` requests a graceful completion of a paused job. It
is available only while the machine is in `LINK_MACHINE_STATE_PAUSED`. Its
accepted outcome is `LINK_MACHINE_STATE_FINISHED`.

`WINDER_LINK_MSG_RESET_JOB` requests the controller transition from
`LINK_MACHINE_STATE_FINISHED` to `LINK_MACHINE_STATE_HOMING_REQUIRED`. Its
acceptance does not complete the transition; the following `STATE_SNAPSHOT` is
the source of truth for the resulting machine state.

For all three commands, `COMMAND_ACCEPTED` only confirms that the controller
accepted the command for processing. The machine-state transition is reported
solely by a later `STATE_SNAPSHOT`. The HMI must not synthesize
`FINISHED`, `PAUSED`, `RUNNING`, or `HOMING_REQUIRED` from an ACK alone.

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
1  LINK_FIELD_MACHINE_STATE         link_machine_state_t, scale x1
2  LINK_FIELD_JOB_MASTER_SPEED      float,     scale x100  -> centi-rps
3  LINK_FIELD_JOB_WINDING_PITCH     float,     scale x100  -> centi-mm
4  LINK_FIELD_JOB_TARGET_LENGTH     float,     scale x1000 -> mm
5  LINK_FIELD_JOB_SHIFT_EVERY       uint,      scale x1    -> layers
6  LINK_FIELD_JOB_RIGHT_EDGE_SHIFT  float,     scale x100  -> centi-mm
7  retired legacy homing sub-state field (ignored by the new HMI)
8  LINK_FIELD_TRAVEL_RANGE_MM       double,    scale x100  -> centi-mm
9  LINK_FIELD_MASTER_SPEED_RPS      float,     scale x100  -> centi-rps
10 LINK_FIELD_WOUND_LENGTH_M        double,    scale x1000 -> millimetres on wire
11 LINK_FIELD_COMPLETED_LAYERS      uint,      scale x1    -> layers
12 LINK_FIELD_APPLIED_RIGHT_EDGE_OFFSET_MM
                                      double,  scale x100  -> centi-mm
```

`LINK_FIELD_JOB_MASTER_SPEED` is the configured master speed from the active
job/context. `LINK_FIELD_MASTER_SPEED_RPS` is the current runtime speed of the
master motor. They are separate values and may both be present in one snapshot.

`LINK_FIELD_JOB_TARGET_LENGTH` carries an `int32` wire value scaled by 1000.
The wire value is expressed in millimeters, and the HMI decoder exposes the
decoded value in meters (`scaled_value / 1000.0`).

`LINK_FIELD_MACHINE_STATE` carries a `link_machine_state_t` value. It must
not carry a generated controller runtime state ID. Homing progress is represented
only by detailed `link_machine_state_t` values. Unknown field IDs, including the
retired field ID 7, are ignored without rejecting the rest of the snapshot.

`LINK_FIELD_TRAVEL_RANGE_MM` carries an `int32` wire value scaled by 100. The
HMI decodes it as `scaled_value / 100.0`; zero is a present value and is distinct
from an omitted field.

`LINK_FIELD_MASTER_SPEED_RPS` carries an `int32` wire value scaled by 100. The
HMI decodes it as `scaled_value / 100.0` revolutions per second; zero is a valid
present runtime speed and is distinct from an omitted field.

`LINK_FIELD_WOUND_LENGTH_M` carries the controller-context wound length in
meters as an `int32` scaled by 1000. The wire value is numerically millimeters;
the HMI decoder exposes `scaled_value / 1000.0` meters.

`LINK_FIELD_COMPLETED_LAYERS` carries the completed-layer count with scale 1.

`LINK_FIELD_JOB_RIGHT_EDGE_SHIFT` is the configured per-step job parameter.
It is not the current applied offset. `LINK_FIELD_APPLIED_RIGHT_EDGE_OFFSET_MM`
carries the accumulated runtime offset as an `int32` scaled by 100, and the HMI
decoder exposes `scaled_value / 100.0` millimeters.

The numeric values of the machine-state enum are a stable part of the wire contract:

```text
link_machine_state_t
0   LINK_MACHINE_STATE_HOMING_REQUIRED
1   LINK_MACHINE_STATE_HOMING_SEARCHING_RIGHT_REFERENCE
2   LINK_MACHINE_STATE_HOMING_BACKING_OFF_RIGHT_REFERENCE
3   LINK_MACHINE_STATE_HOMING_SEARCHING_LEFT_REFERENCE
4   LINK_MACHINE_STATE_HOMING_BACKING_OFF_LEFT_REFERENCE
5   LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL
6   LINK_MACHINE_STATE_HOMING_APPLYING_OFFSET
7   LINK_MACHINE_STATE_HOMING_COMPLETING
8   LINK_MACHINE_STATE_READY
9   LINK_MACHINE_STATE_ACCELERATING
10  LINK_MACHINE_STATE_RUNNING
11  LINK_MACHINE_STATE_PAUSED
12  LINK_MACHINE_STATE_STOPPING
13  LINK_MACHINE_STATE_FINISHED
14  LINK_MACHINE_STATE_ALARM
```

These values must not be renumbered or coupled to generated state-machine IDs.
New values require an explicit backward-compatible contract extension.

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
