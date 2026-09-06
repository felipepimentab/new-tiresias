# Atomic prescription transfer: evaluation

Status: proposed follow-up. Protocol v5 implements atomic complete-parameter
communication and persistence. It does not implement prescription transactions.

The current prescription contains IDs 3–13: eight 136-byte compressor LUTs and
three four-byte phase-compensation gains. Its value payload is 1,100 bytes.
An ordinary v5 load needs 11 SETs instead of 275 four-byte SETs. An unchanged
parameter does not cause a flash write or revision increment.

## Recommendation

Stage a prescription in a fixed firmware buffer, then publish it with one
explicit commit. Leave the previous prescription active throughout staging.
Communication failure before commit then requires no compensating writes and
no working workstation connection to restore the old prescription.

Use a small, compile-time transaction representation:

- one 1,100-byte staging buffer;
- an 11-bit received-parameter mask;
- a transaction token, expected active generation, contract CRC, and declared
  prescription checksum;
- one session owner and a transfer deadline;
- no heap allocation, generalized object store, or arbitrary parameter sets.

Allow normal GETs to read the active prescription while staging. Reject ordinary
SETs to the prescription's IDs until the transaction ends, or reject all SETs
for the simplest initial policy. Never expose the staging buffer through GET.
The fixed parameter list belongs in one compile-time definition on each side.

## Proposed operations

1. `BEGIN_PRESCRIPTION(token, expected_generation, contract_crc, checksum)`
   validates compatibility and ownership, then initializes the staging mask.
2. `STAGE_PARAMETER(token, parameter_id, complete_value)` validates membership
   and exact size before copying one whole value into its fixed buffer offset.
   A repeated identical stage is harmless; reject conflicting duplicate data.
   Staging acknowledgment means received, not persisted or activated.
3. `COMMIT_PRESCRIPTION(token)` requires all 11 bits and a matching checksum.
   Persist the complete candidate, then publish all values under the parameter
   mutex and advance the prescription generation once. Readers cannot observe
   the copying of individual buffers while that mutex is held.
4. `ABORT_PRESCRIPTION(token)`, disconnect, or staging timeout discards the
   staging state. The old active prescription remains untouched.
5. `GET_PRESCRIPTION_STATE` returns active generation, checksum, and the last
   committed token. It resolves a lost commit indication after reconnect.

Bind staging ownership to the connection session. Once commit has begun, let it
finish as one local transaction even if BLE disconnects. Do not automatically
undo an already committed prescription merely because its acknowledgment was
lost. On reconnect, the durable token/generation tells the workstation whether
commit occurred. Repeated COMMIT for that token returns the recorded outcome.

Checksums detect incomplete/corrupt payloads; they do not authenticate a sender.

## Persistence choices

| Approach | Communication failure | Reboot/power-loss recovery | Cost |
| --- | --- | --- | --- |
| Workstation reads old values and restores them on failure | Cannot restore while disconnected | No firmware-owned fallback | Small firmware change, weak guarantee |
| RAM staging, then 11 current Settings writes | Old values survive transfer failure before commit | Can leave a mixed prescription during commit | About 1.1 KB RAM, insufficient durable atomicity |
| One whole-prescription Settings record | Publish only after complete record save | Relies on NVS record commit semantics | Simplest durable active snapshot; previous version not explicitly retained |
| A/B whole-prescription records plus active selector | Previous prescription remains selected until commit | Explicit retained fallback and deterministic selection | Two records plus small selector; recommended if previous prescription must remain available |

The first two options are insufficient for a durable atomic prescription. A
single snapshot is enough if retaining an explicitly addressable previous
prescription is unnecessary. For the requested fallback, prefer A/B snapshots.

Each snapshot should contain an explicit format version, contract CRC, token,
generation, length, checksum, and all 1,100 value bytes in fixed ID order. Use
field-by-field encoding. Store both the generation and checksum in the selector
so it cannot select an overwritten or mismatched slot accidentally.

Commit sequence:

1. Write the complete candidate into the inactive slot.
2. Validate the saved record, including exact size and checksum.
3. Atomically save a small selector referencing that slot and generation. This
   is the durable commit point; no earlier record becomes active merely because
   it has a larger generation number.
4. Publish the complete candidate to RAM under the parameter mutex, then emit
   the terminal result. Block readers across the durable-selector/RAM switch.

Before step 3, a reboot chooses the old selected slot. After step 3, a reboot
chooses the new one. The other slot retains the previous prescription until the
next update. Boot must validate the selector and chosen record, with a defined
recovery policy for a corrupt selector or snapshot. Distinguish factory defaults
from validated previous snapshots and report recovery explicitly.

Review NVS sector capacity, garbage collection reserve, write-error outcomes,
and selector atomicity against the installed Zephyr implementation before
implementation. Logical A/B slots are not inherently independent physical flash
failure domains. Fault-injection tests must cover each persistence boundary,
including a successful selector save followed by reset before RAM publication.
Do not claim physical power-loss guarantees from host mocks alone.

## Integrating independent parameter writes

A prescription snapshot must become authoritative for its 11 members. Continuing
to overlay their old per-parameter keys at boot would silently break transaction
semantics. Define a migration: when no valid snapshot exists, construct the first
one from current defaults/per-parameter values. Once migrated, ignore obsolete
keys for those members. IDs outside the prescription retain independent storage.

An individual SET to a prescription member must subsequently create a new full
snapshot with that one parameter changed, or be explicitly disallowed. It cannot
continue writing an independent key while claiming one coherent active snapshot.

## DSP boundary

A software transaction does not make 11 independent DSP writes atomic. The
existing optional adapter applies a parameter before flash and can partially
change physical hardware on an I/O error. Keep BLE/flash transaction guarantees
separate from hardware activation.

Before integrating live DSP activation, choose an explicit mute/pause-and-apply
or DSP bank-switch procedure. Retain a validated previous snapshot for recovery;
if new hardware application fails, restore it before resuming audio. Failure to
restore must produce an explicit error state rather than report successful
rollback. The durable selector, hardware activation order, and boot replay policy
must be designed together. This is outside the current BLE/persistence scope.

## Verification required for the follow-up

Use offline fault injection to interrupt every staging boundary and each flash
commit step. Verify that GET and reboot restoration return one complete old or
new prescription, never a mixture. Cover missing/duplicate parameters, stale
sessions, checksum mismatch, timeout, disconnect, lost commit acknowledgment,
repeated commit, generation conflict, and individual-SET migration behavior.
Hardware or physical power-loss tests require separate authorization.
