# M0rtis Concepts

This document explains the networking and protocol concepts behind M0rtis. Each entry is tied directly to a decision or component in the protocol.

---

## TCP — Why It Is Used Here

TCP (Transmission Control Protocol) is a connection-oriented transport protocol. It provides three guarantees that M0rtis depends on:

**Reliability.** Every byte sent will be received, or the connection will be reported as broken. TCP handles retransmission automatically. If a packet is lost on the network, TCP resends it without the application knowing.

**Ordering.** Bytes arrive in the same order they were sent. If a node sends event A then event B, AXIOM will always receive A before B.

**Flow control.** If AXIOM is processing events slower than Auralens is sending them, TCP will slow the sender down rather than dropping data.

These three properties make TCP the right choice for M0rtis events, where a missed or reordered inference could produce wrong reasoning in AXIOM.

The tradeoff is latency — TCP adds overhead for acknowledgements and retransmission. For local network communication between two devices a few meters apart, this overhead is negligible.

---

## Sockets

A socket is an endpoint for network communication. It is identified by an IP address and a port number. Two sockets — one on each end of a connection — form a channel through which bytes flow.

In M0rtis:
- AXIOM opens a **server socket**, binds it to a port (default 7373), and listens for incoming connections
- Each Auralens node opens a **client socket** and connects to AXIOM's address and port
- Once connected, both sides have a dedicated socket for that connection

Key operations on a TCP socket:

| Operation | Description |
|---|---|
| `bind` | Attach the socket to a local address and port |
| `listen` | Mark the socket as ready to accept connections |
| `accept` | Block until a client connects, return a new socket for that connection |
| `connect` | (Client side) Establish a connection to a server |
| `send` | Write bytes into the connection |
| `recv` | Read bytes from the connection |
| `close` | Terminate the connection and release resources |

---

## Ports

A port is a number (0–65535) that identifies a specific service on a device. IP addresses identify devices; ports identify which application on that device receives the data.

M0rtis uses port `7373` by default. This port is in the user-registered range (1024–49151) and is not assigned to any well-known service.

AXIOM binds to this port on startup. Auralens nodes must be configured with the AXIOM hub's IP address and this port number.

---

## Persistent Connections

M0rtis uses persistent connections — a node connects once and holds the connection open for the duration of its session. This is different from HTTP's traditional model where a new connection is opened for each request.

Persistent connections are the right model here because:
- Inference events can arrive at high frequency — reconnecting per event would add significant overhead
- AXIOM needs to know which physical node a connection corresponds to — persistent connections make identity trivial
- TCP connection setup (three-way handshake) has latency cost that would accumulate at event rates

---

## Framing

TCP delivers a stream of bytes with no inherent message boundaries. Framing is the mechanism that tells the receiver where one message ends and the next begins.

M0rtis uses **length-prefix framing**: the sender writes a 4-byte integer (the payload size) followed by the payload bytes. The receiver reads the 4-byte prefix first, then reads exactly that many bytes for the payload.

This is the most common framing approach for binary and JSON protocols on TCP. See `FRAMING.md` for the full wire format.

---

## Byte Order (Endianness)

Multi-byte integers can be stored with the most significant byte first (big-endian) or least significant byte first (little-endian). Different CPU architectures use different conventions.

Network protocols use **big-endian** by convention — this is called network byte order. M0rtis follows this convention for the 4-byte length prefix.

In C++, `htonl()` converts a 32-bit integer from host byte order to network byte order before sending. `ntohl()` converts from network byte order back to host byte order after receiving.

```cpp
uint32_t length = payload.size();
uint32_t network_length = htonl(length);  // convert before sending
send(socket_fd, &network_length, 4, 0);
```

---

## Handshake

A handshake is an initial exchange that establishes identity and validates compatibility before real communication begins. M0rtis uses a one-way handshake — the node sends a `handshake` message, AXIOM validates it, and if valid the event stream begins.

The handshake carries:
- Protocol version — confirms both sides speak the same protocol
- Node identity — tells AXIOM which physical device this connection represents

In Phase 0 there is no challenge-response or authentication. The handshake is purely informational. Phase 4 will add authentication to the handshake.

---

## Heartbeat

A heartbeat is a periodic message sent when there is nothing else to say, purely to signal that the connection is alive.

Without heartbeats, AXIOM cannot distinguish between a node that is idle (no events occurring) and a node that has silently disconnected (e.g. network failure without a clean TCP close). Most TCP implementations have a keepalive mechanism, but it operates on a very long timeout by default (hours). Application-level heartbeats give faster detection.

M0rtis nodes send a `heartbeat` message every 30 seconds when no event has been sent. AXIOM can detect a dead connection within roughly 60 seconds.

---

## Reconnection and Backoff

Nodes are responsible for reconnecting after a disconnection. Immediately retrying on failure risks flooding the network with connection attempts if AXIOM is down.

**Exponential backoff** is the standard approach: wait 1 second after the first failure, 2 seconds after the second, 4 after the third, and so on up to a maximum (e.g. 60 seconds). This gives AXIOM time to recover without being overwhelmed.

```
attempt 1: wait 1s
attempt 2: wait 2s
attempt 3: wait 4s
attempt 4: wait 8s
...
attempt N: wait min(2^N, 60)s
```

---

## Concurrency — Handling Multiple Nodes

AXIOM must handle N Auralens nodes simultaneously. This means handling N simultaneous TCP connections, each potentially sending events at the same time.

Two common approaches:

**Thread per connection.** Spawn a thread for each accepted connection. Simple to implement. Each thread blocks on `recv` waiting for data from its node. Works well for small numbers of nodes.

**Multiplexing with select/poll.** A single thread monitors all connections simultaneously using `select` or `poll`. When any connection has data ready, the thread reads it. More complex but scales better.

For Phase 0 with a small number of nodes on a personal local network, the thread-per-connection model is simpler and appropriate.

---

## Protocol Versioning

A version number in every message ensures that as M0rtis evolves, AXIOM can detect when a node is running an incompatible version of the protocol.

M0rtis uses semantic versioning (`MAJOR.MINOR.PATCH`):

- **MAJOR** — breaking change. Old nodes cannot communicate with new AXIOM, or vice versa.
- **MINOR** — additive change. New message types or fields added. Old nodes still work.
- **PATCH** — clarification or non-breaking fix. No behavior change.

In Phase 0, AXIOM rejects any connection where the version does not exactly match `"0.0.0"`. Future phases may introduce negotiation where AXIOM accepts a range of compatible versions.
