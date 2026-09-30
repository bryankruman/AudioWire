# 06 — Fork strategy: what to keep, cut and rewrite in SonoBus

## Why fork at all (rather than start clean on AOO)

SonoBus gives us, working today on both target platforms: JUCE audio device handling, the AOO
integration with Opus, auto jitter buffering, dynamic resampling, latency measurement, level
metering, a proven real-time-safe threading model, and iOS/Windows builds that Apple and users have
already accepted. Rewriting those is months. Deleting the collaboration features is days.

The cost is inheriting an **old AOO fork** (`essej/aoo` `sono` branch, which its own README says
is only for building SonoBus) and **JUCE 6** (fine: GPLv3, and JUCE 8 is AGPL which we want to
avoid). Plan to migrate to upstream AOO 2.x (BSD-3) in Phase 5 once the one-peer core is stable.

## Repository layout after Phase 0

```
AudioWire/
├── CMakeLists.txt                # single build; targets: audiowire_core, AudioWire (win/ios), tests
├── core/                         # C1 — platform-independent (from SonobusPluginProcessor, rewritten)
│   ├── Session.{h,cpp}           # one-peer state machine
│   ├── Transport.{h,cpp}         # AOO source/sink wrapper, presets
│   ├── SecureTransport.{h,cpp}   # ChaCha20-Poly1305 wrap (libsodium)
│   ├── Pairing.{h,cpp}           # SPAKE2, key storage interface
│   ├── Control.{h,cpp}           # control messages
│   ├── Discovery.h               # interface; impls per platform
│   └── LatencyProbe.{h,cpp}      # from LatencyMeasurer
├── app/                          # shared JUCE UI (from SonobusPluginEditor, rewritten)
│   ├── Main.cpp
│   ├── HomeView.{h,cpp}  PairView.{h,cpp}  SettingsView.{h,cpp}  DiagnosticsView.{h,cpp}
│   └── AwLookAndFeel.{h,cpp}     # from SonoLookAndFeel, restyled
├── platform/
│   ├── windows/   TrayApp, WinAudioBridge (WASAPI loopback + driver client), MdnsWindows, DpapiKeyStore
│   └── ios/       VoiceProcessingAudioIODevice.mm, NwBrowserDiscovery.mm, KeychainKeyStore.mm, AudioSessionManager.mm
├── driver/                       # C3 — audiowire.sys (SysVAD-derived), own solution, WDK
│   ├── audiowire.inf  AwRing.*  AwIoctl.*  AwSpeaker*.cpp  AwMic*.cpp
│   └── README.md                 # test-signing, build, install
├── installer/                    # Inno Setup / WiX script, firewall + pnputil steps
├── deps/                         # JUCE (essej fork), aoo (essej fork), opus prebuilt, libsodium, mdns.h
├── docs/                         # this plan → later user docs
└── .github/workflows/            # windows.yml (app+driver), ios.yml
```

## Keep / cut / rewrite — file by file

Source: SonoBus `Source/` listing as of 2026-09-29.

| SonoBus file(s) | Action | Notes |
|---|---|---|
| `SonobusPluginProcessor.{h,cpp}` (1 287 lines) | **Rewrite → `core/Session`, `core/Transport`** | Keep: AOO source/sink setup, codec format table, `AutoNetBufferMode` logic, `connectRemotePeer`, per-peer buffer time, dynamic resampling glue, level meter sources. Cut: server/group methods, 32-peer array (→ 1), 64 input channel groups (→ 1 in/1 out), reverb, compressor/expander/EQ, chat queue, soundboard hooks, plugin-parameter tree. |
| `SonobusPluginEditor.{h,cpp}`, `PeersContainerView`, `ConnectView`, `ChannelGroupsView`, `OptionsView`, `LatencyMatchView` | **Cut; write `app/*View`** | Nothing here matches the new UX. Read them for the JUCE plumbing patterns (timers, meter updates, callouts) then delete. |
| `ChatView`, `Soundboard*`, `SampleEditView`, `BeatToggleGrid`, `Metronome`, `RandomSentenceGenerator` | **Cut** | Collaboration/music features. |
| `EffectParams`, `faustCompressor.h`, `faustExpander.h`, `MVerb.h` | **Cut** | Effects. AEC/AGC/NS come from Apple on the phone. |
| `LatencyMeasurer`, `LocalLatencyMeasurer.h`, `JitterBufferMeter` | **Keep → `core/LatencyProbe`** | Powers the diagnostics screen and the G4 measurement. |
| `RunningCumulant`, `RunCumulantor` | **Keep** | Used by the jitter statistics. |
| `SonoLookAndFeel`, `Sono*Button`, `SonoCallOutBox` | **Keep, restyle → `AwLookAndFeel`** | Saves building JUCE widgets from scratch; restyle to the iOS-like spec. |
| `CrossPlatformUtils*.{cpp,mm}` | **Keep, extend** | Already hosts iOS/Windows specifics; the audio-session code goes next to it. |
| `AutoUpdater` | **Keep (Windows only)** | Point at GitHub Releases; disabled on iOS (App Store handles updates). |
| `VersionInfo`, `DebugLogC.h`, `SonoUtility.h`, `SonobusTypes.h` | **Keep, rename** | |
| `mobile/SonoBusMobile.jucer`, `mobile/Builds`, `mobile/JuceLibraryCode` | **Cut** after CMake iOS target works | One build definition. |
| `linux/`, `snap/`, `release/` scripts | **Park** | Not v1 platforms; keep in git history, remove from tree to avoid confusion. |
| `localization/` | **Keep the mechanism, reset the strings** | New copy per `05-ux-spec.md`; English only at launch, structure ready for translations. |
| `deps/aoo` (essej fork), `deps/windows`, `deps/mac` prebuilt opus | **Keep for v1** | Add iOS opus build (SonoBus's mobile project has one — locate and reuse). Migrate to upstream AOO in Phase 5. |
| `JUCE/` (essej `sono6good`) | **Keep** | Do not upgrade to JUCE 7/8 (JUCE 8 is AGPL; JUCE 7 GPLv3 is an option later if a feature needs it). |

## Naming and attribution

- Project/app name: **AudioWire**; bundle id `ai.rlab.audiowire`; Windows service/driver name
  `audiowire`; mDNS type `_audiowire._udp`.
- Keep SonoBus's copyright notices in every retained file, add AudioWire's, and add a prominent
  *"Based on SonoBus by Jesse Chappell (Sonosaurus LLC)"* to README and About. GPL requires the
  notices; courtesy requires the credit. Consider emailing the SonoBus author before publishing — a
  friendly heads-up avoids surprise and might yield advice on the iOS build.
- Do not reuse SonoBus icons, artwork or the name in the store listing.

## Licensing

Facts `[verified — read LICENSE / LICENSE_EXCEPTION / repo READMEs on 2026-09-29]`:

- SonoBus: **GPL-3.0-or-later** with a **§7 additional permission** allowing distribution through
  app stores "even if that store has restrictive terms", provided source stays available elsewhere.
- SonoBus's JUCE fork: JUCE 6, **GPLv3** (JUCE 8 is AGPLv3 — not used).
- AOO (upstream and the essej fork): **BSD-3-Clause** ("Standard Improved BSD").
- SysVAD sample: **MS-PL**; `Virtual-Audio-Driver`: MIT. Both GPL-compatible for our use (MS-PL code
  stays MS-PL inside the driver; the driver is a separate program from the GPL app anyway).
- libsodium: ISC. `mjansson/mdns`: public domain. Opus: BSD-3.

Plan: AudioWire is **GPL-3.0-or-later with the identical App Store additional permission** in
`LICENSE_EXCEPTION`. Under GPLv3 §7 the additional permission attached to SonoBus's code travels
with that code into the fork; our new code gets the same permission from us.

The one open question (`[assumed]` — not legal advice): **JUCE 6's GPLv3 has no App Store
exception.** SonoBus ships on the App Store regardless, as do other GPL JUCE apps, and the FSF's
position is that Apple's terms conflict with GPLv3. Practically the risk is low (only JUCE's
copyright holders could object, and they sell the alternative), but it is not zero. Options are in
`08-risks-and-decisions.md` **R6**.

## Phase 0 checklist (the actual fork)

1. `git clone --recursive https://github.com/sonosaurus/sonobus` → new repo `AudioWire` preserving history (so `git blame` and upstream cherry-picks keep working). Add `upstream` remote.
2. Build unmodified SonoBus on Windows (VS 2022, CMake) and iOS (Projucer for now) — proves the toolchain before touching anything.
3. Apply the cut list above in one commit series; rename targets, bundle id, product name.
4. Move `SonobusPluginProcessor` guts into `core/` behind `Session`/`Transport`; keep the app compiling with a placeholder UI ("Connect to host:port").
5. Add CMake iOS target; delete `mobile/`.
6. CI green on both platforms; `README.md` states "Phase 0 complete".
