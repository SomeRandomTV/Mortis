# M0rtis Message Catalog

Version: `0.0.0`

---

## Common Fields

Every M0rtis message regardless of type carries these fields:

| Field | Type | Required | Description |
|---|---|---|---|
| `version` | string | yes | Protocol version. Must be `"0.0.0"` |
| `type` | string | yes | Message type identifier |
| `timestamp` | string | yes | ISO 8601 UTC timestamp of when the message was created on the sender |

---

## Message Types

### handshake

Sent by an Auralens node immediately after establishing a TCP connection. Must be the first message on any connection. AXIOM closes the connection if this is not the first message received.

**Direction:** Auralens → AXIOM

**Fields:**

| Field | Type | Required | Description |
|---|---|---|---|
| `node_id` | string | yes | Unique identifier for this node. Stable across reconnections. |
| `node_type` | string | yes | Type of node. Must be `"auralens"` in Phase 0. |
| `location` | string | no | Human-readable description of where this node is deployed (e.g. `"living room"`) |

**Example:**

```json
{
  "version": "0.0.0",
  "type": "handshake",
  "timestamp": "2024-01-01T12:00:00Z",
  "node_id": "room1",
  "node_type": "auralens",
  "location": "living room"
}
```

---

### event

Sent by an Auralens node when an inference event occurs on its RGBD stream. May be sent at any time after a successful handshake.

**Direction:** Auralens → AXIOM

**Fields:**

| Field | Type | Required | Description |
|---|---|---|---|
| `node_id` | string | yes | ID of the node emitting the event. Must match the handshake `node_id`. |
| `event_type` | string | yes | Category of inference event. See event types below. |
| `confidence` | number | yes | Model confidence score for this inference. Range 0.0 to 1.0. |
| `payload` | object | no | Event-type-specific data. May be empty. |

**Event types (Phase 0):**

| Event type | Description |
|---|---|
| `motion_detected` | Movement detected in the camera frame |
| `person_detected` | A person is present in the frame |
| `person_left` | A previously detected person has left the frame |
| `object_detected` | A specific object has been identified |
| `scene_change` | Significant change in the overall scene |

**Example — person detected:**

```json
{
  "version": "0.0.0",
  "type": "event",
  "timestamp": "2024-01-01T12:00:05Z",
  "node_id": "room1",
  "event_type": "person_detected",
  "confidence": 0.94,
  "payload": {
    "position": "center",
    "depth_meters": 2.3
  }
}
```

**Example — object detected:**

```json
{
  "version": "0.0.0",
  "type": "event",
  "timestamp": "2024-01-01T12:00:10Z",
  "node_id": "room1",
  "event_type": "object_detected",
  "confidence": 0.87,
  "payload": {
    "label": "chair",
    "depth_meters": 1.8
  }
}
```

**Example — motion detected:**

```json
{
  "version": "0.0.0",
  "type": "event",
  "timestamp": "2024-01-01T12:00:02Z",
  "node_id": "hallway1",
  "event_type": "motion_detected",
  "confidence": 0.99,
  "payload": {}
}
```

---

### heartbeat

Sent periodically by an Auralens node to signal that the connection is alive even when no inference events are occurring. AXIOM uses the absence of heartbeats to detect dead connections.

**Direction:** Auralens → AXIOM

**Interval:** Every 30 seconds when no event has been sent

**Fields:** No additional fields beyond the common set.

**Example:**

```json
{
  "version": "0.0.0",
  "type": "heartbeat",
  "timestamp": "2024-01-01T12:00:30Z"
}
```

---

## Validation Rules

AXIOM applies these checks to every received message:

1. Message must be valid JSON
2. `version` must be present and equal to `"0.0.0"`
3. `type` must be present and a known message type
4. `timestamp` must be present and a valid ISO 8601 string
5. For `event` messages: `node_id` must match the `node_id` from the handshake on this connection
6. For `event` messages: `confidence` must be a number in the range 0.0 to 1.0

Any violation closes the connection and logs the reason.

---

## Adding New Event Types

New `event_type` values can be added in a MINOR version increment. AXIOM must handle unknown event types gracefully — log a warning and continue, do not close the connection.
