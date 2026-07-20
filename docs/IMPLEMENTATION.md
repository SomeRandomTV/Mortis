# M0rtis Phase 0 — Implementation Strategy

**Version:** `0.0.0`
**Status:** In progress — protocol layer designed, framing implemented, hub/node integration pending

This document captures the implementation plan for M0rtis Phase 0, the decisions
made along the way and why, and the concrete next steps. It's meant to be a
working reference — update it as decisions change.

---

## 1. Where We Started

The initial `m0rtis_hub` / `m0rtis_node` code proved the socket plumbing
(connect/bind/listen/accept, send/recv) but implemented none of the actual
protocol — it was a raw TCP echo test. Gap against the Phase 0 checklist:

| Phase 0 requirement | Status at start |
|---|---|
| TCP transport | Done |
| Persistent connections | Partial — hub was single-connection, single-threaded |
| Length-prefixed JSON framing | Missing entirely |
| Handshake for node identity | Missing |
| Inference event messages | Missing |
| Heartbeat for liveness | Missing |
| Protocol versioning | `VERSION` constant existed but wasn't on the wire |

A buffer-overflow bug was also identified: `buff[bytes_recv] = '\0'` writes
one byte past a 1024-byte buffer when `recv()` fills it exactly. Not yet hit
because test messages were short, but it will bite once framing enables
larger reads. **Fix this when touching the recv loop.**

---

## 2. Directory Structure

The protocol definition (message schema, framing) is **shared** between hub
and node — it must not be duplicated in both, or the two copies will drift.
It lives in its own module, separate from either binary:

```
./
├── CMakeLists.txt
├── docs/
│   └── MESSAGES.md              # schema documentation
├── m0rtis_proto/                 # shared protocol definition (header-only)
│   ├── include/
│   │   └── m0rtis_proto/
│   │       ├── message_type.hpp  # envelope "type" enum — strict
│   │       ├── event_type.hpp    # inference event "event_type" enum — lenient
│   │       ├── envelope.hpp      # the Envelope struct + JSON (de)serialization
│   │       └── framing.hpp       # length-prefixed send_message / recv_message
│   └── third_party/
│       └── nlohmann/
│           └── json.hpp          # vendored, header-only (v3.11.3)
├── m0rtis_hub/
│   ├── CMakeLists.txt            # links m0rtis_proto
│   ├── include/m0rtis_hub.hpp
│   └── src/{m0rtis_hub.cpp, main.cpp}
├── m0rtis_node/
│   ├── CMakeLists.txt            # links m0rtis_proto
│   ├── include/m0rtis_node.hpp
│   └── src/{m0rtis_node.cpp, main.cpp}
├── scripts/
└── tests/
    └── test_main.cpp
```

**Why `include/m0rtis_proto/` is nested one level deeper than just
`include/`:** CMake adds `m0rtis_proto/include/` as an include directory,
and headers are reached as `#include <m0rtis_proto/envelope.hpp>`. This
namespaces the include path so a generic filename like `envelope.hpp`
can't silently collide with a same-named header in another module. Same
reasoning applies to `third_party/nlohmann/json.hpp` — `#include
<nlohmann/json.hpp>` is namespaced by directory, not by a bare filename.

`third_party/` is a sibling of `include/`, not nested inside it — vendored
code you didn't write stays clearly separated from the protocol headers
you did.

CMake wiring (`m0rtis_proto` as a header-only `INTERFACE` library):

```cmake
add_library(m0rtis_proto INTERFACE)
target_include_directories(m0rtis_proto INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
```

Then in `m0rtis_hub/CMakeLists.txt` and `m0rtis_node/CMakeLists.txt`:

```cmake
target_link_libraries(m0rtis_hub PRIVATE m0rtis_proto)
```

---

## 3. Message Envelope

Every message on the wire is one JSON object. The shape is fixed regardless
of `type`; only `payload` varies:

```json
{
  "version": "0.0.0",
  "type": "inference_event",
  "id": 42,
  "timestamp": 1752739200123,
  "payload": { }
}
```

| Field | Type | Notes |
|---|---|---|
| `version` | string | Matches `PROTOCOL_VERSION`. Carried on **every** message, not negotiated once — supports mixed-version fleets later. |
| `type` | string enum | `"handshake"` \| `"inference_event"` \| `"heartbeat"` in Phase 0. |
| `id` | uint64 | Monotonic per-connection counter. Not a UUID — doesn't need global uniqueness, just needs to let Phase 1 acks reference "message #42". |
| `timestamp` | int64 | Unix epoch, **milliseconds**. Sortable, small, no timezone parsing. |
| `payload` | object | Shape depends on `type`. |

### Payload shapes

**`handshake`** (first message a node sends after connecting):
```json
{ "node_id": "auralens-07", "protocol_version": "0.0.0" }
```

**`inference_event`**:
```json
{
  "event_type": "person_detected",
  "confidence": 0.94,
  "bounding_box": { "x": 120, "y": 80, "w": 60, "h": 140 },
  "metadata": {}
}
```

**`heartbeat`**: `{}` — the envelope's own `id`/`timestamp` are enough;
richer payload (uptime, queue depth) is a non-breaking future addition.

### Strict vs. lenient enum handling — a deliberate split

Two enums, two different failure behaviors, on purpose:

- **`MessageType` (envelope's `type`) — strict.** An unrecognized
  top-level type means the peer is speaking a version of the protocol
  we don't understand at all. Hand-written `to_json`/`from_json` that
  **throws** on an unknown string.

  ```cpp
  inline MessageType message_type_from_string(const std::string &s) {
      if (s == "handshake")       return MessageType::Handshake;
      if (s == "inference_event") return MessageType::InferenceEvent;
      if (s == "heartbeat")       return MessageType::Heartbeat;
      throw std::runtime_error("m0rtis: unrecognized message type '" + s + "'");
  }
  ```

- **`EventType` (inference_event's `event_type`) — lenient.** An
  unrecognized event type is just data from a node running newer
  firmware than this build of AXIOM knows about. Falls back to
  `EventType::Unknown` rather than throwing, so one new event type
  doesn't break processing for the whole fleet. Uses nlohmann's
  `NLOHMANN_JSON_SERIALIZE_ENUM` macro, which falls back to the
  **first** listed pair on an unmatched string — hence `Unknown` must
  stay first in the list:

  ```cpp
  enum class EventType {
      Unknown,  // must stay first — see note above
      PersonDetected, MotionDetected, ObjectDetected,
      ZoneEntered, ZoneExited
  };

  NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {
      {EventType::Unknown,        "unknown"},
      {EventType::PersonDetected, "person_detected"},
      // ...
  })
  ```

**Hint:** if you add a third enum later, ask which failure behavior it
needs *before* picking an implementation approach — it determines whether
the macro shortcut is usable or you need the hand-written version.

---

## 4. Framing

Raw TCP is a byte stream, not a message stream — `recv()` doesn't respect
JSON object boundaries. Two messages sent close together can arrive in one
`recv()` call, or one message can split across two calls. **Length-prefixed
framing** fixes this: send a 4-byte length header (network byte order)
before the JSON body; the receiver always reads exactly 4 bytes, then
exactly that many more.

Key implementation details, all deliberate:

- **`send`/`recv` loops, not single calls.** Neither is guaranteed to
  move all requested bytes in one call. `send_all`/`recv_all` helpers
  loop until done, an error occurs, or the peer closes mid-message.
- **`htonl`/`ntohl` on the length header.** Converts to/from network byte
  order — cheap, and removes a class of bug if hub and node ever run on
  different architectures.
- **Combined send/recv of `Envelope` directly** (`send_message(sock,
  const Envelope&)` / `recv_message(sock) -> std::optional<Envelope>`)
  rather than separate framing and serialization layers — fewer call
  sites, less to get wrong, for the complexity Phase 0 actually needs.
- **`std::optional<Envelope>` return, not exceptions, from `recv_message`.**
  A malformed/partial message is an expected, recoverable condition on a
  network boundary, not a program-logic error — callers check for
  `nullopt` rather than wrapping every receive in `try/catch`.
- **Every failure path logs to `stderr` before returning.** Callers don't
  have to remember to log each failure site individually.
- **10MB max message size**, checked *before* attempting to read the
  declared length. Guards against a corrupted or malformed length header
  causing a huge allocation. Real Auralens payloads should be well under
  a few KB — 10MB is headroom, not a target. (Phase 0 has no
  authentication, so this guard matters even without a malicious actor
  in mind — corruption alone is enough reason.)

```cpp
inline bool send_message(int sock, const Envelope &env) {
    const std::string body = nlohmann::json(env).dump();
    if (body.size() > MAX_MESSAGE_SIZE) { /* log, return false */ }
    const uint32_t len_net = htonl(static_cast<uint32_t>(body.size()));
    if (!detail::send_all(sock, reinterpret_cast<const char *>(&len_net), sizeof(len_net)))
        return false;
    return detail::send_all(sock, body.data(), body.size());
}

inline std::optional<Envelope> recv_message(int sock) {
    uint32_t len_net = 0;
    if (!detail::recv_all(sock, reinterpret_cast<char *>(&len_net), sizeof(len_net)))
        return std::nullopt;
    const uint32_t len = ntohl(len_net);
    if (len > MAX_MESSAGE_SIZE) { /* log, return nullopt */ }
    std::vector<char> body(len);
    if (len > 0 && !detail::recv_all(sock, body.data(), len))
        return std::nullopt;
    try {
        return nlohmann::json::parse(body.begin(), body.end()).get<Envelope>();
    } catch (const std::exception &e) { /* log, return nullopt */ }
}
```

Full implementation: `m0rtis_proto/include/m0rtis_proto/framing.hpp`.

**Hint — test this in isolation before wiring it into hub/node.**
`socketpair()` gives you a connected pair of local sockets without any
real networking, perfect for a quick round-trip test:
`send_message` on one end, `recv_message` on the other, assert the
`Envelope` comes back equal. Also worth testing deliberately: sending a
message just over `MAX_MESSAGE_SIZE`, and closing the connection mid-read
to confirm `recv_message` returns `nullopt` instead of hanging or crashing.

---

## 5. Node Implementation Plan

Replace the current stdin-echo loop entirely. Target shape:

1. **Connect** (existing code, keep as-is).
2. **Handshake** — immediately after connect, build and send exactly one
   `Envelope` with `type = Handshake`, `payload = {"node_id": ..., "protocol_version": PROTOCOL_VERSION}`.
3. **Event loop** — as the CV pipeline produces inference events, build an
   `Envelope` with `type = InferenceEvent` and the appropriate payload,
   assign the next `id` from a monotonically incrementing counter, stamp
   `timestamp_ms` with current epoch time, and `send_message`.
4. **Heartbeat timer** — on a fixed interval (pick something like every
   5–10s) with no event traffic, send a `Heartbeat` envelope so AXIOM can
   distinguish "alive but quiet" from "dead." This likely means moving off
   a purely blocking send-then-wait-for-input model toward either a
   background thread or a timed/non-blocking loop — worth deciding
   explicitly rather than backing into it.

**Hint:** keep the monotonic `id` counter as node-local state (e.g. a
member of `MortisNode`, starting at 0 or 1), incremented once per
`send_message` call regardless of message type — handshake, events, and
heartbeats all consume from the same counter, since `id` uniquely
identifies *a message*, not just events.

---

## 6. Hub Implementation Plan

Replace the current echo loop. Target shape:

1. **Accept** (existing code, keep as-is).
2. **Expect handshake first** — the very first `recv_message` on a new
   connection must be `type == Handshake`. Anything else: log and close
   the connection immediately. Store the `node_id` from the payload
   against this connection for logging/identification going forward.
3. **Dispatch loop** — for every subsequent `recv_message`, switch on
   `env->type`:
   - `InferenceEvent` → hand off to the AXIOM processing pipeline
     (whatever that hook looks like — not yet designed).
   - `Heartbeat` → update a "last seen" timestamp for this node; no
     further action needed in Phase 0.
   - `Handshake` (again, after the first) → protocol violation, close
     connection.
   - `recv_message` returns `nullopt` → connection is dead or sent
     garbage; log and close.
4. **Multi-node support** — current hub code accepts one connection,
   fully services it in a blocking loop, and only returns to `accept()`
   once that connection ends. This needs to become one thread (or
   `select`/`epoll`-based event loop) per connection so multiple Auralens
   nodes can be connected to AXIOM simultaneously, which is an explicit
   Phase 0 goal ("nodes," plural).

**Hint:** the strict-vs-lenient enum split from Section 3 pays off exactly
here — an unrecognized `event_type` inside a well-formed `InferenceEvent`
envelope should NOT close the connection (falls back to `Unknown`,
gets logged/handled gracefully), but an unrecognized top-level `type`
SHOULD close it (throws during `recv_message`'s parse, comes back as
`nullopt`, hub treats it as a dead/bad connection). This is already how
the code behaves as designed — worth confirming with a deliberate test
once hub dispatch is wired up.

---

## 7. Known Bugs to Fix Along the Way

- **Buffer overflow**: `buff[bytes_recv] = '\0'` in both current
  `m0rtis_hub.cpp` and `m0rtis_node.cpp` writes one byte past a
  1024-byte stack buffer when `recv()` fills it exactly. Framing
  replaces this recv pattern entirely, so this should disappear
  naturally as part of the rewrite — just don't reintroduce it.
- Current hub is single-threaded/single-connection — see Section 6.4.

---

## 8. Suggested Build/Test Order

1. ~~Envelope schema + serialization (`envelope.hpp`, `message_type.hpp`,
   `event_type.hpp`)~~ — done, sanity-compiled and round-trip tested.
2. ~~Framing (`framing.hpp`)~~ — done, needs a `socketpair()`-based
   round-trip test before integration (see Section 4 hint).
3. Node: handshake send + monotonic id counter.
4. Hub: expect-handshake-first logic, close on violation.
5. Node: inference event emission (can stub with fake/manual events
   before real CV pipeline integration).
6. Hub: dispatch loop for `InferenceEvent`/`Heartbeat`.
7. Node: heartbeat timer (background thread or timed loop).
8. Hub: multi-node threading — one thread per accepted connection.
9. Fix the buffer overflow bug as part of the recv-loop rewrite (should
   be automatic once framing replaces raw recv/send).
10. End-to-end test: real node process talking to real hub process over
    a real TCP connection (not just `socketpair()`), confirm handshake,
    a handful of events, and heartbeats all round-trip correctly, and
    that the hub can hold multiple concurrent node connections.

---

## 9. Open Questions for Later Phases (Not Phase 0 — Don't Build Yet)

- Phase 1 (`1.0.0`): bidirectional comms, `ack`/`command`/`status_request`/
  `status_response` message types — the `id` field designed in now is
  what acks will reference.
- Phase 2 (`1.1.0`): mDNS discovery — no wire-format impact, just how
  the initial connection address is obtained.
- Phase 3 (`1.2.0`): viewer connections and fan-out — hub will need a
  second connection pool distinct from node connections.
- Phase 4 (`2.0.0`): TLS + auth — breaking change to handshake; current
  handshake payload shape should be extensible enough to add a
  credential field without redesigning the envelope itself.
- Phase 5 (`3.0.0`): binary framing — `framing.hpp`'s length-prefix
  approach survives this change conceptually (still length-prefixed),
  but the body encoding changes from JSON to binary behind a
  negotiation flag.
