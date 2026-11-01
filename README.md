# AudioWire

AudioWire turns an iPhone into a wireless speaker and microphone for a Windows PC on the same Wi-Fi network. The project uses the SonoBus JUCE 6 and AOO forks, Opus, and an encrypted one-to-one connection.

**Status: implemented pre-alpha, not a released v1.** The Windows app builds and its automated core and network tests run locally. iOS and kernel-driver source, build definitions, installer, and CI workflows are included. The iOS app and driver still need their platform builds and physical-device validation. There is no App Store release or Microsoft-signed driver in this repository.

## Use the Windows development build

Build from source with the instructions below, then open `AudioWire.exe`. Click **Pair a new phone** to display a six-digit code and a locally generated QR code. The code expires after two minutes and permits five attempts. The phone selects the PC on Wi-Fi or enters the displayed address and port.

A paired phone reconnects with its stored key. Pick **Speakerphone**, **Speaker only**, or **Mic only**. Volume and microphone mute are shared over the encrypted connection. Settings and pairing survive restarting the app.

Without the AudioWire driver, Windows speaker mode captures your existing PC speakers; those speakers also continue playing. A virtual microphone requires the driver. The app also recognizes an existing compatible audio connection as an optional development fallback. No third-party audio software is required by the intended release.

The **Use iPhone for all PC sound** toggle is explicit. Its default restoration setting returns sound to the previous devices on disconnect, provided the user has not selected another device in the meantime. Prior selections are journaled before a switch so normal startup can recover them after an app crash; this recovery path still needs VM validation.

## Build

Windows: Visual Studio 2022 with Desktop development with C++, CMake 3.25 or later. Dependencies are downloaded from pinned commits; no global package-manager installation is required.

```powershell
./scripts/build.ps1
```

Portable core tests, without JUCE or audio hardware:

```sh
cmake -S . -B build/core -DAUDIOWIRE_BUILD_APP=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --config Release --parallel
ctest --test-dir build/core -C Release --output-on-failure
```

Windows ARM64:

```powershell
./scripts/build.ps1 -Architecture ARM64
```

iOS, on a Mac with Xcode:

```sh
cmake -S . -B build/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DAUDIOWIRE_BUILD_TESTS=OFF \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build/ios --config Release --target AudioWire -- \
  -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO
```

For a real iPhone, generate with `iphoneos`, set your own Xcode development team, and provision `ai.rlab.audiowire`. The shared core contains no JUCE dependencies. See [build details](docs/BUILDING.md) and [driver instructions](driver/README.md).

## What's included

| Component | Implementation |
|---|---|
| Shared engine | AOO source/sink, stereo PC sound, mono phone microphone, Opus/PCM quality presets, dynamic resampling, adaptive audio reserve |
| Security | Ristretto255 SPAKE2-style pairing, mutual key confirmation, per-direction session keys, ChaCha20-Poly1305 packets, replay window |
| Windows | Tray application, standard PC sound capture, microphone driver feed, optional PC sound mirroring, DPAPI key protection, autostart |
| iOS | Native voice-processing audio unit, echo-convergence microphone gate, Network-framework discovery, Keychain storage, interruption/background handling |
| UI | Connection and pairing, manual address and QR links, modes, mute, volume, saved devices, settings, diagnostics |
| Driver | SysVAD-derived speaker and microphone, bounded buffered-I/O feed, silence on underrun, statistics, x64/ARM64 WDK project |
| Packaging | Inno Setup installer, SetupAPI device helper, local-subnet/private-network firewall rules, signature gates for release packaging |
| Validation | Core tests, TCP pairing/reconnect test, exhaustive PCM conversion test, app and iOS CI definitions |

The custom pairing protocol needs independent cryptographic review before public release. The diagnostics latency value measures control-channel round-trip time; it is not a mouth-to-ear measurement. Echo quality, Wi-Fi reconnection timing, and the 60 ms acoustic target require a real PC and iPhone.

## Documentation

[Current validation results](docs/VALIDATION_STATUS.md), [release checklist](docs/RELEASE_CHECKLIST.md), [device/VM acceptance](docs/DEVICE_VALIDATION.md), and [pairing review packet](docs/security/PAIRING_REVIEW.md) distinguish local passes from open platform/release gates.

[Implementation status](docs/IMPLEMENTATION.md) records what is implemented, what has been verified, deviations from the original plan, and release gates.
[Troubleshooting](docs/TROUBLESHOOTING.md), [security design](docs/SECURITY.md), [store release preparation](docs/STORE_RELEASE.md), and [contributing](CONTRIBUTING.md) explain operation and development.

The numbered documents in [docs/](docs/00-overview.md) are the original product plan. Their success criteria remain the release contract; source code or a simulator build alone does not satisfy the driver, device, signing, or store milestones.

## Licence and credit

AudioWire application and core: **GPL-3.0-or-later** with the [App Store additional permission](LICENSE_EXCEPTION). Driver changes are MIT; retained Microsoft code remains MS-PL. See [third-party notices](THIRD_PARTY_NOTICES.md).

Based on [SonoBus](https://github.com/sonosaurus/sonobus) by Jesse Chappell (Sonosaurus LLC), using [AOO](https://github.com/essej/aoo) by Christof Ressi, Winfried Ritsch and contributors, [Opus](https://opus-codec.org/), [libsodium](https://libsodium.org/), [mdns](https://github.com/mjansson/mdns), and [Nayuki's QR generator](https://github.com/nayuki/QR-Code-generator).
