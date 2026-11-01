# 04 — Networking: discovery, pairing, transport, security

> Original product plan. See [implementation status](IMPLEMENTATION.md) for verified behavior, deviations and open release gates.

## What SonoBus does today (and what we remove)

SonoBus users connect to a **connection server** (default `aoo.sonobus.net`, or a self-hosted
`aooserver`, or the built-in one on port 10999), join a named **group**, and the server introduces
peers to each other, handling NAT traversal for internet use. AudioWire v1 is LAN-only and
one-to-one, so the whole server/group/public-groups layer goes: `connectToServer()`,
`joinServerGroup()`, `setWatchPublicGroups()`, the `ConnectView` UI and the recent-servers list.
What stays is the part underneath: `connectRemotePeer(host, port)` and the AOO source/sink pair.

## Discovery (find the PC)

- PC advertises **DNS-SD service `_audiowire._udp.local`** with instance name = PC name, SRV
  pointing at the pairing TCP port, TXT record:
  `v=1 id=<16-hex device id> name=<friendly> paired=<0|1> caps=spk,mic aoo=<udp port>`.
- Windows: `mjansson/mdns` (public domain, single header, no allocations) on its own thread. Windows
  10 has a system mDNS responder too, but driving it through the DNS-SD API adds Bonjour SDK
  dependencies; the header library avoids that.
- iOS: `NWBrowser` (see `03-ios-app.md` iOS-4).
- Fallback when multicast is blocked (guest networks, client isolation): the phone can type the PC's
  IP shown in the tray app, or scan the QR code which embeds `ip:port:code`.

## Pairing (prove it is *your* PC, once)

Goal: stop any device on the same Wi-Fi from listening to the PC or injecting audio into its mic,
without accounts or a server.

1. PC generates a random **6-digit code** (and a QR containing `audiowire://pair?ip=…&port=…&code=…`) and shows it in the tray window for 2 minutes.
2. Phone connects to the pairing TCP port and both sides run a **PAKE** (password-authenticated key exchange) on the code. Recommended primitive: **SPAKE2** (available in libsodium-based libraries; or CPace). A PAKE means an eavesdropper cannot brute-force the 6 digits offline, and a wrong guess costs a full round trip — the PC blocks after 5 failures.
3. Output: a 32-byte **pair key**, stored in Windows **DPAPI** (per-user) and iOS **Keychain**, plus each side's random device id.
4. Every later connection: phone sends `device id + HMAC(pair key, nonce)`; PC verifies, derives a fresh **session key** (HKDF over pair key + both nonces). No code, no prompt.
5. "Forget this PC / Forget this phone" deletes the pair key on that side.

Simpler alternative if PAKE turns into a time sink: TLS-PSK-style — hash the code with a salt the
PC sends, use as a one-time key to encrypt the pair key handover. Weaker (offline brute force of 6
digits is trivial if the handshake is recorded), acceptable for a LAN-only v1 if clearly logged as
debt. **Recommended: do SPAKE2 from the start**; it is ~200 lines with libsodium primitives.

## Transport (move the audio)

Inherited from SonoBus/AOO, configured for one peer:

| Parameter | v1 default | Exposed to user? |
|---|---|---|
| Codec | Opus | as one of three presets |
| Frame size | 10 ms (480 samples @ 48 kHz) | no |
| PC→phone bitrate | 96 kb/s stereo ("Balanced"); 160 kb/s ("High"); PCM 16-bit ("Max, wired-quality LAN only") | preset |
| Phone→PC bitrate | 48 kb/s mono voice ("Balanced"); 64 kb/s ("High") | preset |
| Jitter buffer | AOO auto mode (`AutoNetBufferMode::AutoFull` equivalent) starting at 20 ms | "Advanced → Latency vs stability" slider, default Auto |
| Packet loss handling | Opus PLC (built in) + AOO resend window for control messages | no |
| Clock drift | AOO dynamic resampling in the sink | no |

Control channel — AOO reliable messages (already used by SonoBus for chat and latency matching):
`mute`, `volume`, `mode` (speaker/mic/both), `quality`, `latency_probe`, `rename`, `battery` (phone → PC, for the tray tooltip), `bye`.

## Encryption

SonoBus sends audio unencrypted. On a shared network a live microphone should not be readable by
whoever runs Wireshark. Plan:

- Wrap each AOO UDP datagram payload in **ChaCha20-Poly1305** (libsodium `crypto_aead_chacha20poly1305_ietf`) with the session key and a 64-bit counter nonce. ~1 µs per 10 ms packet; negligible.
- The AOO header (sequence, timestamp) stays visible so the jitter buffer works without decrypting first; the audio payload and control messages are sealed.
- Replay protection: sliding window on the counter (AOO already tracks sequence numbers).
- Implementation point: a thin `SecureTransport` shim between AOO's socket send/receive and the OS socket — no changes inside AOO.

## Reconnection state machine

```
Idle ──user taps PC──► Pairing ──ok──► Connected
  ▲                                       │
  │                     link lost / app backgrounded on PC / sleep
  │                                       ▼
  └──user "Disconnect"──── Reconnecting (backoff 1,2,4,8,15 s; re-discover via mDNS; re-auth with pair key)
```

- Phone drives reconnection (it is the one that moves). The PC just keeps advertising and accepting.
- On PC resume from sleep: re-register mDNS, reopen sockets, keep the pair.
- If the PC's IP changed (DHCP), mDNS finds it again; the device id, not the IP, identifies the pair.

## Firewall and network requirements

- Inbound on PC: UDP 5353 (mDNS), TCP pairing port, UDP AOO port — all opened by the installer for
  `AudioWire.exe` on Private networks. Document that "Public" network profile blocks discovery.
- Wi-Fi tips shown in the app's help: 5 GHz preferred; disable "AP isolation"; phone and PC on the
  same SSID/VLAN; mesh systems with client steering can cause 1–2 s dropouts on band switch.

## Later (not v1)

- **Internet mode**: bring back an optional relay/introducer (upstream AOO's `aooserver` is BSD
  and actively maintained) for using the phone as a PC speaker away from home. Security model must
  be rethought (pair key still works; server only introduces).
- **USB for iPhone**: needs Apple Mobile Device Support on the PC and a usbmuxd-style TCP tunnel
  (like `peertalk`). Considerable effort; only worth it if Wi-Fi latency proves inadequate.
