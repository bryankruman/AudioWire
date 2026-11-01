# 03 — iOS app

> Original product plan. See [implementation status](IMPLEMENTATION.md) for verified behavior, deviations and open release gates.

## Starting point in SonoBus

- `mobile/SonoBusMobile.jucer` is a Projucer project (JUCE 6 fork) that builds the App Store
  SonoBus for iOS, including an AUv3 extension. The main `CMakeLists.txt` has no iOS target.
- SonoBus's iOS audio goes through JUCE's `iOSAudioIODevice`, which uses the **RemoteIO** audio
  unit — no echo cancellation. SonoBus's user guide tells people to wear headphones for this reason.
- There are no written iOS build instructions upstream (GitHub issue #119 is open and unanswered).
  Expect a day or two to get the first build going.

## Plan

### iOS-1 — Build definition

Replace the Projucer project with an iOS target in CMake (JUCE 6's CMake API supports
`CMAKE_SYSTEM_NAME=iOS`; `juce_add_gui_app(... )` with bundle ID `ai.rlab.audiowire`). Drop the
AUv3 extension entirely — AudioWire is not an audio-unit plugin. One build definition for both
platforms is a Phase 0 exit criterion.

### iOS-2 — Audio session and echo cancellation

Apple's stack gives us speakerphone behaviour for free if we use it correctly:

| Setting | Value | Why |
|---|---|---|
| `AVAudioSession.category` | `.playAndRecord` | Simultaneous mic + speaker |
| `AVAudioSession.mode` | **`.voiceChat`** | Enables Apple's AEC/AGC/NS tuning for this device model and routes to the loudspeaker with `.defaultToSpeaker` |
| Options | `.defaultToSpeaker`, `.allowBluetooth`, `.allowBluetoothA2DP`, `.mixWithOthers` (toggle) | Loudspeaker by default; AirPods/car kits keep working; user can mix with phone music |
| Preferred sample rate | 48 000 Hz | Matches Opus and the Windows side; avoids a resampler |
| Preferred IO buffer | 10 ms (0.01 s) | Matches AOO/Opus frame size |
| Audio unit | **`kAudioUnitSubType_VoiceProcessingIO`** (AUVoiceIO), not RemoteIO | This is the unit that actually performs echo cancellation. |

Implementation: a new `VoiceProcessingAudioIODevice` (Obj-C++), a JUCE `AudioIODevice` subclass
modelled on JUCE's own `juce_ios_Audio.cpp` but hosting AUVoiceIO. Known gotchas from Apple's
WWDC 2019/2023 voice-processing sessions and field reports:

- Enable voice processing **before** starting the unit; attach the playback path first.
- The first ~300 ms after playback starts may leak echo before the canceller converges; the app
  gates the outgoing mic for 500 ms after the speaker stream (re)starts.
- `AUVoiceIO` may internally run at 24/16 kHz on older devices and up-sample; still request 48 kHz.
- When the user picks **Mic only** or **Speaker only**, switch to plain RemoteIO to skip the
  processing (lower latency, no AGC colouring).
- Plan B if Apple's AEC interacts badly with our stream: WebRTC AEC3 via `webrtc-apm`
  (BSD, CMake, no dependencies) running on the phone with the decoded speaker feed as reference.

### iOS-3 — Background execution and lifecycle

- `UIBackgroundModes: audio` — the app keeps streaming with the screen locked or another app in
  front, as long as the audio unit is running. Both directions count as "playing audio".
- Handle `AVAudioSession.interruptionNotification` (phone call, Siri): pause, then resume on
  `.ended` with `.shouldResume`; tell the PC via the control channel so its mic goes silent rather
  than emitting garbage.
- Handle route changes (headphones plugged in / AirPods connect): keep streaming; update the
  "playing through" label.
- On network loss: keep the audio unit alive, play silence, show *Reconnecting…*, retry with backoff
  (1, 2, 4, 8 s, cap 15 s). Do not tear down the session; that avoids the 300 ms AEC re-convergence.
- Low-power mode / thermal: drop to the "Voice" quality preset automatically and say so.

### iOS-4 — Discovery, permissions and pairing

- **Local Network permission** (iOS 14+): first `NWBrowser` use triggers the system prompt.
  `Info.plist` needs `NSLocalNetworkUsageDescription` ("AudioWire finds your PC on this Wi-Fi") and
  `NSBonjourServices` listing `_audiowire._udp`. Missing either silently breaks discovery — this is
  a top cause of "it doesn't find my PC" tickets in every such app.
- **Microphone permission**: `NSMicrophoneUsageDescription`. Ask only when the user first enables
  Mic or Both, not at launch.
- Discovery via `NWBrowser(for: .bonjour(type: "_audiowire._udp", domain: nil))`. On resolve,
  hand host/port to the shared core.
- **Camera permission** only if the user taps "Scan QR instead" (optional path).
- Pairing UI: type the 6 digits shown on the PC, or scan the PC's QR. Store the derived key in the
  **Keychain** (`kSecAttrAccessibleAfterFirstUnlock` so reconnect works on a locked phone).

### iOS-5 — UI (JUCE) 

Screens are specified in `05-ux-spec.md`. JUCE-specific notes:

- Use JUCE's `Component` with a hand-rolled look-and-feel matching iOS conventions (SF-like
  typeface via system font, large touch targets, dark/light following `UIUserInterfaceStyle`).
- Safe-area insets: JUCE 6 exposes `Desktop::getDisplays().getPrimaryDisplay()->safeAreaInsets`.
- Haptics and the iOS share sheet via a tiny Obj-C shim; do not fight JUCE for native widgets.
- Dynamic Type / VoiceOver: JUCE 6 accessibility is partial; label every control with
  `setTitle`/`setDescription` and test with VoiceOver on — the Raywonder SonoBus fork shows what
  screen-reader users needed.

### iOS-6 — App Store

- Bundle `ai.rlab.audiowire`, Apple Developer Program ($99/yr) under the same organisation as the
  Windows EV cert (see `08-risks-and-decisions.md` D2).
- Review notes must explain: what the app talks to (the user's own PC on the LAN), why it needs
  local network + microphone, and that it has no account/server. Include a demo video — reviewers
  cannot install our Windows side.
- Privacy nutrition label: **no data collected**. Add an in-app "Diagnostics → copy logs" instead
  of any telemetry.
- Licence: GPLv3 + App Store additional permission, mirroring SonoBus's `LICENSE_EXCEPTION`.
  Source link in the App Store description and in-app About screen.
- TestFlight first: public link, so people currently stuck on AudioRelay's full TestFlight beta
  have somewhere to go.

## Effort estimate

- First successful iOS build of the trimmed fork with CMake: 2–4 days.
- `VoiceProcessingAudioIODevice` + session handling + lifecycle: 1–2 weeks.
- Discovery/pairing shim + Keychain: 3–5 days.
- UI: 1–2 weeks (shared with Windows for the connection screens).
- App Store submission and one review round-trip: 1 week calendar.
