# m0rtis — Connection State Machine

This document defines the connection lifecycle for both sides of a m0rtis
connection: the **Hub** (server, accepts many nodes) and the **Node**
(client, connects to one hub). It is the source of truth for what message
sequences are legal, and is meant to be implementable as pure state +
transition logic with **no I/O** — sockets call into this, this doesn't
call into sockets.

Intended home: `m0rtis_proto/connection_state.hpp`

---

## Design principles

- **States are few and coarse.** Failure *reasons* are data attached to the
  terminal state, not separate states. This keeps the transition table small
  and keeps "close the connection and clean up" as a single code path
  regardless of *why* the connection is ending.
- **No I/O in the state machine itself.** It answers "given this state and
  this trigger, what's the next state (and is this trigger even legal)?" —
  nothing more. This makes it unit-testable with zero sockets involved.
- **Hub and Node are separate machines.** Their roles aren't symmetric: the
  hub defends against malformed/out-of-order input from potentially many
  untrusted nodes; the node mostly just tracks its own progress through one
  conversation.

---

## Hub-side: per-connection state

One instance of this state lives per accepted connection (i.e., per thread,
once the hub is multi-threaded — see the earlier wiring discussion).

### States

| State | Meaning |
|---|---|
| `AwaitingHandshake` | Connection accepted at the TCP level; no valid `Envelope` exchanged yet |
| `Active` | Handshake validated; node is a trusted, ongoing participant |
| `Disconnected` | Terminal. Connection is closed, thread is cleaning up |

### Disconnect reasons (attached data, not states)

| Reason | Meaning |
|---|---|
| `ClientClosed` | Peer closed the connection / EOF |
| `IOError` | `send`/`recv` failed at the transport level |
| `VersionMismatch` | Handshake's protocol version or node id failed validation |
| `ProtocolViolation` | A message arrived that isn't legal in the current state |
| `HeartbeatTimeout` | No message received within the timeout window |

### Transition table

| Current State | Trigger | Valid? | Next State | Notes |
|---|---|---|---|---|
| `AwaitingHandshake` | receive `Handshake`, version/node_id OK | ✅ | `Active` | register node; start heartbeat-timeout timer |
| `AwaitingHandshake` | receive `Handshake`, bad version/id | ❌ | `Disconnected` | reason: `VersionMismatch` |
| `AwaitingHandshake` | receive `InferenceEvent` or `Heartbeat` | ❌ | `Disconnected` | reason: `ProtocolViolation` — nothing but a handshake is legal here |
| `AwaitingHandshake` | no handshake received within N seconds | ❌ | `Disconnected` | reason: `HeartbeatTimeout` (or a dedicated `HandshakeTimeout` reason — open question) |
| `AwaitingHandshake` | `recv_event` fails / EOF | — | `Disconnected` | reason: `ClientClosed` or `IOError` depending on cause |
| `Active` | receive `InferenceEvent` | ✅ | `Active` (self-loop) | process event; reset heartbeat timer |
| `Active` | receive `Heartbeat` | ✅ | `Active` (self-loop) | update last-seen; reset heartbeat timer |
| `Active` | receive `Handshake` again | ❓ | *open question* | see Open Questions #2 |
| `Active` | heartbeat timer expires, nothing received | — | `Disconnected` | reason: `HeartbeatTimeout` — timer-driven, not message-driven |
| `Active` | `recv_event` fails / EOF | — | `Disconnected` | reason: `ClientClosed` or `IOError` depending on cause |
| `Disconnected` | (any) | — | — | terminal; thread joins/exits, no further transitions |

### Hub lifecycle (narrative)

```
 accept()
    │
    ▼
AwaitingHandshake ──(bad handshake / wrong msg type / timeout / IO error)──► Disconnected
    │
    │ (valid handshake)
    ▼
  Active ──(InferenceEvent / Heartbeat)──► Active   [self-loop]
    │
    └──(heartbeat timeout / IO error / client closed)──► Disconnected
```

---

## Node-side: connection lifecycle

### States

| State | Meaning |
|---|---|
| `Disconnected` | No socket; not connected |
| `Connecting` | TCP `connect()` in flight |
| `HandshakeSent` | TCP connected; `Handshake` envelope sent; not yet confirmed accepted |
| `Active` | Steady state — free to emit `InferenceEvent` / `Heartbeat` |

### Transition table

| Current State | Trigger | Next State | Notes |
|---|---|---|---|
| `Disconnected` | connect initiated | `Connecting` | |
| `Connecting` | TCP connect succeeds | `HandshakeSent` | build + send `Handshake` envelope |
| `Connecting` | TCP connect fails | `Disconnected` | report error; retry policy is a separate concern |
| `HandshakeSent` | socket stays open after send | `Active` | **implicit accept** — see Open Questions #1 |
| `HandshakeSent` | socket closes immediately after send | `Disconnected` | presumed hub rejection; indistinguishable from a network blip today |
| `Active` | emits `InferenceEvent` / `Heartbeat` | `Active` (self-loop) | |
| `Active` | send/recv failure | `Disconnected` | |
| `Active` | user/app requests quit | `Disconnected` | clean shutdown |

### Node lifecycle (narrative)

```
Disconnected ──(connect initiated)──► Connecting
                                          │
                        ┌─────────────(fail)
                        │                 │
                        ▼                 ▼
                  HandshakeSent      Disconnected
                        │
        ┌───(socket closes)          (socket stays open)
        │                                 │
        ▼                                 ▼
  Disconnected                          Active ──(emit events)──► Active [self-loop]
                                          │
                        (send/recv failure / user quit)
                                          │
                                          ▼
                                    Disconnected
```

---

## Open questions (deliberately unresolved for now)

1. **No handshake-ack message type exists.** `HandshakeSent → Active` is
   currently inferred purely from "the socket didn't close." This can't
   distinguish hub acceptance from hub slowness from network lag. Options:
   - Add a 4th `MessageType` (e.g. `HandshakeAck`) — conflicts with the
     "only 3 message types for Phase 0" comment in `message_type.hpp`.
   - Keep 3 types, but give the node an explicit handshake-confirmation
     timeout instead of relying on silence-as-acceptance.

2. **Duplicate `Handshake` while `Active`** — is this a `ProtocolViolation`
   (simplest; handshake happens exactly once), or a legitimate re-announce
   (useful if a node's identity/config can change mid-session)? No default
   has been chosen yet.

3. **Heartbeat timeout value** is unspecified. It needs to be a concrete,
   probably-configurable number, and must be meaningfully larger than the
   node's heartbeat-send interval to avoid false-positive disconnects under
   normal jitter.

4. **Does the node auto-reconnect** after landing in `Disconnected`, or is
   `Disconnected → Connecting` always externally triggered (human/process
   restart)? Currently the state machine treats it as available but not
   automatic.

---

## Testability note

Because this machine has no I/O, both tables above should be directly
expressible as pure data (state → trigger → next state) and unit-tested
without opening a single socket — e.g. feeding a sequence of `MessageType`
values into the hub machine and asserting the resulting state/reason,
independent of `framing.hpp` or any actual network connection. This is the
natural first layer of the test suite.
