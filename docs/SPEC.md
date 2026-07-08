# M0rtis Protocol Specification

Version: `0.0.0`

---

## Overview

M0rtis operates over TCP. An Auralens node connects to the AXIOM hub, performs a handshake, and then streams inference events for the duration of the connection. The connection is persistent — a node does not connect per-event, it connects once and holds the connection open.

---

## Transport

| Property | Value |
|---|---|
| Protocol | TCP |
| Default port | `7373` |
| Byte order | Big-endian (network byte order) |
| Encoding | UTF-8 |
| Connection model | Persistent, one connection per node |

TCP is chosen for its reliability and ordering guarantees. Events emitted by a node will arrive at AXIOM in the same order they were sent, and no event will be silently dropped.

---

## Connection Lifecycle

### 1. Connect

The Auralens node opens a TCP connection to the AXIOM hub on the configured host and port.

### 2. Handshake

Immediately after connection, the node sends a `handshake` message. This is always the first message on any connection. AXIOM will discard any connection that does not send a valid handshake as its first message.

The handshake carries:
- Protocol version
- Node identity
- Node type

### 3. Event stream

After a successful handshake the node may send `event` messages at any time. There is no acknowledgement in Phase 0 — the node sends and the hub receives.

### 4. Disconnect

Either side may close the connection at any time. AXIOM logs node disconnection and marks the node as offline. The node is responsible for reconnection — it should attempt to reconnect with exponential backoff.

### 5. Reconnect

A reconnecting node sends a fresh handshake on the new connection. AXIOM treats it as a new connection from the same node identity.

---

## Versioning

Every message carries a `version` field with the value `"0.0.0"`. This field is checked by AXIOM on every received message.

Version format: `MAJOR.MINOR.PATCH`

| Change type | Version increment |
|---|---|
| Breaking wire format change | MAJOR |
| New message type added | MINOR |
| Non-breaking fix or clarification | PATCH |

In Phase 0, AXIOM rejects any message where `version` does not match the expected value and closes the connection. Future phases may introduce negotiation.

---

## Framing

All messages are framed with a 4-byte length prefix followed by the UTF-8 encoded JSON payload. See `FRAMING.md` for the full wire format.

---

## Error Handling

### Invalid handshake

If the first message on a connection is not a valid `handshake`, or the version is unsupported, AXIOM closes the connection immediately with no response.

### Malformed message

If a received message cannot be parsed as valid JSON, or is missing required fields, AXIOM logs the error and closes the connection.

### Oversized message

If the length prefix indicates a payload larger than the maximum allowed size (64 KB in Phase 0), AXIOM closes the connection immediately.

### Node disconnection

If AXIOM detects that a node has disconnected (read returns zero bytes or an error), it logs the disconnection, marks the node offline, and releases the connection resources. It does not attempt to reconnect on behalf of the node.

---

## Constraints — Phase 0

- Maximum message payload: 64 KB
- Maximum simultaneous nodes: unbounded (limited by system resources)
- No message acknowledgement
- No flow control
- No encryption
- No authentication
- No automatic node discovery
- Direction: Auralens → AXIOM only
