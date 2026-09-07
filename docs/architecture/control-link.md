# Control Link and Remote Management

## Status

Implemented MVP: shared Bluetooth initialization, connectable advertising, one peripheral
ACL, Device Information Service, the custom Tiresias service, fixed-contract parameter
access, complete parameter-image persistence, advertising restart, and LED 1 indication.

## Service boundaries

| Service | Responsibility |
|---|---|
| Tiresias Control Link | Product-specific commands, status, events, and fixed DSP parameters |
| Device Information Service | Standard static identity; implemented |
| BASS | Standard Broadcast Assistant procedures |
| MCUmgr/SMP | Firmware update; do not duplicate in the custom service |
| VCS/HAS/PACS/CSIS and others | Adopt when their standard semantics match a feature |
| Battery Service | Future hardware only; current board cannot measure battery level |

Use one custom service with focused characteristics. Add another vendor service only for a
materially different lifecycle, authorization boundary, or sustained data rate.

## Ownership

Control Link owns the remote session, negotiation, authorization, wire-protocol parsing,
transaction IDs, response delivery, remote status aggregation, contract identification, and
disconnect cancellation.

It does not own Bluetooth stack setup, ADAU1787 access, device/audio policy, broadcast
synchronization, BASS state, firmware update, or audio buffers. These belong to Bluetooth
Management, Codec Controller, Device Controller, Audio Streaming, MCUmgr, and the audio
data plane.

Control and broadcast reception are independent. `READY` + `STREAMING`, a control session
without a broadcast, and broadcast reception without a control peer are all valid. V1
supports one incoming control/Broadcast Assistant ACL.

## State model

| State | Meaning |
|---|---|
| `DISABLED` | Connectable control unavailable |
| `ADVERTISING` | Accepting an ACL |
| `LINKED` | ACL present; session admission is in progress |
| `READY` | Tiresias requests are admitted |
| `ERROR` | Persistent local Control Link failure |

- Enter `ADVERTISING` only after the indexed advertising-start event.
- A peripheral ACL moves through `LINKED` to `READY`. The trusted-workstation MVP has no
  authorization gate.
- Disconnect cancels peer-owned work and requests advertising restart.
- Malformed, unauthorized, busy, or rejected requests return protocol errors; they do not
  enter `ERROR`.

## Custom service

Protocol v5 transfers one complete parameter per operation. It is incompatible
with v4; update firmware and workstation together. The service UUID remains
`7b9a0001-6e4f-4b2d-a9c8-4f2e6f5d1000`. Discovery uses this UUID, not the local
name. Characteristic UUIDs use the same suffix; `0003` remains reserved.

| Characteristic | Access | Value |
| --- | --- | --- |
| Protocol Information (`0002`) | Read | 24 bytes |
| Status (`0004`) | Read, notify | 16 bytes |
| Request (`0005`) | Write with response | 8-byte header, then 0–180 bytes |
| Response (`0006`) | Indicate | 12-byte header, then 0–180 bytes |

Standard DIS identity fields remain optional and are read independently.
Metadata integers are little-endian; parameter bytes have no transport-level
numeric interpretation or byte order. Headers are explicitly encoded, never
native C structures.

### Wire records

- Protocol Information: `<BBHIHHIII>` — major (5), minor (0), record size (24),
  capabilities, maximum request size (188), maximum response size (192), contract
  CRC32, boot ID, parameter revision.
- Request header: `<BBIBB>` — opcode, flags (zero), nonzero transaction ID,
  parameter ID, payload byte count. Payload immediately follows the header.
- Response header: `<BBIBBI>` — opcode, result, transaction ID, parameter ID,
  payload byte count, committed revision. Payload immediately follows the header.
- Status: `<BBBBIIBBH>` — control state, flags, last result, reserved zero,
  revision, last transaction ID, last parameter ID, reserved zero, reserved zero.
  Byte 13, formerly the chunk offset, is reserved in v5.

GET (opcode 1) has no request payload and returns the entire parameter snapshot.
SET (opcode 2) must carry exactly the contract byte count and returns the entire
confirmed value. A failed operation returns only the response header, with a
zero payload count. Result codes retain their v4 values: OK, bad request, not
found, read-only, out of range, busy, persistence failure, internal error, DSP
failure. Admission failures are ATT errors, not application results.

The 15-entry contract CRC32 is `0x7e8eb66c`. IDs 3–10 each contain a writable
136-byte compressor LUT; ID 15 is a writable 180-byte soft clip LUT. Headroom
and SoftClip can now be loaded as a separate output-stage configuration.
Firmware and workstation catalogs must both have the updated access flag. Names live in the workstation and DSP addresses remain in firmware.

### MTU and framing

A request and its successful response must each fit a single ordinary ATT
operation. Required ATT MTU is `12 + parameter_byte_count + 3`: 19 for a four-byte
value (the default MTU of 23 suffices), 151 for a compressor LUT, and 195 for the
largest GET. Firmware is configured with a local L2CAP TX MTU of 498; the actual
negotiated peer MTU still determines admission. MTU negotiation is handled by
the BLE stack/central; the client does not split parameters to accommodate a
smaller MTU. A peer must negotiate at least 195 to use the whole catalog.

Firmware rejects insufficient MTU before queueing or changing any value. It
also rejects incomplete/oversized SETs, mismatched payload counts, nonzero
attribute offsets, write commands, and ATT prepare/execute writes. Long-write
fragments are deliberately not accumulated or applied. A central that attempts
a long write on a small-MTU connection receives an ATT error without a commit.
This avoids backend-dependent long-write assembly semantics.

The workstation uses Bleak's explicit `response=True` write API. Its API accepts
write-with-response values up to 512 bytes, but that API limit does not establish
the negotiated MTU or server support for long writes.
See [Bleak write documentation](https://bleak.readthedocs.io/en/latest/api/client.html#bleak.BleakClient.write_gatt_char).

### Atomicity and failure behavior

The service copies the complete accepted request into a fixed single-entry
queue. Its worker calls `codec_parameters_set()` exactly once, without merging
any old bytes. In the Bluetooth-only configuration, that function saves the
complete value under its parameter's Settings key, then updates the RAM mirror
and advances the revision once. Identical values are successful no-ops.
GET returns bytes and revision from one mutex-protected snapshot.

- Malformed or incomplete requests change nothing.
- Disconnect before a queued request is processed discards the request.
- Disconnect after processing starts may still permit a complete commit.
- A persistence failure leaves the old RAM mirror and revision intact.
- A lost indication after commit makes the outcome uncertain, not partial.
  Reconnect and read the complete parameter before deciding whether to retry.
- Each request requires a Response subscription, an authorized READY connection,
  and the sole request slot. Firmware retains indication storage until the stack
  releases it. Busy admission is an ATT insufficient-resources error.

The workstation requires matching transaction ID, opcode, parameter ID, exact
value length, and (for SET) exact echoed bytes. ATT write acknowledgment alone
never means successful persistence. The exchange has a deadline and no automatic
SET retries. Status is a snapshot, not a durable transaction log.

These guarantees cover communication-induced partial updates of each stored
parameter. Physical flash fault/power-loss behavior remains that of the Settings
NVS backend and has not been hardware-tested here. The optional ADAU1787 path
still applies hardware before persistence: physical DSP failures or persistence
failures following DSP application are not a transactional hardware guarantee.
The current build disables that path. Deferred-DSP capability reporting is
unchanged; it must be reconciled when hardware integration is validated.

### Prescriptions

The current prescription loader makes 11 atomic parameter SETs (eight LUTs and
three gains, 1,100 bytes). It does not commit the prescription atomically. A
failure can leave earlier complete parameters from the new prescription and
later complete parameters from the old one. Retrying the whole prescription is
idempotent for unchanged values, but requires restored communication.

The firmware document `docs/architecture/prescription-transactions.md` evaluates
an explicit begin/stage/commit protocol and durable A/B prescription records.
That design is proposed, not implemented by v5.

## Firmware integration

| Resource or policy | Owner |
|---|---|
| One-time `bt_enable()`, settings, callbacks, controller setup | Bluetooth Management |
| Advertising set creation/execution and physical event publication | Bluetooth Management |
| Set 0 policy, peripheral ACL, reconnection, Control Link state | Control Link |
| Scan, PA/BASE/BIG/BIS lifecycle | Audio Streaming |
| Startup policy | Device Controller |
| ADAU1787 validation and access | Codec Controller |

`bt_mgmt_init()` is mutex-protected and caches the first result for the boot; either Control
Link or Audio Streaming may call first. Codec Parameters initializes the idempotent
Zephyr Settings backend and loads individual DSP settings directly. The current build assigns
advertising set 0 only to Control Link. New advertising clients require an allocator/composer
and updated controller limits.

`bt_mgmt_adv_start()` is asynchronous. Control Link tracks the pending index and changes
state only on `BT_MGMT_EXT_ADV_STARTED`; `BT_MGMT_EXT_ADV_FAILED` reports failure. ACL
correlation uses the copied connection index. Borrowed connection pointers require an
explicit `bt_conn_ref()` before retention.

Bluetooth and GATT callbacks perform minimal validation, copy to bounded queues, and
return. Control Link routes semantic work; owning subsystem threads validate and execute
it. No callback performs I2C, waits for completion, or decides device policy.

## BASS and security

The same ACL may carry the custom service and standard BASS. A Broadcast Assistant selects
a source through BASS; Audio Streaming owns synchronization and updates Broadcast Receive
State; Device Controller decides audibility; Codec Controller changes presentation.

The build enables Scan Delegator capability, but the compiled subsystem path does not yet
initialize or route it. Add BASS solicitation, callback routing, and receive-state updates
without vendor source-selection opcodes.

The MVP trusts one workstation and does not require encryption, bonding, or physical presence.
It still validates structural framing, array bounds, sizes, queue capacity, and transaction IDs
on-device, but treats parameter bytes supplied by the workstation as opaque. Production authorization,
semantic validation, roles, audit records, and raw-memory maintenance access remain future work.

Audio deadlines take priority over management throughput. Keep buffers fixed, queues
bounded, notifications rate-limited, and concurrency at one parameter operation until
stack, queue, controller-memory, and underrun measurements justify expansion.

## Delivery order

1. Implemented: fixed contract, Status, indexed RAM reads, complete-image persistence, and the
   three-location synchronization lifecycle.
2. Next: Codec Adapter parameter-write implementation and hardware validation.
3. Optional dynamic contract generation, bulk writes, and whole-profile persistence.
4. BASS/Scan Delegator integration and simultaneous control/broadcast testing.
5. Production roles, audit-safe records, stress tests, and recovery hardening.

References: [BASS 1.0.1](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/BASS_v1.0.1/out/en/index-en.html),
[BAP](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/16212-BAP-html5/out/en/index-en.html),
[Zephyr LE Audio](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/api/audio/bluetooth-le-audio-arch.html).
