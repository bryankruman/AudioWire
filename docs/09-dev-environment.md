# 09 — Development environment, accounts and certificates

> Original product plan. See [implementation status](IMPLEMENTATION.md) for verified behavior, deviations and open release gates.

Everything that must exist before or during Phase 0. Versions are targets to pin in CI once the
first build succeeds; adjust the table when verified.

## Machines

| Role | Machine | Notes |
|---|---|---|
| Windows app + driver dev | Bryan's Windows 11 desktop | Driver testing happens in a **VM**, never on the dev host: a BSOD in a virtual mic driver should not take the workstation down. |
| Driver test VM | Hyper-V Gen2, Windows 11 23H2+, 4 GB, test-signing on, Secure Boot **off** (test-signing requires it) | Snapshot before every driver install. Kernel debugging over network (`kdnet`) to the host. |
| iOS build | A Mac with Xcode (Apple Silicon preferred) | No Mac → a Mac mini M4 (~$600) or a cloud Mac (MacStadium / GitHub Actions `macos-latest` for CI; a physical Mac is still needed for device debugging). |
| iOS test devices | At least one iPhone on iOS 15 (minimum) and one on current iOS | Echo behaviour differs per model; test a speaker-phone-sized (Plus/Max) and a small one. |
| Second Windows PC or laptop | For the "two machines on one LAN" and ARM64 tests | Snapdragon X laptop optional for ARM64 verification. |

## Toolchain — Windows

| Tool | Version to pin | Purpose |
|---|---|---|
| Visual Studio 2022 Community | 17.x, "Desktop development with C++" + Spectre-mitigated libs (WDK needs them) | App and driver |
| Windows SDK | 10.0.26100 (matching WDK) | |
| **Windows Driver Kit (WDK)** | 10.0.26100.x **with WDK VS extension** | `audiowire.sys`. Verify `portcls.lib` is present for the target (Windows-driver-samples issue #1180). |
| CMake | ≥ 3.25 | Single build tree |
| Ninja | latest | Faster than MSBuild for the app |
| Inno Setup 6 (or WiX 4) | | Installer with `pnputil` driver step |
| `signtool` (in SDK) | | Signing installer, app, driver CAB |
| Wireshark | | Verifying discovery and that payloads are encrypted |
| Driver Verifier, `devcon`/`pnputil`, WinDbg Preview | | Driver bring-up |

## Toolchain — iOS

| Tool | Version | Purpose |
|---|---|---|
| Xcode | current stable (15/16 at writing); pin in CI | Build, sign, archive |
| CMake iOS toolchain | via `-G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0` | Replaces Projucer |
| Apple Developer Program (organisation) | | Provisioning, TestFlight, App Store |
| Opus for iOS | build from source (xcframework) or reuse SonoBus's `mobile/` prebuilt if present | |

## Dependencies to vendor under `deps/`

| Dependency | Source | Licence | Pin |
|---|---|---|---|
| JUCE 6 fork | `https://github.com/essej/JUCE` branch `sono6good` (already in SonoBus's `JUCE/`) | GPLv3 | commit used by SonoBus 1.7.3 |
| AOO fork | `https://github.com/essej/aoo` branch `sono` (already in `deps/aoo`) | BSD-3 | as in SonoBus |
| Opus | bundled with AOO deps / prebuilt per platform | BSD-3 | 1.4+ |
| libsodium | `https://github.com/jedisct1/libsodium` (stable) | ISC | 1.0.20 |
| mdns.h | `https://github.com/mjansson/mdns` | Public domain | latest |
| SysVAD | `https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad` | MS-PL | main @ P2 start |
| (Plan B) webrtc-apm | `https://github.com/qazwsxwtc/webrtc-apm` or `ewan-xu/AEC3` | BSD-3 | only if R3 triggers |

## Accounts and certificates (start in P0 — long lead times)

| Item | Cost | Lead time | Needed by |
|---|---|---|---|
| Apple Developer Program (org) | $99/yr | ~1 week (D-U-N-S check) | P1 TestFlight |
| **EV code-signing certificate** (Sectigo/DigiCert/GlobalSign; hardware token or cloud HSM) | ~$290–500/yr; max validity 459 days | 1–3 weeks identity verification | P2 week 7 (first attestation) |
| **Microsoft Partner Center — Hardware Developer Program** | Free | after EV cert exists | P2 week 7 |
| Microsoft Trusted Signing (optional, for installer/app only) | $9.99/mo | days | P4 |
| GitHub organisation + repo, Actions minutes | Free tier likely enough (macOS minutes are 10×) | immediate | P0 |
| Domain (e.g. `audiowire.app`) for docs and the `audiowire://` link | ~$20/yr | immediate | P4 |

## CI outline

- `windows.yml`: checkout with submodules → CMake configure (Ninja) → build `AudioWire.exe` → build driver with `msbuild driver/audiowire.sln` (WDK installed via the `Microsoft/setup-msbuild` + WDK NuGet packages) → package → upload artefacts. Signing only on tagged releases with secrets.
- `ios.yml`: macOS runner → CMake Xcode generate → `xcodebuild -scheme AudioWire -sdk iphonesimulator` for PRs; `-sdk iphoneos` archive on tags with fastlane `match` for certificates.
- `core-tests.yml`: cross-platform Catch2 unit tests for `core/` (pairing vectors, state machine, control message encode/decode) — no audio hardware needed.

## Local secrets handling

Follow the global rule: never commit or print certificates, provisioning profiles, EV token PINs,
or App Store Connect API keys. Store them in Bitwarden (rlab vault) and reference them by item name
in docs; CI gets them as encrypted secrets.
