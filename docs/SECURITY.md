# AudioWire security and protocol

AudioWire accepts one PC and one phone per active connection. There are no accounts, public connection servers, relays, telemetry, or internet-discovery features.

## Pairing

A PC explicitly opens a two-minute pairing window with a cryptographically random six-digit code. The code is displayed only in the local UI and its QR. It is not put in advertisements, diagnostic text, or network messages.

Pairing uses a balanced SPAKE2-style exchange on libsodium's Ristretto255 prime-order group. Role-specific mask points are independent hash-to-group outputs from fixed protocol labels. The password scalar binds the code and both device identities. The derived pair key binds identities, both messages, and the shared secret. HMAC-SHA256 confirmation has distinct PC and phone labels.

Each online pairing exchange consumes one of five attempts, including an exchange that a client abandons after receiving the PC's confirmation. Only explicit user action opens a new window. Both parties verify confirmation before storing the key; the phone waits for the PC's final acknowledgement.

**This is an application-specific PAKE implementation, not a claim of standards conformance or an audited SPAKE2 library. Independent protocol review and test vectors are release gates.** The primitive operations are from libsodium; protocol composition still needs review.

## Returning connections

Each side supplies a fresh 32-byte nonce. Returning connections prove possession of the pair key with direction-specific HMACs over fixed-length identities and both nonces. Session transmit and receive keys are derived with separate domain labels. Control keys have an additional namespace, so delayed TCP control messages do not share the UDP replay window.

Pair keys are protected with per-user Windows DPAPI or the iOS Keychain, accessible after first unlock on that device. Settings JSON contains device names/addresses/IDs, never pair keys. Forgetting a device deletes the protected key and its remembered address.

## Packets

Every AOO datagram is encrypted in full using ChaCha20-Poly1305-IETF. The authenticated envelope is `AWIR | version | kind | 64-bit counter`, followed by ciphertext and a 16-byte tag. The counter is encoded in the last eight bytes of a 12-byte nonce. Separate directional keys prevent nonce collision and reflection. Keys change on every connection.

The receiver authenticates before advancing a 64-packet replay window. It permits reordering inside the window and rejects duplicates, old packets, invalid types, oversized packets and failed authentication. TCP carries length-framed, encrypted control messages. Initial pairing metadata is public; the code and pair key are never transmitted.

The entire AOO message is decrypted before AOO parses it. AOO never sees unauthenticated bytes. This intentionally differs from the original idea of leaving the AOO header visible.

## Kernel boundary

The microphone feed uses a private, ACL-protected control device with a unique class GUID, `FILE_DEVICE_SECURE_OPEN`, single-writer access, and buffered I/O. Requests carry a fixed version, bounded sample count, and PCM samples; no pointers or mappings cross the boundary. Unknown versions, lengths and commands fail.

The ring holds 200 ms of 48 kHz PCM16 mono. On overflow it discards the oldest frames to keep the microphone current; on underrun it outputs silence. Closing the app clears queued data. Capture conversion uses integer operations, including exact IEEE float bit generation. No kernel floating-point state is used.

Driver security guidance: [Microsoft's device security model](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/windows-security-model) and [driver security checklist](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/driver-security-checklist).

## Diagnostics and reporting

Diagnostics contain state, audio status, route, counts, selected quality and control-channel round-trip time. No audio recordings, keys or pairing codes are included. Error messages from malformed incoming handshakes are not logged. Report security issues privately to the repository owner rather than including credentials or recordings in public issues.
