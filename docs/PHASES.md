# M0rtis Phases

This document describes the planned evolution of the M0rtis protocol from its initial unidirectional form through a fully capable, secure, high-performance local sensor network protocol.

---

## Phase 0 — Unidirectional Event Delivery

**Version:** `0.0.0`
**Status:** Current

The foundation. One direction only — Auralens nodes push inference events to the AXIOM hub. AXIOM receives, validates, and processes them. No response, no acknowledgement, no discovery, no security.

**Capabilities:**
- TCP transport
- Length-prefixed JSON framing
- Persistent connections
- Handshake for node identity
- Inference event messages
- Heartbeat for liveness detection
- Protocol versioning

**Constraints:**
- Auralens → AXIOM only
- No acknowledgement
- No authentication
- No encryption
- No automatic node discovery
- Static node configuration

---

## Phase 1 — Bidirectional Communication

**Version:** `1.0.0`
**Status:** Planned

AXIOM gains the ability to send messages back to connected Auralens nodes. This enables remote control of nodes — pausing inference, adjusting sensitivity, requesting snapshots — without physical access to the device.

**New capabilities:**
- AXIOM → Auralens message direction
- Acknowledgement messages — AXIOM confirms receipt of events
- Command messages — AXIOM sends instructions to nodes
- Node status query — AXIOM can request current state from a node

**New message types:**
- `ack` — AXIOM acknowledges a received event
- `command` — AXIOM sends an instruction to a node (pause, resume, configure)
- `status_request` — AXIOM requests a node status report
- `status_response` — node replies with current state

**What changes from Phase 0:**
- Nodes must now read from the connection as well as write
- Message IDs introduced so acks can reference specific events
- Both sides must handle partial reads and writes

---

## Phase 2 — Node Discovery

**Version:** `1.1.0`
**Status:** Planned

Nodes no longer need to be manually configured with AXIOM's address. They announce themselves on the local network and AXIOM discovers them automatically.

**New capabilities:**
- mDNS/Zeroconf-based node announcement
- AXIOM broadcasts its presence on the local network
- Nodes discover AXIOM without static IP configuration
- Dynamic node registration and deregistration

**What changes from Phase 1:**
- Nodes use service discovery before attempting TCP connection
- AXIOM registers a local DNS service record on startup
- Node configuration no longer requires a hardcoded hub address

**Dependencies:**
- mDNS library (e.g. Avahi on Linux)

---

## Phase 3 — Stream Viewer

**Version:** `1.2.0`
**Status:** Planned

A third actor joins the network — a viewer client that connects to AXIOM and receives a live feed of inference events from all connected Auralens nodes. AXIOM acts as a hub, fanning events out to all subscribed viewers.

**New capabilities:**
- Viewer client connection type (distinct from node connections)
- AXIOM fans inference events to all connected viewers
- Viewer can subscribe to events from specific nodes or all nodes
- Live event stream with low latency

**New message types:**
- `viewer_handshake` — viewer identifies itself to AXIOM
- `subscribe` — viewer requests events from specific nodes or all nodes
- `stream_event` — AXIOM forwards an inference event to a viewer

**What changes from Phase 2:**
- AXIOM now manages two connection pools — nodes and viewers
- Incoming events are routed to both the AXIOM pipeline and all subscribed viewers
- Fan-out must not block event processing

---

## Phase 4 — Security

**Version:** `2.0.0`
**Status:** Planned

Nodes and viewers must authenticate before being accepted. All traffic is encrypted. This phase is a MAJOR version increment because it changes the connection lifecycle in a breaking way — unauthenticated Phase 1/2/3 nodes cannot connect to a Phase 4 AXIOM.

**New capabilities:**
- TLS encryption for all connections
- Node authentication via pre-shared keys or certificates
- AXIOM maintains an allowlist of trusted node identities
- Rejected connections are logged with reason

**What changes from Phase 3:**
- TLS wraps the TCP connection before any M0rtis framing occurs
- Handshake extended with authentication credential
- AXIOM validates credential before accepting the connection
- Nodes and viewers must be provisioned with credentials

**Why this is MAJOR:**
- Existing unencrypted nodes cannot connect
- Handshake format changes
- All deployments must be migrated simultaneously

---

## Phase 5 — Binary Framing

**Version:** `3.0.0`
**Status:** Future consideration

At high inference rates (many nodes, many events per second), JSON serialization becomes a measurable overhead. Phase 5 replaces JSON with a compact binary format to reduce payload size and parsing cost.

**New capabilities:**
- Binary message encoding (e.g. MessagePack or a custom binary schema)
- Significantly smaller payloads than JSON equivalents
- Faster serialization and deserialization on constrained edge devices

**What changes from Phase 4:**
- Framing header updated to indicate encoding type
- All message types redefined in binary schema
- JSON encoding retained as a compatibility option behind a negotiation flag

**Why this is MAJOR:**
- Wire format is incompatible with all previous versions
- Both sides must upgrade simultaneously unless encoding negotiation is implemented

---

## Version Summary

| Phase | Version | Key Addition |
|---|---|---|
| 0 | `0.0.0` | Unidirectional event delivery |
| 1 | `1.0.0` | Bidirectional, acknowledgements, commands |
| 2 | `1.1.0` | Automatic node discovery |
| 3 | `1.2.0` | Real-time stream viewer |
| 4 | `2.0.0` | Authentication and encryption |
| 5 | `3.0.0` | Binary framing |
