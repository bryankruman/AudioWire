# 07 — Roadmap and milestones

Effort figures assume one developer working mostly full-time with AI assistance, with C++/JUCE
comfort and **no** prior Windows kernel experience. Calendar time for certificates and store review
is listed separately because it runs in parallel and cannot be compressed.

| Phase | Name | Effort | Exit criterion (demonstrable) |
|---|---|---|---|
| P0 | Fork and strip | 1–2 wks | Trimmed fork builds on Windows + iOS from one CMake tree; CI green; placeholder UI connects to `host:port` and passes audio both ways using SonoBus's existing engine |
| P1 | Walking skeleton | 2–3 wks | iPhone discovers PC via mDNS, pairs with a code, streams both ways; Windows side uses WASAPI loopback of the default output for speaker direction and VB-Cable (if present) for mic direction as a *temporary* stand-in; new Home/Pair screens on both |
| P2 | Windows driver | 4–8 wks | `audiowire.sys` test-signed: `AudioWire Speaker` + `AudioWire Microphone` visible, Teams call works with both selected; format matrix passes; Driver Verifier clean 24 h |
| P3 | Speakerphone quality | 2–3 wks | iOS `VoiceProcessingIO` device in; far end hears no echo at arm's length; auto quality/latency presets; reconnection state machine passes the Wi-Fi-drop test (G5) |
| P4 | Polish and release | 2–4 wks | Signed installer with attestation-signed driver; App Store listing live; user docs; diagnostics/copy-logs; remote volume; "use for all PC sound" flow |
| P5 | Beyond v1 | — | Upstream AOO 2.x migration; macOS/Linux PC side; Android; per-app capture; internet relay; USB |

Total to v1.0: roughly **11–20 weeks of work**, **4–6 months calendar** including the EV
certificate and store review wait times. The spread is almost entirely P2 (the driver).

## Parallel tracks to start in P0 (long lead times)

| Track | Why start now | Owner |
|---|---|---|
| **EV code-signing certificate + Partner Center (Hardware Program) registration** | Identity verification takes 1–3 weeks; you cannot ship the driver without it. Needs an organisation (see D2). | Bryan |
| **Apple Developer Program** enrolment for the organisation | Needed for TestFlight in P1/P3; org enrolment can take a week (D-U-N-S). | Bryan |
| **Courtesy email to the SonoBus author** | Goodwill; may save days on the iOS build. | Bryan |
| **Name check**: "AudioWire" on App Store, GitHub, domains, trademark search | Renaming later is expensive. | Bryan |

## Phase detail

### P0 — Fork and strip (1–2 weeks)

Deliverables: repo with history, cut list applied (`06-fork-strategy.md`), `core/` extraction
compiling, CMake iOS target, CI. **Risk to watch:** first iOS build of the JUCE 6 fork on current
Xcode (deprecations, bitcode, minimum iOS 15). Budget two days for it.

### P1 — Walking skeleton (2–3 weeks)

- `core/Discovery` + Windows mDNS advertiser; iOS `NWBrowser` shim; Info.plist entitlements.
- `core/Pairing` (SPAKE2 via libsodium) + DPAPI/Keychain stores; pairing screens.
- `SecureTransport` wrap.
- Windows `WinAudioBridge` v0: loopback of default output → AOO; AOO → any selectable render device (VB-Cable if present). This gives a usable *speaker-mode* product for early testers while P2 runs.
- Home screen with mode selector, mute, volume, meters; tray icon with states.
- TestFlight internal build; Windows zip build on GitHub Releases marked "pre-alpha, no driver yet".

### P2 — Windows driver (4–8 weeks) — critical path

Week-by-week shape:
1. WDK toolchain, build unmodified SysVAD, install test-signed on a VM (Hyper-V Win11 with test-signing), hear the sample's tone. *If this week slips, everything slips.*
2. Trim to two endpoints; INF; friendly names; format lists.
3. Ring buffer + IOCTL interface; user-mode test tool that plays a WAV into the mic endpoint.
4. Wire `WinAudioBridge` to the driver; clocking/drift tuning with AOO resampler; underrun handling.
5–6. Format matrix, Verifier, sleep/resume, RDP, ARM64 build.
7–8. Buffer: installer integration (`pnputil`), uninstall, fallback detection, first attestation submission (requires the EV cert to have arrived).

### P3 — Speakerphone quality (2–3 weeks)

- `VoiceProcessingAudioIODevice` on iOS; 500 ms post-start mic gate; RemoteIO for single-direction modes.
- Presets (Balanced/High/Max) mapped to Opus settings both directions; low-power auto-downgrade.
- Reconnect state machine with backoff; PC sleep/resume; iOS interruption handling.
- Latency measurement in diagnostics; verify G4 ≤ 60 ms one way on a real home network.
- Plan B decision point: if Apple AEC is unsatisfactory after one week of tuning, switch to WebRTC AEC3.

### P4 — Polish and release (2–4 weeks)

- Installer (Inno Setup), signed; firewall rules; autostart; "Use iPhone for all PC sound" with auto-revert.
- Remote volume, battery in tray, rename devices.
- Accessibility pass; dark mode; copy review against `05-ux-spec.md` word list.
- Docs: user README, troubleshooting (Wi-Fi isolation, permissions, test-signing), CONTRIBUTING, build docs.
- App Store submission with review notes and demo video; public TestFlight link; GitHub Release v1.0.0; optional `winget` manifest.

### P5 — Beyond v1 (unscheduled, in likely order)

1. Migrate to upstream AOO 2.x (BSD) — removes the "do not use this fork" dependency.
2. macOS PC side (BlackHole-style virtual device or Core Audio HAL plugin — separate driver work).
3. Android phone side (Oboe/AAudio + Android AEC).
4. Per-application capture on Windows (process loopback API).
5. Internet mode via optional relay (upstream `aooserver`).
6. Multiple phones (e.g. one as speaker in another room).
7. USB for iPhone (usbmuxd-style tunnel) — only if latency demands it.
