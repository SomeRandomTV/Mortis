# M0rtis Protocol

M0rtis is the local network communication protocol between Auralens sensor nodes and the AXIOM hub. It is designed for real-time, reliable, chronologically ordered delivery of structured inference events over a local network.

Version: `0.0.0`

---

## Purpose

Auralens nodes run computer vision inference on RGBD camera streams. When an inference event occurs, the node needs to deliver a structured description of that event to the AXIOM hub for reasoning and response. M0rtis defines exactly how that delivery happens — the transport, the framing, the message format, the connection lifecycle, and the versioning contract.

---

## Design Principles

**Reliable and ordered.** Events must arrive at AXIOM in the order they were emitted. A missed or reordered event could produce incorrect reasoning. TCP provides both guarantees at the transport layer.

**Real-time.** Events are delivered as they occur, not batched. Latency between inference and AXIOM processing should be bounded by the local network, not by the protocol.

**Simple first.** Phase 0 is intentionally minimal — unidirectional, JSON framing, no authentication, no discovery. Complexity is added in later phases only when it is needed.

**Version-aware from day one.** Every message carries a protocol version. This makes future evolution possible without breaking existing nodes.

**Local only.** M0rtis is not designed for the public internet. It assumes a trusted local network. Security hardening is a later phase concern.

---

## Topology

```
[Auralens node 1] ──┐
[Auralens node 2] ──┼──→ [AXIOM hub]
[Auralens node N] ──┘
```

One AXIOM hub. One or more Auralens nodes. Each node maintains a persistent TCP connection to the hub. The hub accepts all connections and processes events from all nodes concurrently.

---

## Current Phase

**Phase 0 — Unidirectional event delivery**

- Auralens → AXIOM only
- TCP transport
- Length-prefixed JSON framing
- Protocol version `0.0.0`
- No authentication
- No node discovery
- No acknowledgement

See `PHASES.md` for the full roadmap.

---

## Documents

| File | Contents |
|---|---|
| `SPEC.md` | Full protocol specification — connection lifecycle, versioning, error handling |
| `FRAMING.md` | Wire format — length-prefix layout, byte order, encoding |
| `MESSAGES.md` | Complete message catalog with schemas and examples |
| `CONCEPTS.md` | Networking and protocol concepts relevant to M0rtis |
| `PHASES.md` | Roadmap from Phase 0 through Phase 5 |
