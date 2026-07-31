# SDL_net multiplayer transport strategy

## Scope

This document proposes a non-Windows multiplayer transport for Lego Loco. The
original Win32 path remains assembly-derived DirectPlay code. The host path will
preserve the recovered Lego Loco application payloads where practical, promote
all gameplay traffic to reliable TCP, and replace DirectPlay session/system
behavior. It will therefore not interoperate with the retail executable.

Research sources:

- Canonical game binary: `lego-loco-unpacked/Exe/loco.exe`, analyzed through
  Ghidra as 32-bit x86/MSVC.
- SDL_net upstream commit `4dd9d8418e9781d10481fd6177e3a1064ca201fe`.
- Avahi upstream commit `9ee8d90fe7fdb763358b2e40b38deb9ef0fca009`;
  authoritative `org.freedesktop.Avahi.*` D-Bus introspection XML and daemon
  implementation were checked directly.
- Embedded DNS-SD fallback `mjansson/mdns` upstream commit
  `a569c4759bd47e0f2a7bfc4d4c19620445782806`.
- Stable API checked against SDL_net tag `release-3.2.0`; the networking API
  used below is unchanged on upstream main. Nixpkgs exposes this as
  `sdl3-net` 3.2.0 and pkg-config module `sdl3-net`.

Context7 was attempted first, but its monthly quota was exhausted. The official
upstream source, public header, migration guide, and examples were inspected
instead.

## What the original game actually does

There are three distinct layers; they should not be collapsed into one class:

1. `Netman` is the 0x804-byte game/session model. Its constructor is
   0x43D0A0 and its per-frame queue drain is 0x43F0C0.
2. `TrainSubsystem` is the 0x38-byte asynchronous network dispatcher. Its
   constructor is 0x438BC0, queue function is 0x4393D0, queued-command
   dispatcher is 0x439550, and receive dispatcher is 0x4396C0.
3. The DirectPlay peer wrapper is a separate 0x160C-byte allocation created by
   `TrainSubsystem::InitNetwork` at 0x4391A0. The embedded
   `IDirectPlay4A*` is at original offset +0x1588, local DPID at +0x924, and
   connected state at +0xD50.

The existing `NETWORKING.md` and transcribed network sources are useful
indexes, but they are not canonical and contain claims that need assembly
validation before implementation. One confirmed correction is the application
GUID. Raw bytes at 0x479158 are:

`46 25 CD F9 7F 57 D2 11 94 26 00 A0 24 4B DA 7A`

Interpreted as a Windows GUID, this is
`{F9CD2546-577F-11D2-9426-00A0244BDA7A}`. This investigation corrected the
stale field ordering in `NETWORKING.md`, `src/network/netman.h`, and the
related transcribed constant/comment.

### Payload contract

`WIN32_SendNetworkData` at 0x460D40 is the lowest application send boundary.
Disassembly proves that it:

- writes little-endian uint16 value 300 (0x012C) at payload bytes 2..3;
- uses the local DirectPlay ID at peer +0x924 as the source;
- treats destination 0 as the DirectPlay all-players destination;
- passes send flag bit 0 to DirectPlay, where bit 0 is
  `DPSEND_GUARANTEED`;
- returns 1 after an accepted send and 0 on failure (not a byte count);
- chooses DirectPlay `Send` or `SendEx` according to peer capability state.

This means the application wire payload starts with:

`uint16 packet_type; uint16 protocol_version = 300; ...`

and is already suitable for retaining as an opaque legacy payload. It does
_not_ include message length, source player, destination player, session
identity, or transport framing.

The original uses both delivery classes. Assembly-verified examples:

- Guaranteed: disconnect/control data (0x43D250), map/building data
  (0x43D350/0x43D500), 0x228-byte player-layout state (0x440070), map sync
  (0x43B060), attachment chunks (0x439DF0), and connection setup.
- Best effort: periodic player/train presence from 0x43DF20 and train movement
  updates from 0x43BFB0. The queue at 0x4393D0 drops a non-guaranteed type-6
  send once queue depth reaches six, but does not apply that drop rule to
  guaranteed traffic.

The SDL host intentionally strengthens the second class: every admitted legacy
payload is delivered through the same reliable TCP path. Original reliability
flags remain useful evidence and diagnostics, but do not select a host transport.

`WIN32_PeekMessageLoop` at 0x4606D0 preserves application packet boundaries,
returns the sender DPID next to the payload, rejects payload version != 300,
and translates DirectPlay system messages into game-facing packets. Confirmed
synthetic types include 0x0A (connection/session lost), 0x14, 0x3C, 0x46, 0x50,
and 0x5A. A version mismatch causes a 4-byte type-0x1E/version-300 response to
the sender. A replacement must synthesize equivalent join/leave/session events;
SDL_net does not provide them.

### Session/lobby behavior

- Hosting opens a DirectPlay session with a maximum-player count and
  application GUID (0x45FD80).
- Joining enumerates sessions, selects an instance GUID, opens it, then creates
  the local player (0x460360 and 0x45E730).
- Player/session discovery, player IDs, all-player sends, and system
  notifications are DirectPlay services, not part of the Lego payload.
- The recovered lobby functions at 0x40AA20, 0x40AAF0, 0x40ABA0, and 0x40AC50
  are direct calls, not extra vtable slots. They now live in the compiled
  `ui/GameSetupPanel_network.cpp`; successful host/client admission is finalized
  through `ConnectToNetworkGame` before the original Go control is exposed.
- The host Search action in `src/decompiled_cpp/ui/GameSetupPanel.cpp` now
  browses asynchronously and routes selected resolved endpoints through the
  SDL_net handshake. Host mode binds first and only then starts DNS-SD
  publication; see `docs/sdl-net-wire-protocol.md`.

## SDL_net 3 comparison

| Requirement | DirectPlay 4 behavior used by Loco | SDL_net 3 primitive | Adapter work required |
|---|---|---|---|
| Initialization | COM/service-provider setup | `NET_Init` / `NET_Quit` | RAII lifetime and host-only build wiring |
| Session discovery | Enumerate application-GUID sessions | No mDNS/DNS-SD API | Separate service registration/browser plus direct-connect UI |
| Host/listen | Open/create session | `NET_CreateServer` | Session metadata and admission policy |
| Join | Select session and create player | `NET_ResolveHostname`, `NET_CreateClient` | Endpoint parsing, handshake, protocol checks, virtual player ID assignment |
| Guaranteed traffic | Packet-oriented guaranteed Send | TCP `NET_StreamSocket` | Length framing, routing envelope, output limits |
| Original best-effort traffic | Packet-oriented non-guaranteed Send | Same TCP stream | Promote to reliable delivery; do not retain host-side stale-drop behavior |
| Packet boundaries | Preserved by DirectPlay | Absent from TCP | Frame decoder handling split/coalesced reads |
| Source/destination IDs | DPID supplied out-of-band | None | Stable virtual player IDs and routing table |
| Broadcast to players | DPID 0 | No session abstraction | Host fan-out over admitted TCP streams |
| Join/leave/system events | Generated by DirectPlay | None | Explicit control frames translated to game-facing events |
| Polling | Nonblocking DirectPlay Receive | Nonblocking APIs and `NET_WaitUntilInputAvailable` | One I/O owner; iterate all sockets after wakeup |
| IPv4/IPv6 | Provider-dependent | Opaque addresses and dual-stack bind | Endpoint parser plus DNS/mDNS result handling |
| Backpressure | DirectPlay/game queues | SDL_net queues unsent stream data internally | Explicit per-peer caps; disconnect stalled peers instead of dropping frames |
| Threading | Existing game network thread and locked queues | Different sockets are thread-safe, same socket requires serialization | A single transport thread owns every SDL_net socket |

Important SDL_net details from the official source/header:

- All operations are nonblocking except functions named `NET_Wait...`.
- `NET_WriteToStreamSocket` copies unsent data into an internally growing
  queue; it does not supply message framing and its queue has no application
  high-water limit.
- `NET_ReadFromStreamSocket` may return any positive partial byte count, 0
  for no data, or -1 for end-of-stream/failure.
- `NET_WaitUntilInputAvailable` reports how many objects are ready but not
  which objects; all candidate objects must then be tried nonblockingly.
- `NET_ResolveHostname` asynchronously resolves hostnames and numeric IPv4/IPv6
  addresses for `NET_CreateClient`, but SDL_net does not register, browse, or
  resolve DNS-SD service records. Discovery therefore remains a separate layer.

## Recommended architecture

### 1. Preserve legacy payloads behind a transport-neutral boundary

Create a typed host-only interface, for example:

- `src/sdl3_shims/sdl3_net_transport.h`
- `src/sdl3_shims/sdl3_net_transport.cpp`
- `src/sdl3_shims/sdl3_net_protocol.h`

The interface should expose host, discover, join, close, send, and poll using
native C++ types. Do not emulate `IDirectPlay4A`, manually build a fake COM
vtable, or force a native object into the original 0x160C-byte x86 layout.
Keep all host implementation and includes under the exact `#ifndef _WIN32`
boundary. The original Windows path continues to call the recovered DirectPlay
functions.

At the game boundary, retain this value:

`ReceivedLegacyPacket { VirtualPlayerId sender; vector<uint8_t> payload; }`

The existing TrainSubsystem and Netman dispatchers should continue to see the
same payload bytes and sender identity they received from
`WIN32_PeekMessageLoop`.

### 2. Add an outer transport envelope

Do not modify bytes inside a legacy packet except for the original send-time
version stamp at bytes 2..3. Prepend an SDL-host transport frame containing at
least:

- magic and transport protocol version;
- frame kind (handshake, legacy payload, join, leave, session end, etc.);
- source and destination virtual player IDs;
- payload length;
- optional original DirectPlay flags for diagnostics, not transport selection.

Encode every field explicitly; do not transmit native C/C++ structs. Reject
unknown versions, invalid destinations, payloads shorter than four bytes,
legacy version mismatches, and payload lengths above a documented limit. A
64-KiB reliable-frame limit covers the largest currently recovered attachment
chunks (about 0x7FEC bytes including their header) while leaving room for all
known map/layout packets.

For TCP, maintain a receive accumulator per peer and parse zero or more complete
frames after every partial read. Tests must split headers/payloads at every byte
boundary and coalesce multiple frames into one read.

### 3. Use a host-relayed star topology

The session host owns one `NET_Server` and one accepted TCP stream per client.
The host assigns stable nonzero virtual player IDs. Clients send destination IDs
to the host; the host delivers locally or relays to one/all admitted clients.
This reproduces DirectPlay routing without opening peer-to-peer NAT and trust
problems. SDL_net needs no gameplay or discovery datagram socket; only the
embedded DNS-SD fallback owns UDP multicast sockets.

Destination 0 remains the game-level all-players address. Whether a broadcast
is reflected to its sender must be validated per original call flow and locked
by tests; do not guess this in the codec.

### 4. Separate discovery from gameplay

Use DNS-Based Service Discovery (DNS-SD) as the discovery model and mDNS as the
default LAN backend. Advertise a service such as `_legoloco._tcp.local.`. Its
SRV record supplies the TCP host/port; TXT records can carry a bounded transport
version, legacy version 300, session UUID, display name, and current/max player
counts. Keep the TCP handshake authoritative because DNS records are cached and
may be stale. Deduplicate browser results by session UUID and publish a goodbye
record when hosting stops.

mDNS/DNS-SD should work well on ordinary home and office LANs because it needs
no central server and naturally supports IPv4/IPv6. It is not universal: guest
Wi-Fi isolation, VLANs, multicast filtering, VPNs, containers, and routed
networks often block `.local` multicast. Discovery failure must never prevent a
direct connection.

Put every discovery implementation behind one host-only, transport-independent
interface. The interface deals only in typed advertisements, resolved endpoints,
and add/update/remove/status events; it must not expose D-Bus object paths, Avahi
interface/protocol integers, `DNSServiceRef`, Android Java objects, Windows API
handles, raw DNS records, or UDP sockets. At minimum it provides nonblocking
start/stop browse, publish/update/withdraw, poll, backend identity, and a fatal
health result. A coordinator owns exactly one backend at a time and converts all
backend output to the same session model. The intended contract shape is:

```cpp
struct EndpointCandidate {
    std::string host_or_numeric_address;
    uint16_t port;
    uint32_t scope_id;  // Portable interface scope for link-local IPv6, else 0.
};

struct SessionAdvertisement {
    std::string session_uuid;
    std::string display_name;
    uint16_t transport_version;
    uint16_t legacy_version;
    uint8_t current_players;
    uint8_t max_players;
    uint16_t tcp_port;
};

struct DiscoveredSession {
    SessionAdvertisement metadata;
    std::vector<EndpointCandidate> endpoints;
};

enum class DiscoveryEventKind { Added, Updated, Removed, Ready, BackendChanged };
enum class BackendHealth { Running, Fatal };

class IDiscoveryBackend {
public:
    virtual ~IDiscoveryBackend() = default;
    virtual DiscoveryBackendKind kind() const noexcept = 0;
    virtual StartResult Start(const DiscoveryRequest&) = 0;
    virtual UpdateResult Update(const SessionAdvertisement&) = 0;
    virtual PollResult Poll() = 0;  // Events plus BackendHealth; never blocks.
    virtual void Stop() noexcept = 0;
};
```

`DiscoveryCoordinator` owns backend factories, the original request, a monotonically
increasing backend generation, and the fallback policy. Consumers key sessions by
UUID and generation rather than a backend-specific browse handle. Calls are confined
to the discovery/network thread; game/UI code receives owned events through the
existing queue boundary.

Implementation status: `network_discovery.{h,cpp}` provides the contract and
coordinator, and `discovery_runtime.{h,cpp}` confines it to a dedicated worker
thread with queued requests and owned UI snapshots. The recovered lobby Search
control now starts browsing, renders discovered names/player counts, and stops on
panel exit. `avahi_dbus_discovery.{h,cpp}` implements the desktop-Linux primary. Its deterministic regression hosts a fake `org.freedesktop.Avahi` on an
isolated private bus and covers Server2, v1 fallback, publication/update, browse/
resolve/remove, daemon loss, and cleanup. The pinned embedded fallback is also
implemented and tested in a private user/network namespace with its own multicast
interface. Native-platform adapters remain.

Default backend order is platform-specific:

1. Standard desktop Linux tries **Avahi over the system D-Bus**. This lets the
   existing daemon own multicast sockets, interface changes, cache coherency,
   probing, collision handling, and publication lifetime. Use
   `org.freedesktop.Avahi` at `/`: `EntryGroupNew` plus `AddService`/`Commit`/
   `UpdateServiceTxt` for publication, and `ServiceBrowserNew` followed by
   `ServiceResolverNew` for browse results. Install signal matches before object
   creation. Prefer the `Server2` Prepare/Start API when available, with the
   stable v1 API as compatibility fallback.
2. If the system bus, Avahi name owner, API, browser, resolver, or entry group is
   unavailable, desktop Linux falls back to pinned **`mjansson/mdns`**. The
   embedded backend then owns interface enumeration, periodic queries and
   announcements, TTL expiry, deduplication, probing/conflicts, packet validation,
   and UDP-5353 lifecycle.
3. macOS/iOS use the native Bonjour DNS-SD API, Windows uses its native DNS-SD
   API, and Android uses `NsdManager`. Each remains a small adapter behind the
   same interface. Unknown/headless POSIX targets may select the embedded backend.

Do not run two publishers or merge two browsers concurrently. On a fatal backend
error, withdraw/stop it, clear its discovered-result generation, emit a backend
transition event, and start the next backend with the original request. Once a
session has fallen back to embedded mDNS, do not switch back merely because
Avahi later appears; retry the preferred backend on the next explicit discovery
or hosting lifecycle. This avoids duplicate services and oscillation. Direct
Connect remains independent and available even when every discovery backend
fails.

The embedded browser uses ephemeral query sockets and requests unicast replies,
so it can browse even when another local responder owns UDP 5353; it also listens
on 5353 opportunistically for announcements and goodbye records. Publication is
stricter. Testing on NixOS with active systemd-resolved confirmed the RFC 6762
section-15 risk: both stacks could bind, but multicast queries were consumed by
the existing wildcard responder and never reached the embedded publisher. The
backend therefore probes for an existing IPv4 UDP-5353 owner and refuses embedded
publication rather than silently advertising an unreachable session. Hosting then
reports discovery unavailable while Direct Connect remains usable.

The isolated regression gives the embedded backend sole ownership of UDP 5353 and
proves probing, PTR/SRV/TXT/A/AAAA publication and resolution, periodic queries,
TXT updates, TTL reconciliation, goodbye removal, duplicate-host rejection, and
cleanup. The preferred Avahi D-Bus backend avoids the second-stack problem entirely.

Unicast DNS-SD can use the same SRV/TXT schema under a configured domain, for
example `_legoloco._tcp.games.example.org.`. This works across routed networks
when an administrator or future rendezvous service publishes the records, but
it is not automatic Internet-wide discovery: the client must know which domain
to browse. Treat configured unicast discovery domains as optional additions to
the local `.local` browser.

The lobby must also expose **Direct Connect**. Accept a hostname or numeric IP
with an optional port: `host`, `host:port`, an IPv4 address, or `[IPv6]:port`.
Use the configured default game port when omitted. Parse the endpoint first,
then pass only the host/address portion to asynchronous `NET_ResolveHostname`
and the port to `NET_CreateClient`. Numeric addresses, ordinary DNS names, mDNS
results, and unicast DNS-SD results all enter the exact same TCP handshake and
version-validation path. A direct endpoint bypasses browsing, not admission or
protocol checks. Across the Internet, direct IP/DNS still requires a reachable
host port (for example firewall allowance and IPv4 port forwarding); neither
DNS-SD nor SDL_net supplies NAT traversal or a relay.

### 5. Promote all gameplay traffic to reliable TCP

Carry every admitted legacy payload over the framed TCP connection, including
the periodic presence and movement packets that originally cleared
`DPSEND_GUARANTEED`. This removes UDP endpoint association, sequencing,
retransmission policy, and a second gameplay polling path. With at most nine
players and small periodic state packets, this is a favorable simplicity trade.
The approximately 32-KiB maximum recovered attachment frame may briefly precede
a movement update on the stream, but should be acceptable; measure this before
considering multiple TCP channels.

On the non-Windows path, do not apply the original queue-depth-six stale-drop
rule to promoted messages. Once accepted, a frame is reliable. Preserve the
original flag only as diagnostic metadata so traces can still identify which
traffic DirectPlay treated as best effort.

### 6. Preserve queue and ownership behavior

A single transport thread should own all SDL_net objects. Game-to-network and
network-to-game communication uses owned message queues. This matches the
existing TrainSubsystem shape and obeys SDL_net's same-socket serialization
rule.

The original queue-depth-six stale-drop remains documented and unchanged on the
Win32 DirectPlay path, but the host TCP path must not drop those promoted frames.
Add a byte high-water mark for each peer. If a peer cannot drain reliable traffic
within the limit/timeout, disconnect it and synthesize the normal connection-loss
path; never silently discard an accepted frame. Poll
`NET_GetStreamSocketPendingWrites` because SDL_net's internal queue otherwise
grows with producer load.

## Incremental implementation plan

1. **Protocol audit and codecs**
   - Correct the application GUID documentation.
   - Inventory each packet builder/consumer and its exact size, destination,
     ownership, and guaranteed bit from assembly.
   - Centralize little-endian readers/writers and bounds validators. Do not use
     packed native wire structs.
   - Add golden tests for the common 4-byte header, 0x228-byte layout state,
     0xB1C-byte map sync, and maximum attachment chunk.

2. **Standalone SDL_net loopback transport — implemented**
   - `sdl3-net` 3.2 is in `flake.nix` and the root Makefile.
   - `sdl3_net_protocol`, `sdl3_net_transport`, and `sdl3_net_runtime` provide
     framing, authoritative handshake, host relay, virtual IDs, queue caps, and
     one-thread SDL_net ownership without game globals.
   - Deterministic tests cover every partial-read boundary, coalesced/malformed
     frames, stale UUID rejection, two-process loopback routing/leave, and a
     two-process DNS-SD publish → resolve → join → payload flow.

3. **Discovery abstraction, Linux backends, Direct Connect, and lobby slice — implemented**
   - Add a typed backend/coordinator contract with deterministic startup and
     fatal-runtime fallback tests; no platform handle may cross the boundary.
   - Implement Avahi over the system D-Bus as desktop Linux primary, including
     publication, TXT updates, browsing, resolution, daemon loss, and cleanup.
   - Vendor pinned `mjansson/mdns` as Linux/headless fallback and implement its
     cache, probing, interface, announcement, and socket lifecycle policy.
   - Add native-backend seams for Bonjour, Windows DNS-SD, and Android `NsdManager`
     without making those platform SDKs dependencies of the core abstraction.
   - Add endpoint parsing and a Direct Connect action for hostnames, IPv4, and
     bracketed IPv6 with an optional port.
   - Replace the host Search zero-result path with asynchronous browse results
     stored as typed host-only session data rather than x86-layout objects.
   - Render/select discovered sessions and wire both discovered and direct
     endpoints into the recovered 0x40AA20 join flow. Move the four now-recovered
     GameSetupPanel methods out of the dedicated stub file.

4. **Host/join/player lifecycle — implemented**
   - Implement handshake and virtual IDs for up to nine players.
   - Synthesize the game-facing join, leave, connection-lost, and session-ready
     events currently produced at 0x4606D0.
   - Run host and client as separate processes on loopback.

5. **Reliable legacy payload integration — partial**
   - Route TrainSubsystem type-6 sends through framed TCP.
   - Stamp legacy version 300 exactly at send time and validate it at receive.
   - Game-facing receive translation covers layout selection/state,
     connection/controller setup, flags, train position, player count, pixel
     data, building requests, player join/leave, attachments, service/file
     packets `0x3EA`–`0x3EF`, and deferred ping packet `0x3F6`.
   - `0x3EA` now has a bidirectional `0x3EC` response; `0x3EE` replaces
     host-owned asset bytes; `0x3F6` uses recovered type-`0x15` PingEntry
     insertion/movement/removal; and `0x3F9` replaces slot-owned pixel data.
   - Non-empty `0x3EC` records now decode into typed, Train-owned
     `DPlayManager` objects without native-vptr aliasing. `0x3EE` proves
     replace-on-key asset ownership, and `0x3FC` assembles validated
     FIRST/INTERIM/FINAL blocks into separate attachment/final byte arrays.
   - Decoded sessions are now grouped under a native-safe `Vehicle` and
     adopted by Netman through recovered type `0x0F`. `InboundTrainNode` is a
     canonical alias rather than a second byte layout; original tail metadata
     is represented by named native-width fields.
   - `DownloadMissingAssets` now scans typed DPlayManager entries, consumes
     matching host-owned `0x3EE` bytes, and requests only absent keys. Typed
     inbound inspection and game-start editor indexing replace their original
     +0x0C/+0x14 offset accesses; an isolated real-client Go flow proves the
     adopted Vehicle survives the recovered loading handoff.
   - Host route resolution now clones the typed `DPlayManager` state supplied
     by `0x3EC` instead of reopening local PostBag `.crd` files. The recovered
     `SendSignalChange` path creates a native Vehicle clone, preserves the
     session tail marker, and enters loading with a two-node typed list.
     `HandleTimeout`/`SerializePlayerData` and host missing-route filling now
     operate on typed editors and native-width Vehicle pointers.
   - Remaining work: migrate the later movement/position packet paths and the
     Windows-only PostBag deserializer, then validate live train transfer plus
     transport backpressure in mode 3.

6. **Full subsystem and UI integration**
   - Exercise train transfer, map/building overlays, scenario selection,
     attachment transfer, disconnect, lobby return, and host shutdown.
   - Add isolated Wayland flows for Search -> select -> join -> lobby -> exit.

## Test gates

- Codec unit tests with malformed and boundary-sized data.
- In-memory router tests for unicast, all-players, no-session, and invalid ID.
- SDL_net loopback tests with intentionally fragmented/coalesced TCP input.
- Endpoint parser tests covering default ports, DNS names, IPv4, bracketed
  IPv6, invalid ports, and ambiguous input.
- Backend-contract tests for primary selection, startup failure, fatal runtime
  failover, generation clearing, no duplicate publisher, and no fallback oscillation.
- Avahi D-Bus tests for publication/update/withdraw, add/resolve/remove, collision,
  daemon absence/restart, malformed TXT, and object cleanup on a private bus.
- Embedded fallback tests for stale TTL expiry, duplicate session UUIDs, probing/
  conflicts, malformed records, interface changes, and UDP-5353 coexistence with
  Avahi in both startup orders.
- Two-process host/client tests by direct loopback address, then through mDNS,
  followed by a nine-client admission/load test.
- SDL_net simulated stream lag tests proving queue bounds and connection-loss
  behavior without silently losing accepted frames.
- Host-disconnect and client-disconnect event tests.
- `make test` for deterministic tests and `make test-integration` after
  lobby/runtime UI wiring.

## Recommended first code milestone

Deliver one host and one client that connect by an explicitly entered loopback
address, complete a versioned handshake, receive virtual player IDs, exchange a
framed four-byte legacy test packet with version 300, and synthesize join/leave
events. Then publish and browse the same endpoint through mDNS/DNS-SD without
changing the connection path. Do not begin with train/file-transfer payloads.
This proves connection, routing, packet framing, identity, and system events
independently from discovery, while preserving Direct Connect as a permanent
fallback.
