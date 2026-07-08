# M0rtis Wire Format

Version: `0.0.0`

---

## Overview

TCP is a stream protocol. It delivers a continuous sequence of bytes with no concept of where one message ends and the next begins. M0rtis uses a **length-prefix framing** scheme to define message boundaries.

Every message is sent as two parts:

```
[ 4-byte length prefix ][ JSON payload ]
```

The length prefix tells the receiver exactly how many bytes to read for the payload. The receiver reads the prefix first, then reads exactly that many bytes for the message body.

---

## Length Prefix

| Property | Value |
|---|---|
| Size | 4 bytes |
| Type | Unsigned 32-bit integer |
| Byte order | Big-endian (network byte order) |
| Value | Byte length of the JSON payload |

The length prefix encodes the size of the payload in bytes, not characters. For UTF-8 encoded JSON containing only ASCII characters these are the same. For messages containing non-ASCII characters (e.g. unicode in string values), byte length may be greater than character count.

---

## Payload

| Property | Value |
|---|---|
| Format | JSON |
| Encoding | UTF-8 |
| Maximum size | 65,536 bytes (64 KB) |

The payload is a UTF-8 encoded JSON object. It must be valid JSON. It must not exceed 64 KB. There is no compression in Phase 0.

---

## Wire Layout

```
Byte offset   Content
───────────   ───────────────────────────────────────
0             Length prefix byte 0 (most significant)
1             Length prefix byte 1
2             Length prefix byte 2
3             Length prefix byte 3 (least significant)
4 .. 4+N-1   JSON payload (N bytes)
```

---

## Example

Sending the message:

```json
{"version":"0.0.0","type":"handshake","node_id":"room1","node_type":"auralens"}
```

1. Encode the JSON as UTF-8 bytes. Length = 72 bytes.
2. Encode 72 as a 4-byte big-endian unsigned integer: `0x00 0x00 0x00 0x48`
3. Send prefix then payload:

```
00 00 00 48 7B 22 76 65 72 73 69 6F 6E 22 3A 22  ...{"version":"
30 2E 30 2E 30 22 2C 22 74 79 70 65 22 3A 22 68  0.0.0","type":"h
61 6E 64 73 68 61 6B 65 22 2C ...                andshake",...
```

---

## Receiving a Message

```
1. Read exactly 4 bytes → length prefix
2. Decode prefix as big-endian uint32 → N
3. If N > 65536 → close connection (oversized)
4. Read exactly N bytes → payload bytes
5. Decode payload bytes as UTF-8 → JSON string
6. Parse JSON string → message object
```

If any read returns fewer bytes than expected (connection closed mid-message), treat the connection as lost and clean up.

---

## Sending a Message

```
1. Serialize message object → JSON string
2. Encode JSON string as UTF-8 → payload bytes
3. N = byte length of payload bytes
4. If N > 65536 → do not send, log error
5. Encode N as big-endian uint32 → 4-byte prefix
6. Send prefix bytes, then payload bytes
```

Prefix and payload must be sent as a single write where possible to avoid partial sends creating ambiguous stream state.

---

## Why Length-Prefix and Not Delimiter

An alternative framing scheme uses a delimiter character (e.g. newline `\n`) to mark the end of each message. This is simpler to implement but breaks when the delimiter appears inside the payload — for example in a JSON string value. Since M0rtis payloads are arbitrary JSON, a delimiter approach would require escaping, which adds complexity. Length-prefix has no such ambiguity.
