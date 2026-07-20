# M0rtis Message Envelope & Schema Design

**Protocol version:** `0.0.0`
**Phase:** 0 — Unidirectional Event Delivery
**Status:** Design finalized, implementation pending

## Why I'm writing this down

Before I touch `m0rtis_hub.cpp` or `m0rtis_node.cpp` again, I want the message shape nailed down on paper. Right now both sides just echo raw bytes over a socket — there's no framing, no structure, and no way to tell one message from the next. Everything else in Phase 0 (handshake, inference events, heartbeats) depends on getting this envelope right first, because I don't want to redesign it once nodes are already speaking it in the field.

I'm using [nlohmann/json](https://github.com/nlohmann/json) for serialization. It's header-only, so it drops into the build without adding a real dependency, and it has built-in support for mapping C++ enums to string literals, which I'm relying on below.

## The envelope

Every message I send over the wire — regardless of direction or type — is a single JSON object with this shape:

```json
{
  "version": "0.0.0",
  "type": "inference_event",
  "id": 42,
  "timestamp": 1752739200123,
  "payload": { ... }
}
```

| Field | Type | Purpose |
|---|---|---|
| `version` | `string` | Protocol version this message was built against. I'm sending it on every message, not just once at handshake, per the "version-aware from day one" principle — I want mixed-version fleets to be diagnosable from a packet capture alone. |
| `type` | `string` (enum) | What kind of message this is. Phase 0 only defines three values: `handshake`, `inference_event`, `heartbeat`. |
| `id` | `uint64` | A monotonically increasing counter, scoped to the connection. TCP already guarantees ordering, so this isn't for sequencing — I'm adding it now because Phase 1 introduces acknowledgements that need to reference a specific event, and I'd rather not retrofit an ID field onto every message type later. |
| `timestamp` | `int64` | Unix epoch, **milliseconds**. I chose epoch-ms over an ISO 8601 string because it's smaller on the wire, trivially sortable, and has no timezone ambiguity to parse around. I'll format it for display only at the point where it's actually shown to a human. |
| `payload` | `object` | Type-specific body. Its shape depends entirely on `type`. |

At the transport layer, each envelope is preceded by a 4-byte big-endian length prefix so the receiver knows exactly how many bytes to read before attempting to parse JSON. That's a separate concern from the schema itself, but it's what makes the boundary between one envelope and the next unambiguous.

## `type`: message type enum

```cpp
enum class MessageType {
    Handshake,
    InferenceEvent,
    Heartbeat
};
```

Serialized as `"handshake"`, `"inference_event"`, `"heartbeat"` on the wire — snake_case in JSON, PascalCase in C++.

**Failure mode:** if I receive a `type` string I don't recognize, I'm treating that as a protocol-level failure, not a data-level one — it likely means two sides are running incompatible versions and talking past each other. I'd rather reject the message loudly here than silently swallow something I can't interpret. This is different from how I'm handling unrecognized values inside `event_type` below.

## `handshake` payload

Sent as the first message from a node immediately after the TCP connection is established.

```json
{
  "node_id": "auralens-07",
  "protocol_version": "0.0.0"
}
```

`protocol_version` here duplicates the envelope's top-level `version`, but I'm keeping it explicit in the handshake specifically — this is the one message where I want zero ambiguity about what the node speaks, even if the envelope field were ever omitted or malformed.

## `inference_event` payload

The actual reason this protocol exists — a structured description of a CV inference result.

```json
{
  "event_type": "person_detected",
  "confidence": 0.94,
  "bounding_box": { "x": 120, "y": 80, "w": 60, "h": 140 },
  "metadata": {}
}
```

`metadata` is deliberately open — an unconstrained object — so I can attach event-specific data as the inference pipeline grows without needing a schema change for every new field.

### `event_type` enum

```cpp
enum class EventType {
    Unknown,
    PersonDetected,
    MotionDetected,
    ObjectDetected,
    ZoneEntered,
    ZoneExited
};
```

I'm starting with a small set and expanding it as the CV pipeline needs new categories, rather than trying to enumerate everything up front.

**Failure mode:** unlike the envelope's `type`, I'm deserializing any `event_type` string I don't recognize into `EventType::Unknown` rather than rejecting the message. A node running newer firmware may emit an event type this build of AXIOM predates — I'd rather log it and keep processing than have one unfamiliar event type stall the pipeline for every other node on the network. This is the practical expression of the "version-aware from day one" principle: I'm explicitly planning for a fleet where node firmware and hub software drift out of lockstep, and I don't want that drift to be catastrophic.

## `heartbeat` payload

```json
{}
```

Empty by design. The envelope's `id` and `timestamp` already tell AXIOM the node is alive; I don't need anything in the body for Phase 0. If I later want to carry uptime or queue depth in a heartbeat, that's an additive change to an already-open `payload` object — not a breaking one.


## Flow of Messages 

### Node Side (emits)

1) Node connects to hub 
    a) send handshake message 
2) In a loop - emit whatever Event Auralens captures 
3) After most recent event set timer 
    a) On timer timeout send `heartbeat` to node to check if alive 

### Hub side (parses and dispatches)

1) On accept, if message is not `handshake` reject 
2) In a loop: receive a message, parse the envelope, and switch on type:
    a) inference_event → hand off to whatever your AXIOM processing pipeline is
    b) heartbeat → just update "last seen" timestamp for that node, no further action
    c) handshake (after the first one) → protocol violation, probably close the connection
    d) unrecognized type → per what we discussed earlier, this is a strict failure (not the lenient Unknown fallback we're using for event_type)


## Summary of decisions

- **Envelope carries `version` on every message**, not just at handshake — supports diagnosing version drift from raw traffic.
- **`id` is a per-connection monotonic counter**, not a UUID — I don't need global uniqueness, just a stable reference for Phase 1 acks.
- **Timestamps are epoch-ms integers**, not ISO 8601 strings — smaller, sortable, unambiguous.
- **Envelope `type` is strict**: unknown values are rejected as a protocol-level mismatch.
- **`event_type` is lenient**: unknown values fall back to `Unknown` and are still delivered, treated as a data-level forward-compatibility case rather than a protocol error.
- **`payload` is open-ended per type** — new fields can be added without breaking older parsers, as long as I don't remove or repurpose existing ones.

## Next steps

1. Implement the length-prefixed framing layer (4-byte big-endian length header + JSON body) on both `m0rtis_hub.cpp` and `m0rtis_node.cpp`.
2. Write the `Envelope`, `MessageType`, and `EventType` C++ types with `to_json`/`from_json` via nlohmann/json.
3. Replace the current echo loop with real handshake handling on connect.
4. Add heartbeat emission on a timer on the node side, and liveness tracking on the hub side.
5. Fix the buffer overflow in the current `recv` calls (`buff[bytes_recv] = '\0'` writes out of bounds when `bytes_recv == sizeof(buff)`) before building framing on top of it.
