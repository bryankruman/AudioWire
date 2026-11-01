# AudioWire pairing protocol review packet

Status: **prepared for independent review; no independent approval or security proof exists**.
The implementation baseline is commit `305fe54`; follow-up commits add compiler fixes and the review fixtures. Review the exact commit being proposed for release, not only this document.

## Scope and threat model

AudioWire pairs one Windows PC with one iPhone on a local network. Assume an attacker can discover services, open TCP connections, observe, alter, replay and inject LAN traffic, supply malformed messages, and race a legitimate pairing. The attacker does not initially know the six-digit code or stored pair key. A compromised endpoint, malicious OS, or stolen unlocked device is outside the protocol's confidentiality guarantee. Local discovery exposes device IDs, names, addresses and ports. Audio payloads require authenticated encryption.

A PC startup or explicit **Pair a new phone** action opens a two-minute code window with five online exchanges. Each accepted initial pairing request consumes an attempt before the PC supplies a password-dependent proof, including abandoned exchanges. Reopening a window is a local user action or app restart, not a network request. The six-digit code and QR link are sensitive live UI data; do not collect them in logs, screenshots submitted for review, or packet fixtures. All checked-in fixture values are deliberately public synthetic values and have no relationship to an actual device.

## Source map

| File | Boundary to review |
|---|---|
| `core/Pairing.cpp`, `core/Pairing.h` | Scalar derivation, mask points, exchange, confirmation, attempt gate |
| `core/SecureTransport.cpp`, `.h` | Authenticated envelope, counter limits, nonce layout, replay window |
| `app/Runtime.cpp` | TCP framing, identities, message order, persistence, resume, metadata and key separation |
| `platform/windows/DpapiKeyStore.cpp` | Per-user Windows protected storage |
| `platform/ios/KeychainKeyStore.mm` | Keychain accessibility and device-only storage |
| `tests/CoreTests.cpp`, `tests/ProtocolVectors.cpp` | Negative cases and candidate known-answer fixtures |

## Exact candidate construction

This is an application-specific SPAKE2-style composition, **not an RFC 9382 ciphersuite**. An auditor must assess the composition itself. Point/scalar operations use pinned libsodium 1.0.20 Ristretto255. All hashes below are libsodium BLAKE2b with the stated output length; BLAKE2b-256 is not truncation of BLAKE2b-512.

Let `L(s)` be a one-byte byte-length followed by the bytes of `s` (at most 255 bytes). Runtime identities are exactly sixteen lowercase hexadecimal ASCII bytes. `H64` and `H32` are unkeyed 64- and 32-byte BLAKE2b outputs. `Auth(k,label,data)` is HMAC-SHA256 with key `k` over `L(label) || data`. Scalars use libsodium's little-endian scalar representation; points use canonical 32-byte Ristretto encoding.

```text
context = L("AudioWire v1 pairing") || L(pcId) || L(phoneId)
M = ristretto_from_hash(H64("AudioWire v1 SPAKE2 Ristretto255 M"))
N = ristretto_from_hash(H64("AudioWire v1 SPAKE2 Ristretto255 N"))
w = scalar_reduce(H64(context || L(sixDigitCode)))
x,y = independent nonzero random scalars
X = x*G + w*M                    # PC message
Y = y*G + w*N                    # phone message
Zpc = x*(Y - w*N)
Zphone = y*(X - w*M)
transcript = context || X || Y   # always PC order, irrespective of send order
pairKey = H32(transcript || Z)
pcProof = Auth(pairKey, "PC confirms pairing", transcript)
phoneProof = Auth(pairKey, "Phone confirms pairing", transcript)
```

The phone sends its masked point first. The PC returns its point and PC confirmation; the phone checks the PC confirmation before normal completion and returns its confirmation. The PC verifies before persisting. The phone waits for a final ready message before persisting. The code and unmasked shared point never go on the wire. Invalid encoded points and identity shared results fail through libsodium's checked operations. Failure consumes the exchange object.

The PC's final ready acknowledgement and initial metadata are plaintext, not authenticated JSON. During an initial wrong-code flow, the phone also returns a proof to let the PC report the failed attempt. Review cancellation, partial storage, corrupted final acknowledgement, expiry and concurrent code replacement carefully. Passing the localhost happy path is insufficient.

```text
resumeTranscript = pcId[16] || phoneId[16] || pcNonce[32] || phoneNonce[32]
pcResumeProof = Auth(pairKey, "PC resumes", resumeTranscript)
phoneResumeProof = Auth(pairKey, "Phone resumes", resumeTranscript)
pcAudioTx = Auth(pairKey, "AudioWire v1 PC to phone", pcNonce || phoneNonce)
phoneAudioTx = Auth(pairKey, "AudioWire v1 phone to PC", pcNonce || phoneNonce)
controlTx = Auth(correspondingAudioTx, "AudioWire v1 control", empty)
```

Session nonces are independently regenerated on every connection; transmit/receive directions swap by role. PC and phone identities are included in resume proofs, while session-key derivation takes the pair key and nonces. Saved-key authentication is not a fresh Diffie-Hellman exchange. Compromise of the stored pair key plus a recorded resumed session therefore allows derivation of that session's keys: **no forward-secrecy claim is made for resumed sessions**.

Audio and control use separate SecureTransport objects/keys. Header bytes are `AWIR | version=1 | kind | counter_be64`; the 14-byte header is associated data. The 12-byte ChaCha20-Poly1305-IETF nonce is four zero bytes and the eight counter bytes. Counters start at zero; the sender refuses exhaustion. The 16-byte tag follows ciphertext. The receiver authenticates before updating a 64-packet replay window. The complete AOO datagram is encrypted before transmission. TCP frames have a four-byte big-endian length and an 8192-byte cap. Initial JSON and encrypted frames share this outer framing.

## Reviewer questions and release decisions

| ID | Review request | Acceptance evidence |
|---|---|---|
| Q1 | Assess deviations from RFC 9382: Ristretto/hash choices, one-byte lengths, omission of `w` from the final transcript, direct pair-key confirmation rather than separate RFC confirmation keys, and lack of a memory-hard password mapping. | Written cryptographic analysis or migration to a reviewed, specified PAKE; do not infer security from resemblance to SPAKE2. |
| Q2 | Verify unknown-key-share, identity/role/reflection binding, mask-point independence, canonical/identity rejection, and confirmation reuse boundaries. | Independent reproduction and adversarial vectors. |
| Q3 | Examine final plaintext ready/UDP/microphone metadata, nonce alteration, persistence ordering and retry/cancellation. | Defined authenticated-session completion and tests for altered/truncated/interrupted handshakes. |
| Q4 | Test code replacement, expiry during a stalled exchange, app restart, five aborted guesses, second-phone rejection, and denial of service. | Adversarial runtime tests and documented residual denial-of-service behavior. |
| Q5 | Assess resumption without forward secrecy, pair-key compromise, device identity collisions, directional/control KDF separation and replay/counter behavior. | Explicit approved threat-model limitations and key schedule review. |
| Q6 | Review memory lifetimes: temporary key copies, exception paths, queued code strings, retained UI text, protected-store erase behavior, and diagnostics. | Source review, no-secret diagnostic evidence, parser fuzzing and sanitizer results. |

## Public fixtures and reproduction

`tests/vectors/pairing-v1.txt` contains two fully public synthetic cases: leading-zero and all-nine codes, fixed role identities, scalar values 1 and 2, and fixed session nonces. The fixture executable separately computes the group equations and compares them to production Pairing; it also freezes confirmations, resume proofs, directional/control keys and counter-zero encrypted packets. It intentionally prints only counts and status.

```sh
cmake -S . -B build/review -DAUDIOWIRE_BUILD_APP=OFF
cmake --build build/review --config Release --parallel 4
ctest --test-dir build/review -C Release --output-on-failure
```

The deterministic RNG is linked only into the separate `protocol_vectors` test process. No production constructor accepts a caller-supplied scalar. Explicit fixture regeneration is `protocol_vectors --write-public-fixtures <path>`; compare the change and explain any protocol drift before committing it. These are candidate regression vectors sharing libsodium with the implementation, **not independent proof or certified standard vectors**. The reviewer should reproduce the file with another implementation.

## Review handoff record

Reviewer: pending. Reviewed commit: pending. Independent fixture reproduction: pending. Findings, severity, fixes and retest: pending. Release decision and dated signoff: pending. No review request has been sent to another person as part of preparing this packet.

References: [RFC 9382](https://www.rfc-editor.org/rfc/rfc9382.html), [libsodium Ristretto API](https://libsodium.gitbook.io/doc/advanced/point-arithmetic/ristretto). RFC key scheduling and transcript requirements are comparison material, not a claim that AudioWire implements that RFC.
