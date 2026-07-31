# SDL_net host wire protocol

This protocol is a non-Windows transport envelope. It is not part of the 1998
DirectPlay wire format; legacy packet payload bytes remain unchanged inside it.
The implementation is in `src/sdl3_shims/sdl3_net_protocol.{h,cpp}` and currently uses transport version
1 over one reliable SDL_net TCP stream per client.

## Frame header

All integers are explicitly little-endian. No native or packed C++ structure is
sent. Every frame begins with 24 bytes:

| Offset | Width | Field |
|---:|---:|---|
| 0 | 4 | ASCII magic `LOCO` |
| 4 | 2 | Transport version, currently 1 |
| 6 | 1 | Frame kind |
| 7 | 1 | Diagnostic flags |
| 8 | 4 | Source virtual player ID |
| 12 | 4 | Destination virtual player ID |
| 16 | 4 | Payload byte length |
| 20 | 4 | Per-sender sequence number |

Payloads are limited to 64 KiB. TCP readers retain partial frames and drain all
coalesced complete frames after each read. Invalid magic, version, kind, or
length permanently fails that peer decoder.

## Frame kinds

1. `ClientHello`
2. `ServerWelcome`
3. `LegacyPayload`
4. `PlayerJoined`
5. `PlayerLeft`
6. `Rejected`
7. `SessionEnded`

The host is virtual player 1, unadmitted clients use source 0, and the host
assigns client IDs 2 through 9. Destination 0 is broadcast. Client traffic is
always relayed by the host; clients never connect directly to each other.

## Authoritative handshake

`ClientHello` contains:

- canonical 16-byte application ID `f9cd2546-577f-11d2-9426-00a0244bda7a`;
- transport version 1;
- legacy protocol version 300;
- length-prefixed player name, at most 11 bytes;
- optional length-prefixed expected DNS-SD session UUID.

The server rejects wrong application/protocol versions, stale session UUIDs,
malformed names, full sessions, and any non-Hello first frame.
`ServerWelcome` returns the assigned and host player IDs, current/maximum
players, authoritative 36-character session UUID, and validated host player name.
The client projects that host identity through the same `PlayerJoined` event used
for later peers. DNS-SD metadata is only a connection hint; this handshake always
decides admission.

## Legacy payloads and backpressure

`LegacyPayload` requires at least four bytes and the original version-300
little-endian stamp at payload bytes 2..3. The transport does not otherwise
modify those bytes. Every send checks SDL_net's pending-write count and
terminates a peer instead of allowing more than 256 KiB to queue. The worker
retains inbound events up to 1,024 events or 4 MiB and disconnects the session
when either limit is crossed. Accepted TCP frames are never silently dropped.

### Game-facing legacy validation and ownership

The game bridge additionally validates recovered packet-specific layouts before
passing ownership to `TrainSubsystem`/`Netman`:

| Type | Host handling | Bounds/ownership |
|---:|---|---|
| `0x3EA` | Player-info control; records config and answers with `0x3EC` | exactly-used 9-byte header |
| `0x3EB` | Obsolete central-server/URL control retained as host state | NUL-terminated target; no browser or DirectPlay side effect |
| `0x3EC` | Track-build records decoded into typed `DPlayManager` routes, grouped under a native-safe `Vehicle`, then adopted through Netman type `0x0F` | exactly `0x14 + count * 0x390`, duplicate counts equal, count 0–3 |
| `0x3ED` | Original receive jump-table hole; accepted/discarded | at least 6 bytes |
| `0x3EE` | Replaces host-owned `(mode,type)` bytes; `DownloadMissingAssets` consumes matching typed route keys before filesystem fallback | exactly `0x0C + data_size` bytes |
| `0x3EF` | Reserved original jump-table hole | accepted/discarded |
| `0x3F6` | Copied to type-`0x15`; Netman creates/moves/frees `PingEntry` nodes | exactly `10 + count * 8`; all slots 0–8 |
| `0x3F7` | Decoded into an owned `TrainPositionAckPacket`, forwarded by Train type `0x17`, then consumed by Netman to remove the matching transfer ping; local ownership recovery uses typed Vehicle fields | exactly 12 bytes; both slot indices 0–8 |
| `0x3F9` | Copied to type-`0x16`; Netman replaces the sender slot pixel buffer | exactly `0x14 + data_size` and known virtual owner |
| `0x3FC` | FIRST/INTERIM/FINAL attachment blocks assembled in native ownership | exactly `0x10 + data_size`; monotonic sequence; `.att` and final `.dat` bytes remain separate |

The host-native asset/track containers are inside `#ifndef _WIN32`; original
Windows packet layouts and control flow remain separate. Non-empty `0x3EC` records are decoded without interpreting their original
four-byte vtable values as native pointers. One native-safe `Vehicle` is
constructed per packet, with a primary editor and one secondary editor per
record. `InboundTrainNode` is now an alias of `Vehicle`; its original +0x70
list link and +0x74..+0x89 metadata have named native fields, so Netman can
adopt the object through recovered message type `0x0F` without offset casts.

## Ownership and discovery ordering

`sdl3_net_runtime.{h,cpp}` is the process worker and sole SDL_net object owner
for the game. UI/game threads submit host, join, send, and stop commands and
consume owned events/snapshots.

Hosting binds the TCP listener first. Only after the worker reports
`Listening` does GameSetupPanel publish the session through DNS-SD. Browsers
consume resolved numeric A/AAAA candidates where available and pass the chosen
host, port, and expected UUID through the same client handshake used by Direct
Connect. Publication is withdrawn and sockets are stopped on lobby Exit,
Options, or panel destruction.
