# m0rtis — Node Heartbeat Mechanism

**Scope:** node-side only (`MortisNode`, `m0rtis_node/`). Hub-side handling
of heartbeats (updating "last seen," the `HeartbeatTimeout` disconnect
reason) is covered by `docs/CONNECTION-STATE.md` and isn't duplicated here.

**Status:** design finalized, implementation pending.

---

## Why I'm writing this down

The first pass at this (`heartbeat_daemon()`, still in the working tree at
time of writing) used `fork()` to background the heartbeat sender, gated by
a `bool sent` flag meant to signal "an inference event just went out, skip
the next heartbeat." That doesn't work: `fork()` duplicates the *entire
address space* of the process. The forked child gets its own private copy
of `sent` — nothing the parent (the main event loop) writes to it after the
fork is ever visible to the child. A liveness signal shared between "the
thing sending events" and "the thing deciding whether to send a heartbeat"
needs actual shared memory, which means a thread, not a separate process.

`m0rtis_proto/include/m0rtis_proto/framing.hpp`'s file header already
anticipated this — it calls out that `emit_event`/`recv_event` aren't
thread-safe on a shared socket and says as much: "Caller's job to serialize
access to a given socket if that's ever a thing (see MortisNode's eventual
heartbeat-thread design)." A thread was the intended shape from early on;
this doc is that design written out properly.

---

## Design

### One connection, one thread, one mutex

The heartbeat sender reuses the **same socket** (`sock`) that
`connect_hub()` opened and the main event loop sends inference events on —
not a second connection. This matches the rest of the protocol: one TCP
connection per node, one handshake per connection
(`docs/CONNECTION-STATE.md`, `docs/MESSAGES.md`). A separate
heartbeat-only connection would mean a second handshake the hub-side state
machine and docs don't model.

Because two threads (the main event loop and the heartbeat thread) now
write to the same socket, every `emit_event(sock, ...)` call — from either
thread — goes under a `std::mutex` (`_sock_mutex`). `framing.hpp` is
explicit that it does no locking of its own; serializing access to a given
fd is the caller's job.

### Reset-on-activity timer, not a periodic check

`docs/MESSAGES.md`'s node-side message flow describes this as:

> 3) After most recent event set timer
>    a) On timer timeout send `heartbeat` to node to check if alive

That's an idle/debounce timer — it restarts on every send, node-wide,
whether the send was an inference event or a previous heartbeat. It is
**not** "wake up every `HEARTBEAT_INTERVAL` seconds and check a flag,"
which would still fire close to a fixed cadence even when the node is
chatty.

The heartbeat thread implements this with a `std::condition_variable`:

- It waits on the condition variable for up to `HEARTBEAT_INTERVAL` seconds
  (currently `15`, `m0rtis_node.hpp`).
- Every time the event loop successfully sends an inference event, it sets
  `sent = true` and calls `notify_one()` on the same condition variable.
- If the heartbeat thread wakes because it was **notified** (an event just
  went out), it clears `sent` and goes straight back to waiting — this
  restarts the full 15-second window from that moment.
- If the heartbeat thread wakes because the wait **timed out** — 15
  consecutive seconds with nothing else sent — it sends a `Heartbeat`
  envelope.

A heartbeat only ever goes out after genuine silence, and every send of
any kind pushes the next possible heartbeat back by a full interval,
rather than just suppressing the next tick of a fixed schedule.

### Why the flag has to be set (and cleared) under the condition variable's mutex

`sent` is a plain signal checked by a predicate passed to
`condition_variable::wait_for`. If the event loop set `sent = true` and
called `notify_one()` without holding `_heartbeat_cv_mutex`, there's a
window where the heartbeat thread could check the predicate (false), and
*then* the event loop's write-and-notify happens before the heartbeat
thread actually starts waiting — a lost wakeup. The heartbeat thread would
then sleep out the full 15 seconds not knowing anything happened. Setting
`sent` while holding the same mutex the condition variable uses closes
that window: the heartbeat thread is either already blocked (notify wakes
it) or hasn't yet checked the predicate (it'll see the update when it
does).

### Thread lifecycle

The heartbeat thread starts at the top of `event_loop()` — which only ever
runs after `connect_hub()` has already completed the handshake (see
`main.cpp`), so this is equivalent to "start right after the handshake is
accepted" without needing to touch `connect_hub()` itself.

`node_shutdown()` — the single place that closes `sock` — is also the
single place that stops and joins the heartbeat thread, and it does so
**before** closing the socket. An `std::atomic<bool> _shutdown_requested`
is set, the condition variable is notified so the thread wakes immediately
instead of waiting out its current interval, and the thread is joined.
Only after that does `node_shutdown()` touch `sock`. Keeping this ordering
inside `node_shutdown()` itself (rather than trusting every caller to stop
the thread first) keeps "nothing else touches `sock` after this returns"
as one invariant owned by one function.

---

## Members added to `MortisNode`

| Member | Type | Purpose |
|---|---|---|
| `sent` | `std::atomic<bool>` | Set by the event loop after a successful inference-event send; read and cleared by the heartbeat thread. Atomic because it's written on one thread and read on another — a plain `bool` here is a data race. |
| `_sock_mutex` | `std::mutex` | Serializes every `emit_event(sock, ...)` call across both threads. |
| `_shutdown_requested` | `std::atomic<bool>` | Heartbeat thread's stop signal. |
| `_heartbeat_cv_mutex` | `std::mutex` | Guards `sent` and `_shutdown_requested` reads/writes that the condition variable's predicate depends on. |
| `_heartbeat_cv` | `std::condition_variable` | Lets the heartbeat thread sleep up to `HEARTBEAT_INTERVAL` seconds but wake early on either an event notification or a shutdown request. |
| `_heartbeat_thread` | `std::thread` | The heartbeat loop's thread handle, started in `event_loop()`, joined in `node_shutdown()`. |

`_heartbeat_sock` (the second, heartbeat-only connection from the earlier
fork-based attempt) is removed — dead once there's only one connection.

`heartbeat_daemon()` is renamed `heartbeat_loop()`. "Daemon" implied the
fork/setsid double-fork pattern being removed; this is a plain
thread-loop function now, named consistently with `event_loop()`.

---

## Sequence

```
event_loop()                          heartbeat_loop() [own thread]
─────────────                         ───────────────────────────────
start heartbeat_loop() thread   ──►    wait up to 15s on _heartbeat_cv
send inference_event                        │
sent = true; notify_one()       ──►    (woken early) sent was true →
send inference_event                    clear it, go back to waiting
sent = true; notify_one()       ──►    (woken early) → clear, re-wait
        ...                                 │
(15s pass, nothing sent)                (wait times out)
                                         send heartbeat envelope
                                         go back to waiting
        ...
loop ends → node_shutdown()
  _shutdown_requested = true
  notify_all()                  ──►    (woken early) shutdown requested
  join thread                          → thread returns
  emit_event(SHUTDOWN)
  close(sock)
```

---

## Known edge case (not addressed by this design)

`next_fixture_inference_event()` can throw on its first call if the
fixture file is missing. If that happens after the heartbeat thread has
already been started, the exception unwinds out of `event_loop()` without
joining the thread, and `std::thread`'s destructor calls `std::terminate()`
on a still-joinable thread when `MortisNode` is destroyed. This is an
already effectively-fatal startup condition (no fixture, no events either
way), so it isn't handled specially — noted here in case clean teardown in
that specific failure path matters later.
