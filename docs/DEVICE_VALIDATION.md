# AudioWire device and driver acceptance

Status: **not executed on a physical iPhone or driver VM**. This runbook describes the evidence required to close the release gates; a checkbox or source inspection is not a result. Record the tested commit, binary SHA-256, platform/SDK/device versions, operator, result, and evidence path for each row. Do not retain live pairing codes, keys, audio from other people, or signing credentials.

## iPhone and Windows test environment

Use a Mac with a current compatible Xcode installation and a provisioned iPhone, plus a Windows PC on the same network. The inventoried Mac mini was reachable on November 1, 2026 and reported macOS 12.7.6 with no active Xcode; it did not supply a usable build environment. The M1 MacBook Air is now selected for device development; its connection setup and the test iPhone/driver VM remain pending. Windows build tools can run locally; kernel runtime checks still use an isolated VM compatible with the host Windows edition.

Build the unsigned simulator first using `scripts/build-ios.sh`. For the physical device use `scripts/build-ios.sh device <APPLE_DEVELOPMENT_TEAM_ID>` on the Mac. The team ID is not a signing private key. Provision locally using the organisation's protected Apple credentials; never copy credentials into the repository. A simulator can establish compilation and UI startup, but cannot establish the real audio gates below.

| ID | Procedure | Pass criterion |
|---|---|---|
| IOS-01 | Fresh simulator and signed device builds; launch the exact tested build. | Builds succeed, app starts; capture Xcode/SDK/iPhone/iOS versions and artifact hashes. |
| IOS-02 | On a fresh install find PC via Bonjour, confirm the code, then repeat using manual address and a system-Camera QR link. Try a wrong code, five abandoned exchanges, expiry, and a replaced code. | Correct peer connects; invalid/stale requests fail without storing a pair or exposing the live code in diagnostics. |
| IOS-03 | Deny local-network and microphone permissions separately; then enable them in Settings. | Clear recovery; microphone denial permits speaker mode; capture never starts without permission. |
| IOS-04 | Exercise speaker, microphone and speakerphone; mute/unmute and volume from each UI; forget and pair again. | Each control has its stated effect in both directions; forgetting requires new authenticated pairing. |
| IOS-05 | Lock/background phone, foreground it, receive a call/interruption, change wired/Bluetooth routes and Low Power Mode. | Configured background policy holds; routes and audio recover without stale microphone playback or unsolicited mode changes. |
| AUDIO-01 | Place a consented Teams/Zoom call with iPhone at arm's length and normal speaker volume; repeat in Discord, OBS and Audacity as applicable. | Simultaneous speaker/mic works; far end reports no echo; record test conditions and measurements without recording private calls. |
| AUDIO-02 | Feed a controlled impulse/chirp and capture both original and phone-speaker arrival with one synchronized recorder; repeat at least 30 times per mode/network. | Document acoustic path and sound-travel correction; one-way mouth-to-ear meets the product's 60 ms target. Control-channel RTT is not this measurement. |
| AUDIO-03 | Disable test phone Wi-Fi for 30 seconds during a session, restore it, and measure audio return. Repeat PC sleep/resume and a long call. | Audio resumes within five seconds after network restoration; no sustained drift or stale buffers. |
| UX-01 | Nontechnical first setup, VoiceOver/Narrator, maximum Dynamic Type, dark/light, narrow screen and safe areas. | Under-five-minute initial call; operable labels/focus/controls without clipping. |

## Isolated Windows driver VM

Before changing a machine, designate an expendable VM and record its identity and a restorable snapshot. The Windows development host is not a test target. No boot-policy change, driver install, firewall change, or Driver Verifier operation is part of an app/core test.

Build x64 and ARM64 candidates with the supported MSVC/WDK combination and `scripts/build-driver.ps1 -Architecture x64` / `-Architecture ARM64`. The November 1, 2026 attempts both failed with `MSB8020` because `WindowsKernelModeDriver10.0` is missing. LLVM compilation/catalog logs from the original development checkout do not close this gate. Microsoft's [supported kit matrix](https://learn.microsoft.com/en-us/windows-hardware/drivers/other-wdk-downloads) lists the appropriate Visual Studio/SDK/WDK pairing; install or provision it before retrying.

Use a protected test certificate and an isolated, snapshotted VM configured for test-signed driver development. Record the precise VM commands and rollback snapshot before executing them. Inside that VM, the existing helpers provide these operations:

```powershell
# VM ONLY, after the build, protected test signing and snapshot are verified.
AudioWireDriverSetup.exe install C:\AudioWireTest\audiowire.inf
AudioWireDriverFeed.exe --probe
AudioWireDriverFeed.exe C:\AudioWireTest\mono-48000-pcm16.wav
# Enable Driver Verifier only for this driver in the disposable VM.
verifier /standard /driver audiowire.sys
# After the full validation interval and evidence collection:
verifier /reset
AudioWireDriverSetup.exe uninstall
```

The Verifier setup takes effect after a VM reboot. The 24-hour run begins only when the verified driver is loaded with the intended checks active. Before/after `verifier /querysettings` and `verifier /query` output belongs in the evidence; starting a process is not a completed 24-hour pass. Snapshot restore is the rollback if the VM cannot boot. These commands are a plan, not a record that they ran.

| ID | Procedure | Pass criterion |
|---|---|---|
| DRV-01 | Supported x64/ARM64 builds, INF/catalog checks, protected test signing. | Tool versions and hashes recorded; all checks pass; package contains the expected endpoints. |
| DRV-02 | Install in snapshotted VM, enumerate Sound devices, feed known mono audio, record the virtual microphone. | Speaker and microphone endpoints appear and carry expected samples; no-feed produces silence. |
| DRV-03 | Run `--probe`; exercise every advertised shared/exclusive format, duplicate writers, malformed lengths/versions/counts and unsupported requests. | Reject bad requests and extra writers; valid formats work; no kernel crash or out-of-bounds access. |
| DRV-04 | Underfeed/overflow, close/force-kill app, reopen; sleep/resume, RDP, fast user switching and HVCI. | No stale audio or leaked exclusive handle; timely recovery; documented HVCI result. |
| DRV-05 | Run targeted Driver Verifier for 24 hours with repeated audio and lifecycle tests. | No Verifier violations or crash; record start/end evidence and settings. |
| DRV-06 | Load ARM64 candidate on a physical ARM64 Windows PC. | Correct endpoints and audio; cross-compilation alone does not pass. |
| PKG-01 | Install/update/uninstall on clean VM, including alternate-admin elevation, running app, autostart and default-audio crash recovery. | Expected scoped firewall/device/startup entries; previous audio restored; uninstall removes owned entries and preserves unrelated devices/settings. |

## Evidence record

Every row is initially **pending**. Link actual results from `docs/VALIDATION_STATUS.md` and the release checklist. An unavailable platform is blocked, not passed. If a measured result fails, retain the failure and open a concrete issue before marking its retest complete.
