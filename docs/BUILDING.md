# Building AudioWire

The project has one CMake tree for the portable core and the native applications. The kernel driver uses its own WDK project. Downloads, compiler output, generated files, and local toolchains belong under the ignored `build/` directory.

## Pinned dependencies

| Name | Source | Commit |
|---|---|---|
| JUCE 6 | essej/JUCE, sono6good | 9bf1237797e600378dd6f969bd443ab1bf3f704f |
| AOO | essej/aoo, sono | a16c3531d119370be2de21998458e0d17c4d346f |
| Opus 1.5.2 | xiph/opus | ddbe48383984d56acd9e1ab6a090c54ca6b735a6 |
| libsodium 1.0.20 | jedisct1/libsodium | 9511c982fb1d046470a8b42aa36556cdb7da15de |
| mdns | mjansson/mdns | a569c4759bd47e0f2a7bfc4d4c19620445782806 |
| QR generator | nayuki/QR-Code-generator | 3c6d0b3cefb4e049dc337e82237c9644399716a8 |
| Driver reference | VirtualDrivers/Virtual-Audio-Driver | bb34fba15faf569a6ae9bdea360bc1cf4821354e |

CMake downloads archives at these commits. An offline checkout can set `-DAUDIOWIRE_VENDOR_DIR=/absolute/path/to/vendor`; subdirectories must be named `JUCE`, `aoo`, `opus`, `libsodium`, `mdns`, and `qrcodegen`. Supplying an override is a developer action: CMake trusts that checkout rather than silently replacing it.

JUCE compatibility fixes are applied during configuration: include `<utility>`, make the Windows-version helper available to MinGW, and forward the AudioWire iOS URL scheme before JUCE's file-bookmark handler. The original copyright notices remain intact.

libsodium is compiled from its portable C sources. Optimized assembly is not enabled by this wrapper. Opus uses its own CMake build and CPU dispatch.

## Windows application

Use Visual Studio 2022, CMake, and Windows SDK. Run `scripts/build.ps1`. App output is `build/x64/AudioWire_artefacts/Release/AudioWire.exe`; the SetupAPI helper is `build/x64/Release/AudioWireDriverSetup.exe`.

A local build was also exercised with portable w64devkit GCC 16.2.0, using the MinGW Makefiles generator. No system compiler installation was changed. w64devkit's `mingw32-make.exe` forwarding launcher may fail when invoked recursively by CMake; use a copy of its actual `make.exe` under that name in the portable directory. The Visual Studio path does not need this workaround.

`AudioWire --network-test` uses in-memory key storage and a loopback connection. It never installs a driver or modifies firewall/default-device/autostart settings. CTest invokes this test automatically on Windows.

## iOS

Use the simulator commands in the README. The native shim is compiled as Objective-C++ with ARC; JUCE's own Objective-C sources use the framework's build flags. Device builds need an Apple development team, certificate and profile. Do not put signing material into source control.

The app requests local-network permission when discovery starts. Microphone permission is requested when microphone mode is enabled. The iOS plist declares the Bonjour service, background audio, microphone usage text and `audiowire://` URLs.

## Driver and installer

See `driver/README.md`. The app build does not compile or validate the kernel driver. `scripts/package.ps1` runs Inno Setup. Default packages omit the driver; `-WithDriver` includes an explicitly provided candidate. `-Release` additionally requires the driver and verifies the app and kernel signatures before packaging. Signing the completed installer remains a separate release operation.

No build or test script enables Windows Test Mode, installs a driver, changes the default sound devices, contacts a store, or publishes a release. Installer execution is the explicit system-changing step.

## Continuous integration

Core tests run on Windows, Linux and macOS. Windows app builds target x64 and ARM64; tests execute on x64. iOS builds target the unsigned simulator. Driver validation is a manually dispatched workflow on a provisioned WDK runner and does not install a driver on the runner. A physical driver-test VM and iPhone are separate validation environments.

## Safe developer checks and UI previews

`AudioWire --audio-test` opens existing Windows loopback capture and runs 100 audio callbacks. Microphone injection, mirroring, default-device switching and autostart changes are disabled in this mode. `AudioWire --render-ui <absolute-output-directory>` writes light/dark Windows connection/settings/diagnostics and narrow phone-layout PNGs without starting a network or audio session. For GUI executables in PowerShell, use `Start-Process -Wait -PassThru -WindowStyle Hidden` and check `ExitCode`.

## Corresponding source and notices

CMake copies dependency licences into `<build>/licenses`. Keep that directory, `LICENSE`, `LICENSE_EXCEPTION` and `THIRD_PARTY_NOTICES.md` with redistributed binaries. `python scripts/bundle-source.py --vendor <actual-vendor-checkout-directory>` creates `build/package/AudioWire-0.1.0-source.zip` (a CMake `_deps` directory is accepted too), including the exact patched dependency sources used for offline builds. Without `--vendor`, the archive includes project sources and the pinned download/build definitions only. Supply complete corresponding source with any GPL binary distribution.

[Implementation status](IMPLEMENTATION.md) identifies the actual local checks and remaining release gates. [Store release preparation](STORE_RELEASE.md) covers signing, licensing, privacy declarations and submission material.
