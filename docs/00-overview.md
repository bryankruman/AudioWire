# 00 — Overview, goals and success criteria

## The problem

Bryan wants to use an iPhone as a wireless speaker + microphone for a Windows PC. Research on
2026-09-29 found:

- **SonoBus** (GPLv3, on the App Store, Windows build) can move audio both ways, but it is built
  for remote music jamming: you join a "group" on a public connection server, pick codecs and
  jitter buffers per peer, and on Windows you still need a third-party virtual audio cable for
  other apps to see the audio. It has no echo cancellation.
- **AudioRelay** does exactly the right thing (own virtual mic driver, one-click "select your PC",
  Opus, low latency) but is closed source, paid, and its iPhone app is stuck in an invite-only
  TestFlight beta as of March 2026.
- Everything else on the App Store does one direction only.

AudioWire is "AudioRelay, but open source, starting from SonoBus's proven audio engine".

## Goals (v1)

| ID | Goal | How we will know it is met |
|---|---|---|
| G1 | **Three-tap setup.** Install PC app → open iPhone app → tap the PC's name → confirm code. Done. | A first-time user with no audio knowledge goes from App Store to a working Teams call in under 5 minutes, without reading docs. |
| G2 | **Built-in virtual devices on Windows.** `AudioWire Speaker` and `AudioWire Microphone` appear in Sound settings after installing the PC app. | No mention of VB-Cable, Voicemeeter or "routing" anywhere in the UI or docs. |
| G3 | **Speakerphone quality.** Simultaneous play and record with echo cancellation on the phone. | Far end on a Teams/Zoom test call reports no echo with the iPhone at arm's length, speaker at normal volume. |
| G4 | **Low latency on LAN.** | Mouth-to-ear ≤ 60 ms one way on a normal home Wi-Fi network (measured with the built-in latency test). AudioRelay's marketing figure is "low latency"; SonoBus achieves ~20–40 ms on LAN with Opus at 10 ms frames. |
| G5 | **Reconnects itself.** Phone locks, Wi-Fi drops, PC reboots — the pair comes back without user action. | Kill Wi-Fi for 30 s during a call; audio resumes within 5 s of it returning. |
| G6 | **Open source, buildable by others.** GPLv3, public GitHub, CI builds for both platforms. | A fresh clone builds on a clean Windows 11 + Xcode machine following `docs/09-dev-environment.md`. |
| G7 | **Ships in the stores that matter.** iOS App Store; Windows via signed installer from GitHub Releases (and optionally winget). | App Store listing live; Windows installer installs the driver with no "unsigned driver" warning. |

## Non-goals for v1

Explicitly out, so the scope stays small enough to finish:

- **macOS / Linux PC side.** SonoBus already builds there, so the *app* will probably still compile, but virtual audio devices on those platforms (BlackHole / PipeWire) are separate work. Phase 5+.
- **Android phone side.** JUCE can target Android, but audio-session and echo-cancellation code is platform specific. Phase 5+.
- **USB connection for iPhone.** Requires Apple Mobile Device Support (iTunes drivers) plus a usbmuxd-style channel on Windows. Big effort for marginal latency gain on a good LAN. Phase 5+ at earliest.
- **Internet (non-LAN) use.** SonoBus's connection-server/NAT-traversal code is removed in v1. Same-network only. Can return later as an optional relay.
- **Multi-room / multiple phones per PC.** One PC ↔ one phone. AOO supports more; the UI will not expose it in v1.
- **Music-collaboration features.** Soundboard, chat, metronome, reverb, compressor/expander, per-peer mixing, recording, VDO.Ninja — all deleted from the fork.
- **Per-application audio capture on Windows.** (Windows supports it via `AUDCLNT_STREAMOPTIONS` process-loopback on Win10 2004+; a nice Phase 4/5 feature, not v1.)

## AudioRelay feature parity checklist

What AudioRelay offers (from its site, Play Store listing and forum — `[assumed]`, its full docs were not fetchable) and where AudioWire lands on each:

| AudioRelay feature | AudioWire v1 | Notes |
|---|---|---|
| PC audio → phone (player mode) | **Yes** | via `AudioWire Speaker` virtual output + WASAPI loopback |
| Phone mic → PC (mic mode) | **Yes** | via `AudioWire Microphone` virtual input driver |
| Both at once | **Yes** | this is the point of the project; AudioRelay does it too |
| Echo cancellation | **Yes (better)** | Apple VoiceProcessingIO on the phone. AudioRelay relies on the OS / far-end app |
| Opus compression, quality presets | **Yes** | inherited from SonoBus/AOO; 3 presets instead of a codec matrix |
| Adjustable buffer / latency | **Yes, automatic** | AOO auto jitter buffer; an "Advanced" slider only |
| Auto-discover PC on the app | **Yes** | mDNS/Bonjour instead of AudioRelay's UDP broadcast |
| Remote volume control from PC | **Phase 4** | control messages already exist in AOO |
| Rename devices | **Yes** | |
| USB mode | **No (v1)** | see non-goals |
| Android, macOS, Linux | **No (v1)** | see non-goals |
| Listen to PC + phone audio at once (mix) | **Yes** | natural on iOS when using `.mixWithOthers`; a toggle |
| Free / paid tiers | **Free, GPL** | |

## Success criteria for "v1.0 released"

1. G1–G7 all met and demonstrated in a recorded 3-minute walkthrough.
2. App Store review passed; Windows installer attestation-signed.
3. `README.md` rewritten from "planning" to user documentation.
4. At least one external contributor has built it from source (proves G6).
