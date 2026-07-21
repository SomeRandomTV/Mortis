# m0rtis — Bug Todo List

Legend: 🔴 blocks compilation · 🟠 compiles but breaks protocol/logic · 🟡 minor / style

---

## envelope.hpp

- [X]
- [X] 🔴 Typo: `nlohman::json payload` → should be `nlohmann::json payload`
- [X] 🟠 `to_json` references `env.id`, but the struct member is `node_id`
- [X] 🟠 `to_json` references `env.timestamp_ms`, but the struct member is `timestamp`
- [X] 🟠 `from_json` has the same `id` / `timestamp_ms` mismatches as `to_json`
- [X] 🟡 Consider `{}` member-initializer syntax instead of `()` (avoids most-vexing-parse pitfalls): `std::string version{PROTOCOL_VERSION};`
- [X] 🟡 Decide on wire key names (`"node_id"` vs `"id"`) and make sure hub/node agree once they're wired up

## message_type.hpp

- [X] 🔴 `from_json(nlohmann::json &j, const MessageType &t)` — parameters are backwards. Should be `from_json(const nlohmann::json &j, MessageType &t)`, and the body should write `t = string_to_msg(...)`, not `j = ...`
- [X] 🔴 `string_to_msg(std::string &msg)` takes a non-const reference, so it can't bind to temporaries/string literals (e.g. `string_to_msg("handshake")` won't compile). Change to `const std::string &msg`
- [X] 🟡 `msg_to_string`'s switch doesn't have an explicit `case MessageType::UNKNOWN:` — works today (falls through to `throw`) but will trigger a `-Wswitch` warning; add an explicit case

## event_type.hpp

- [X] 🔴 Missing `;` after `enum class EventType { ... }`
- [X] 🔴 `NLOHMANN_JSON_SERIALIZE_ENUM` references `EventType::Unknown` — actual enumerator is `EventType::UNKNOWN` (case mismatch)
- [X] 🟠 `EventType::HazardDetected` exists in the enum but has no entry in the serialization map — will silently misserialize
- [X] 🟠 `ObjectDetected` mapping present but double check all values are covered — recount after fixing `HazardDetected`
- [ ] 🟡 File isn't included anywhere (`envelope.hpp`, `framing.hpp`) — decide how `EventType` is meant to connect to `Envelope::payload` and wire it in

## framing.hpp

- [X] 🔴 `recv_all`'s buffer param is `const char *data` — must be non-const `char *data` since `recv()` writes into it
- [X] 🔴 `recv(sock, void data + nbytes_sent, ...)` — invalid syntax (stray `void`) and wrong counter variable (should be `nbytes_recv`, not `nbytes_sent`, which doesn't exist in this function)
- [X] 🔴 `emit_event` references `body.size()` — local variable is actually named `msg_body`
- [X] 🔴 `emit_event`: `send_all(sock, msg_body.data(), sizeof(msg_body.size()))` — sends `sizeof(size_t)` (always 8) bytes instead of `msg_body.size()` bytes; truncates every payload to 8 bytes
- [X] 🔴 `recv_event` references `body.begin()`/`body.end()` — local buffer is actually named `msg`
- [X] 🔴 `recv_event`: `catch` block doesn't `return std::nullopt;`, and there's no `return` after the `try/catch` at all — falls off the end of a non-`void` function (UB)
- [X] 🟠 `send_all`/`recv_all`: `if (b == 0) { ...; break; }` treats `0` as "transfer complete." For `recv()`, `0` means the peer closed the connection (EOF), not success — this can return "success" on a truncated read. Treat `b <= 0` as failure/incomplete, same as `b < 0`
- [X] 🟠 `MAX_MSG_SIZE` is redefined here as `constexpr const int` inside `namespace m0rtis`. If the earlier `#define MAX_MSG_SIZE 1024` macro is still defined anywhere upstream in the same translation unit, this will be silently macro-substituted into `constexpr const int 1024 = 1024;` — search the codebase and remove the macro version if it still exists

## m0rtis_hub.cpp

- [ ] 🔴 Buffer overflow: `recv(_connection, buff, sizeof(buff), 0);` uses the full 1024-byte buffer, then `buff[bytes_recv] = '\0';` writes one past the end when `bytes_recv == 1024`. Should be `sizeof(buff) - 1` (matches what `m0rtis_node.cpp` already does correctly)
- [ ] 🟡 `sockaddr_in hub_addr;` isn't zero-initialized before setting fields — use `sockaddr_in hub_addr{};`
- [ ] 🟡 Single-threaded: only one node is served at a time despite `<thread>`/`<atomic>` includes — confirm whether concurrent nodes are in scope for this phase

## m0rtis_node.cpp

- [ ] 🔴 `inet_pton` error check only handles `_c < 0` (system error). `inet_pton` returns `0` for an invalid address string, which isn't caught — proceeds to `connect()` with an uninitialized address. Should be `if (_c < 1)` (matches what `m0rtis_hub.cpp` already does correctly)
- [ ] 🟠 No `bytes_recv == 0` (peer-closed) check in the read loop — only `bytes_sent == 0` is handled. If the hub disconnects, the node prints an empty echo and keeps looping instead of exiting
- [ ] 🟡 `sockaddr_in node_addr;` isn't zero-initialized before setting fields — use `sockaddr_in node_addr{};`

---

## Cross-cutting / design follow-ups (not bugs, but worth deciding before tests)

- [ ] Wire `emit_event` / `recv_event` (framing.hpp) into `m0rtis_hub.cpp` / `m0rtis_node.cpp` — they're currently disconnected from the raw echo client/server
- [ ] Standardize on one error-signaling convention — `framing.hpp` uses `int` return codes (0/-1/-2/-3), while the earlier draft used `bool`/`std::optional`. Pick one
