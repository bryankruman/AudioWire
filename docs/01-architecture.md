# 01 — Architecture

> Original product plan. See [implementation status](IMPLEMENTATION.md) for verified behavior, deviations and open release gates.

## One-paragraph version

Two apps share one C++ core. The **Windows app** is a tray application that installs a small
kernel driver exposing two virtual audio devices (`AudioWire Speaker`, `AudioWire Microphone`),
advertises itself on the LAN with mDNS, and moves audio between those devices and the network.
The **iOS app** discovers the PC, pairs once, and runs the phone's speaker and mic through Apple's
voice-processing audio unit (which gives echo cancellation for free). Audio travels over UDP using
**AOO** (Audio over OSC) with the **Opus** codec — the same engine SonoBus uses today — with the
SonoBus connection server, groups and mixer removed and replaced by direct one-to-one LAN links.

## Components

```
┌──────────────────────────── Windows PC ─────────────────────────────┐
│                                                                      │
│  Teams / Zoom / any app                                              │
│     │ plays to                          ▲ records from               │
│     ▼                                   │                            │
│  [AudioWire Speaker]              [AudioWire Microphone]             │
│   render endpoint                   capture endpoint                 │
│     │  (kernel: audiowire.sys, SysVAD/PortCls-WaveRT derived)        │
│     │ WASAPI loopback                   ▲ IOCTL + shared ring buffer │
│     ▼                                   │                            │
│  ┌──────────────── AudioWire.exe (tray app, JUCE) ────────────────┐  │
│  │  WinAudioBridge ──► AOO source (Opus enc) ──┐                  │  │
│  │  WinAudioBridge ◄── AOO sink   (Opus dec) ◄─┤  UDP :  dynamic  │  │
│  │  mDNS advertiser  (_audiowire._udp)          │                  │  │
│  │  Pairing service  (TCP, 6-digit code, keys)  │                  │  │
│  │  Settings, autostart, tray UI                 │                  │  │
│  └───────────────────────────────────────────────┼─────────────────┘  │
└──────────────────────────────────────────────────┼────────────────────┘
                                          Wi-Fi LAN │ (Opus over UDP, AOO framing)
┌──────────────────────────────────────────────────┼────────────────────┐
│  ┌──────────────── AudioWire.app (iOS, JUCE + Swift shim) ─────────┐  │
│  │  mDNS browser (NWBrowser)   Pairing client   Reconnect logic     │  │
│  │  AOO sink (Opus dec) ──► VoiceProcessingIO ──► iPhone speaker    │  │
│  │  AOO source (Opus enc) ◄── (AEC, AGC, NS)  ◄── iPhone mic        │  │
│  │  Big Connect button · Mute · Volume · Mode (Speaker/Mic/Both)    │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└───────────────────────────── iPhone ──────────────────────────────────┘
```

### C1 — `audiowire-core` (shared C++17 library)

Everything platform-independent, extracted from `SonobusAudioProcessor` and rewritten to a
one-peer model:

- **Session**: one remote peer, states `Idle → Discovering → Pairing → Connected → Reconnecting`.
- **Transport**: AOO source + sink pair per session, Opus codec, auto jitter buffer. Inherited.
- **Control channel**: AOO's reliable timestamped messages for mute, volume, mode changes, latency probes, rename.
- **Pairing/crypto**: 6-digit code → SPAKE2-style or simple HMAC-verified key exchange; persistent per-pair key so reconnects need no code. (Detail in `04-networking.md`.)
- **Discovery**: mDNS/DNS-SD via the public-domain `mjansson/mdns` header on Windows; on iOS the same header works but `NWBrowser` is preferred because iOS 14+ requires the local-network privacy prompt and multicast entitlement handling that Apple's own API handles best.

### C2 — `audiowire-windows` (tray app)

- JUCE standalone application, no window at startup, tray icon with **Connected to "Bryan's iPhone" · Speaker + Mic** status.
- `WinAudioBridge`: WASAPI loopback capture of the `AudioWire Speaker` endpoint → AOO source; AOO sink → shared-memory ring buffer → `audiowire.sys` → `AudioWire Microphone` capture endpoint.
- Optional convenience: "Also play PC audio on my real speakers" (mirrors to default device).
- Installer (Inno Setup or WiX) that installs app + driver (`pnputil /add-driver`), registers autostart, and opens the firewall for UDP/TCP on the app's ports.

### C3 — `audiowire-driver` (Windows kernel-mode driver)

SysVAD-derived PortCls/WaveRT driver (MIT/MS-PL sample code) exposing two endpoints and one private device interface. Full plan in `02-windows-virtual-audio.md`. The only kernel code in the project; kept as small as possible.

### C4 — `audiowire-ios` (iPhone app)

- JUCE application for the UI and the shared core, plus a **Swift/Obj-C++ shim** for the pieces JUCE does not do well on iOS: `AVAudioSession` configuration, `AUVoiceProcessingIO` (echo cancellation), `NWBrowser` discovery, background-audio mode, local-network permission prompt. Full plan in `03-ios-app.md`.

## Audio data flow, in order

**PC → phone (speaker direction)**
1. User (or the installer, with consent) sets `AudioWire Speaker` as the Windows default output, or a single app picks it.
2. Windows mixes into the virtual render endpoint at 48 kHz float.
3. `AudioWire.exe` captures it with WASAPI loopback (`AUDCLNT_STREAMFLAGS_LOOPBACK`) in 10 ms blocks.
4. AOO source encodes Opus (default 96 kb/s stereo, "Voice" preset 32 kb/s mono) and sends UDP to the phone.
5. AOO sink on the phone reassembles, resamples if needed, feeds `VoiceProcessingIO` output → speaker.

**Phone → PC (mic direction)**
1. `VoiceProcessingIO` delivers echo-cancelled, noise-suppressed 48 kHz mono mic frames.
2. AOO source encodes Opus 32–64 kb/s mono, sends UDP to the PC.
3. AOO sink on PC decodes; `WinAudioBridge` writes PCM into the shared ring buffer via the driver's device interface.
4. `audiowire.sys` serves those samples on the `AudioWire Microphone` WaveRT capture pin; Teams reads a normal microphone.

**Why echo cancellation lives on the phone:** an echo canceller needs the speaker signal and the mic signal from the *same* acoustic space, time-aligned. On the phone both are local and Apple already ships a tuned canceller for exactly this hardware. Doing it on the PC would mean cancelling with a reference that is one network round-trip out of alignment.

## Technology choices and why

| Choice | Decision | Why | Alternative considered |
|---|---|---|---|
| UI framework | **JUCE 6 (SonoBus's `essej/JUCE` fork, `sono6good` branch)** for both platforms in v1 | One UI codebase, already builds SonoBus on iOS and Windows, GPLv3. | Native SwiftUI + WinUI 3: nicer, but doubles UI work and is unnecessary for ~5 screens. Revisit after v1. JUCE 8 is AGPL — avoid. |
| Network audio | **AOO via the SonoBus fork (`essej/aoo`, `sono` branch)** | It is what `SonobusAudioProcessor` is written against; zero porting risk in Phase 1. | Upstream AOO 2.x (BSD-3, actively developed, C API). Better long-term; migrate in Phase 5 once the one-peer model is stable. |
| Codec | **Opus** (bundled in AOO) | 10 ms frames, PLC, handles Wi-Fi loss well. | PCM (already supported; offered as "Max quality" preset). |
| Discovery | **mDNS/DNS-SD** `_audiowire._udp.local` | Standard, works on iOS with `NWBrowser`, no server. | UDP broadcast (AudioRelay style) — blocked by many APs' client-isolation and iOS multicast rules; keep as fallback only. |
| Pairing | **6-digit code shown on PC, typed/scanned on phone; derives a persistent key** | One-time human check that you are talking to *your* PC; stops a housemate's phone from grabbing the mic. | No pairing (AudioRelay): simpler but anyone on the LAN can inject audio. |
| Encryption | **ChaCha20-Poly1305 over AOO payloads, key from pairing** | SonoBus sends cleartext; a mic stream on shared Wi-Fi should not. | DTLS — heavier, more code. libsodium is small and BSD-licensed. |
| Windows virtual device | **Own kernel driver (SysVAD-derived, PortCls/WaveRT)** | Only way to get a capture endpoint other apps can select. | ACX (Microsoft's newer framework, also Win10 2004+): cleaner but fewer open examples; keep as an option for the driver rewrite if SysVAD proves painful. |
| Echo cancellation | **Apple `AUVoiceProcessingIO` on iOS** | Hardware-tuned, free, and the phone is where the echo is. | WebRTC AEC3 (`webrtc-apm`, BSD) — keep as Plan B if Apple's AEC fights the 48 kHz Opus path. |
| Build system | **CMake** (SonoBus already uses it for desktop) + Xcode project generated by CMake for iOS | Replace the separate `mobile/SonoBusMobile.jucer` Projucer project so there is one build definition. | Keep Projucer for iOS — two build definitions drift. |
| CI | GitHub Actions: `windows-latest` (app + driver with WDK), `macos-latest` (iOS simulator + device archive) | Proves G6. | — |

## Ports and protocols

| Purpose | Protocol | Port | Notes |
|---|---|---|---|
| Discovery | mDNS/DNS-SD | UDP 5353 | Service `_audiowire._udp`, TXT record carries version, device name, pairing-required flag |
| Pairing + control bootstrap | TCP | dynamic, advertised in SRV record | Short-lived; exchanges pairing proof and session key |
| Audio + control | AOO over UDP | dynamic, one socket | Encrypted after pairing |

## Threading model (inherited from SonoBus, simplified)

- Audio thread: WASAPI/CoreAudio callback → AOO `process()`; no locks, no allocation (SonoBus already does this).
- Network thread: AOO send/receive loop, one per session.
- Discovery thread: mDNS query/response loop (Windows) — on iOS `NWBrowser` dispatches to its own queue.
- UI thread: JUCE message thread; reads atomics/lock-free FIFOs for meters and state.
