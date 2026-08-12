# m0rtis — Post-SCRUM Audit: Small Stuff Not Worth Its Own Ticket

**Scope:** everything below was found doing a full pass over the current working tree (not just the last commit — `git status` currently shows the majority of `m0rtis_hub`, `m0rtis_node`, `m0rtis_proto`, and `tests` as locally modified, uncommitted) plus the 17 real tickets in Jira (`SCRUM-6` through `SCRUM-22`; `SCRUM-4`/`SCRUM-5` are onboarding samples, not tracked here).

This doc is deliberately **not** a duplicate of Jira. Two kinds of content live here instead:

1. Places where a Jira ticket's status no longer matches what the code actually does (§1).
2. Small bugs/logic errors found during this audit that don't have — and probably don't warrant — their own ticket (§2), plus a couple of stale doc lines (§3).

Legend: 🔴 breaks correct behavior today · 🟠 works today but fragile/leftover scaffolding · 🟡 minor / cosmetic / hygiene

---

## 1. Jira tickets whose status has drifted from the code

- **SCRUM-9 (Done) — confirmed accurate, and then some.** The raw `recv`/`buff[bytes_recv] = '\0'` pattern isn't just bounds-fixed, it's gone entirely — `m0rtis_hub.cpp` no longer touches a raw buffer anywhere; everything goes through `recv_event`.

- **SCRUM-16 (To Do in Jira, functionally Done in the working tree).** `tests/` now has a real `CMakeLists.txt`, `enable_testing()` + `add_subdirectory(tests)` are wired into the root `CMakeLists.txt`, and `ctest` runs 8 passing suites (`version`, `envelope`, `message_type`, `event_type`, `framing`, `connection_state`, `fixture`, `fixture_reader`) — this was done this session. Worth moving the ticket, or at least commenting on it, once this work is committed.

- **SCRUM-13 (To Do) — partially landed.** The three typos (`namepsace`, `consteval`, `Transistion`) are already fixed and `vnode_t_table` is already populated with 7 of the transitions from `docs/CONNECTION-STATE.md`. What's genuinely still missing — and self-documented in the header's own comment — is 2 rows (`Disconnected→Connecting`, `Connecting→HandshakeSent`) that can't be added yet because `TRIGGERS` has no enum value for "connect initiated" / "TCP connect succeeds". So the ticket is real, just much smaller in scope than its description implies now.

- **SCRUM-15 (To Do) — has an uncommitted, incomplete start.** `event_type.hpp` already has `BoundingBox` and `InferenceEvent` structs, but no `to_json`/`from_json` for either — so the ticket's own acceptance criteria ("round-trips through to_json/from_json") isn't met yet. Worth knowing before someone starts this from scratch and duplicates the structs.

- **SCRUM-8 (Done) — the fix landed, the ticket's own cleanup note didn't.** `string_to_msg` correctly throws on any unrecognized string now, but the ticket text says "Also fix the typo 'messagee' while touching this line" — `message_type.hpp` still throws `"ERROR: Unknown messagee type " + msg`.

---

## 2. Bugs/logic errors found this audit (no ticket)

1. 🔴 **`recv_event` returning `std::nullopt` is treated as retryable everywhere it's checked, but it never is.** Both `accept_connection`'s handshake-wait loop and `recv_loop`'s dispatch loop do `if (env == std::nullopt) { continue; }`. Once the peer closes the connection, `recv()` on that fd returns `0` forever (EOF is sticky), so `accept_connection`'s unbounded `while (true)` spins as fast as the CPU allows instead of closing the socket and exiting. *(Already discussed in this session — fix proposed, not yet applied.)*

2. 🔴 **`recv_loop`'s `switch` has no `break` after the `InferenceEvent` case**, so it falls through into `default:` — every successfully-received, valid `InferenceEvent` gets logged as `"ERROR: Invalid Envelope"` and has `kill_node_connection` called on it right after being printed.

3. 🔴 **`recv_loop()` is hardcoded to `connected_nodes[1].node_sock`**, not the `vnode_id` that actually just completed the handshake in `accept_connection()`. If a node connects with any `VNODE_ID` other than `1`, `std::map::operator[]` default-constructs a new `ConnectedNode` for key `1` with `node_sock == 0` — and fd `0` is stdin. This is currently masked because `m0rtis_node/src/main.cpp` hardcodes `MortisNode vnode(host, port, 1)`, so the two hardcoded `1`s happen to line up.

4. 🟠 **`recv_loop()`'s outer loop is bounded (`counter++ < 5`)** — reads like leftover test scaffolding rather than the "indefinite dispatch loop" `docs/IMPLEMENTATION.md` §6 describes. Worth swapping for a real loop condition (e.g. driven by connection state) before this is anything more than a manual smoke test.

5. 🟡 **`kill_node_connection()` closes the socket but doesn't clean up the map entry** — `is_connected` stays `true` and `node_state` stays whatever it was (typically `ACTIVE`) after the connection is killed, so `connected_nodes` can report a dead connection as still active.

6. 🟡 **`accept_connection()`: `sockaddr_in hub_addr;` isn't zero-initialized** (`sockaddr_in hub_addr{};` would fix it). This is `docs/BUGS-001.md`'s own still-unchecked item for this exact line — `m0rtis_node.cpp`'s equivalent (`sockaddr_in node_addr{};`) already does this correctly.

7. 🟡 **`accept_connection()`: `inet_pton(AF_INET, _HOST, &hub_addr.sin_addr.s_addr)` passes `&sin_addr.s_addr` (a `uint32_t*`) instead of `&sin_addr` (the `struct in_addr*` the function actually expects).** Works today only because `in_addr` happens to be a single `uint32_t` under the hood — `m0rtis_node.cpp`'s equivalent call (`&node_addr.sin_addr`) does it correctly.

8. 🟡 **`accept_connection()`'s `accept()`-failure branch doesn't `close(sock)`** before returning `-5`, unlike every other error branch in the same function (socket-creation, bind, and listen failures all `close(sock)` first).

9. 🟡 **Handshake timeout is comments only.** `accept_connection()` has `// start timer for handshake` and `// stop timer` bracketing the handshake-wait loop, but no timer actually exists — a slow/silent node can hold the loop open indefinitely (compounds with item 1 above once that's fixed: closing on `nullopt` handles a *closed* connection, not a *silent* one).

---

## 3. Stale doc lines (not code bugs)

- `docs/BUGS-001.md`'s `m0rtis_node.cpp` entry, *"No `bytes_recv == 0` (peer-closed) check in the read loop"*, refers to the old stdin-echo read loop, which doesn't exist in `m0rtis_node.cpp` anymore — the file currently only has `connect_hub()`; the event-emission loop (`event_loop()`) is declared but not yet implemented (part of `SCRUM-10`). Not a live bug, just a line that no longer points at real code.
- `CLAUDE.md`'s "Known issues to not reintroduce" section still calls out `connection-state.hpp`'s `namepsace`/`Transistion` typos — both are already fixed in the current file (see §1's `SCRUM-13` note above). That section of `CLAUDE.md` is stale.
