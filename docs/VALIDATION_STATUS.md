# AudioWire validation status — November 1, 2026

Overall status: **partial**. Source preservation and pairing-review preparation are complete locally. Native Windows x64 and local Ubuntu WSL portable-core validation pass. Hosted Windows x64/ARM64 builds, the iOS simulator build, and three-platform core CI now pass. Clean native x64/ARM64 driver builds and INF/catalog checks also pass. iPhone acceptance, driver runtime, ARM64 hardware acceptance and signed distribution remain open.

## Hosted CI after branch publication

**All six hosted jobs passed at source revision `a38b556`.** The reviewed branch and fixes are public. The Windows x64 tests ran; Windows ARM64 and iOS simulator compilation do not establish physical hardware behavior.

| Check | Actual hosted evidence |
|---|---|
| Three-platform portable core | [Successful run 41351298121](https://github.com/bryankruman/AudioWire/actions/runs/41351298121): Windows 2022, Ubuntu 24.04 and macOS 15 all passed. |
| Windows x64 and ARM64 app | [Successful run 41351298096](https://github.com/bryankruman/AudioWire/actions/runs/41351298096): build, x64 tests, corresponding-source/licence bundling and artifact upload passed for both architectures. |
| iOS simulator | [Successful run 41351298118](https://github.com/bryankruman/AudioWire/actions/runs/41351298118): configuration, simulator build and artifact upload passed. Downloaded bundle inspection confirmed ARM64 Mach-O with the iOS Simulator platform marker, bundle ID `ai.rlab.audiowire`, and minimum iOS 15.0. No physical iPhone execution was claimed. |

The first macOS jobs were cancelled by GitHub's scheduled macOS 14 image brownout. Moving to macOS 15 resolved that runner issue. The actual platform builds then exposed old-JUCE assumptions: Windows ARM64 was classified as Intel and attempted x86 intrinsics; the macOS juceaide build helper used a removed window-snapshot API; and JUCE's iOS asset-catalog directory conflicted with CMake's target installation rule. Narrow compatibility patches fixed the ARM64 build and host helper. Xcode now owns iOS bundle packaging/installation instead of the CMake desktop install rule. All target jobs subsequently passed.

The pinned Windows ARM64 port reports unavailable CPU clock metadata as unknown and disables x86 capability probing on ARM. The macOS snapshot fallback is used only by the headless host build helper; AudioWire does not ship a macOS GUI application. Audio transport and pairing protocol code were not changed by these compatibility fixes.

The M1 MacBook Air has been selected for physical iOS development. Connection work is held at the user's request; protected local Apple provisioning is pending; the inventoried 2014 Mac mini was freshly checked as macOS 12.7.6 with no Xcode. The explicitly approved Windows developer-tool installation completed successfully: VS WDK/ARM64/Spectre components, SDK and WDK installers each exited 0 without requesting a reboot. No SSH access change, host driver load or VM provisioning occurred.

Latest hosted artifact digests and the simulator inspection are retained under ignored `build/validation/hosted-ci-evidence.json` and `hosted-ios-artifact.json`. Earlier local package/archive hashes below identify historical development artifacts, not replacements for the latest hosted binaries.

## Revisions and evidence scope

The original development checkout at `C:\Users\Bryan\Projects\AudioWire` was copied into the task worktree and all 109 inventoried source files were SHA-256 compared before changes. Commit `305fe54` preserves that source. Commit `fcc8ec6` contains the tested compiler/security/build changes and public protocol fixtures. Documentation-only follow-up changes do not constitute a new app or driver validation.

Evidence is under ignored `build/validation/` in the task worktree. Logs are local artifacts, not hosted CI results. At the initial checkpoint no implementation had been pushed. The branch publication and actual CI results are now recorded above; no release was published. The original checkout's uncommitted work remains intact.

## Executed checks

| Check | Result | Evidence |
|---|---|---|
| Source preservation | 109 source files copied and hash-compared; baseline commit created; secret filename/token-pattern scan had no findings. | `source-snapshot.json`; commit `305fe54` |
| Fresh dependency configuration | Clean MSVC x64 tree downloaded pinned dependencies with TLS verification enabled. Initial sandbox attempt failed in Windows TLS setup; scoped unsandboxed retry succeeded. | `msvc-x64-configure.log`, `msvc-x64-configure-retry.log` |
| Native Windows app | Visual Studio 2022 MSVC 19.42 / SDK 10.0.26100.0 / CMake 4.4, Release x64: app, helper, feed tool and tests built. | `msvc-final-build.log` |
| Automated suites | **4/4 passed**: core, protocol_vectors, app_network, driver_abi. Includes encrypted bidirectional localhost Opus, wrong/abandoned/stale/expired pairing, stored-key reconnect, mute and 65,536 sample conversions. | `msvc-final-tests.log` |
| Source-archive rebuild, Windows | Extracted the verified archive into a new tree; configured with bundled dependencies, built with native MSVC, and passed **3/3** core/vector/driver-interface suites. No dependency download was needed. | `offline-core-configure.log`, `offline-core-build.log`, `offline-core-tests.log` |
| Source-archive rebuild, Linux | Existing Ubuntu 24.04 WSL with GNU 13.3.0/CMake/Ninja built the same extracted portable source and passed **2/2** core/vector suites. This is local Linux evidence, not hosted CI or a Linux application. | `linux-configure.log`, `linux-core-build.log`, `linux-core-tests.log` |
| Candidate protocol vectors | **50 fields passed**, covering two public synthetic exchanges, direct equations, confirmations, resume proofs, directional/control keys and packet envelopes. This is not an independent cryptographic review. | `tests/vectors/pairing-v1.txt`; `protocol_vectors` executable |
| Windows audio smoke | Existing WASAPI loopback opened; 10 ms callbacks ran; microphone injection disabled. | `msvc-audio.stdout.log`, `msvc-audio.stderr.log` |
| Workflow static analysis | actionlint **1.7.12 passed** on all four workflows after declaring the custom WDK label. Release archive checksum was verified before running the tool. Shellcheck and pyflakes were disabled for this lint invocation; this is not workflow execution. | `actionlint.log`; `.github/actionlint.yaml` |
| Developer installer | Inno Setup compiled an unsigned, driver-free package. Extraction matched the app/helper hashes and included 22 dependency-notice files plus project licence documents. The installer was not run. | `installer-build.log`, `installer-extract.log`, `installer/` |
| Script syntax | PowerShell build scripts parsed; `bash -n scripts/build-ios.sh` and Python source parsing passed. No Mac build was implied. | Source scripts and local parse commands |
| Initial GitHub inspection | Before publication the public repository had no workflow runs and **0** registered self-hosted runners. Hosted CI results now appear above; no WDK runner was provisioned. | Current-session `gh repo view`, `gh run list`, `gh api .../actions/runners` |
| Local driver tools | Approved VS component modification, SDK and WDK installation all exited 0; x64/ARM64 toolsets and PortCls libraries present. | `wdk-install/install-result.json`, `wdk-install/approved-install.log` |
| Native driver builds | Clean x64 and ARM64 Release builds with VS 2026/MSVC 19.50.35729 and SDK/WDK 10.0.28000.0: **zero warnings/errors**. | `wdk-native-x64.log`, `wdk-native-arm64.log` |
| Native driver metadata | Standalone InfVerif accepted each INF; native Inf2Cat checked all explicit VB/CO/NI/GE architecture targets without errors/warnings; expected PE machine types confirmed; SYS/CAT remain unsigned. | `wdk-infverif-*.log`, `wdk-native-results.json` |
| Mac inspection | The documented Mac mini responded over its pinned LAN SSH identity: macOS **12.7.6**, no selected Xcode/developer directory. No Xcode or SDK was installed by this workstream. | Read-only SSH `hostname`, `sw_vers`, `xcode-select -p`, `xcodebuild -version` |

The initial local development artifacts described below were **unsigned**; the latest CI builds also disable signing. The app SHA-256 is `7D683D5DE10775E981EFEDF9318DF1CE9DDF68ADA3ED18C8F69134EF6D61F174`. The development installer SHA-256 is `FB2689031FD629252C3844BC0359DDE76EFB1F17CECEFF246F4D45FA03718EDB`. `build/validation/artifacts.json` records final package/source hashes when bundled. These hashes identify development artifacts, not an approved release.

## Supported native driver results

On November 1, 2026 at 6:10 PM ET, both clean Release builds passed using 64-bit MSBuild from VS 2026 Build Tools, MSVC 19.50.35729 (toolset 14.50.35717), SDK/WDK 10.0.28000.0 and WDK assembly 10.0.28000.2526. Standalone InfVerif passed, and native Inf2Cat accepted the explicit VB/CO/NI/GE target lists for each architecture. These are build/package checks, not a kernel runtime or Microsoft signing result.

| Unsigned driver | PE machine | SYS SHA-256 |
|---|---|---|
| x64 | `0x8664` | `04BB13F63510DBA010320F2EBCFCA8ECA6A3A0D348663EC62FDFACB0BB1ED8E4` |
| ARM64 | `0xAA64` | `7FDA3D6F9BF01FF4A25A683FDFFDAC4D15CE9CAACC8DAD091BB106CFC3D051A1` |

All SYS/INF/CAT hashes are recorded in `build/validation/wdk-native-results.json`; the source files are fingerprinted in `wdk-native-source.json`. Packages are under `build/driver/<architecture>/Release/audiowire/`. The kit stamps the current build date into each INF; these hashes identify this build, not a claim of byte-for-byte reproducibility on another date.

The supported build exposed and resolved three project/tool-selection issues: empty INF version metadata caused StampInf error 87; 32-bit MSBuild could not load the new kit's x64 INF verifier DLL; and the unused `Inf2CatOS` property silently selected default targets instead of the intended Windows list. The project now supplies driver version `0.1.0.0`, the helper discovers installed WDK integration and selects 64-bit MSBuild/matching kit libraries, and `Inf2CatWindowsVersionList` carries the intended target list. An unused DRM-rights parameter is explicitly marked unused; no driver audio behavior changed.

## Failures reproduced and fixed

The initial MSVC build failed with `C2440`/`C2119` on nested callback captures and `C2065`/`C4430` on `DEFINE_PROPERTYKEY`. Hoisting the safe-pointer captures and explicitly including `propkeydef.h` resolved the build. Opus now uses the same static MSVC runtime as the application; the final build has no mixed-runtime linker warnings.

The new adversarial test first failed with:

```text
FAIL: network test: replaced code accepted an in-flight proof
```

An exchange now records its originating code-window generation. A changed or expired window is rejected before pair-key persistence. The test covers both replacement and expiry, permits either an explicit rejection or connection close, and requires no remembered pair/connected session. All four suites pass after the fix.

## Blocked or not executed

| Gate | Current limitation | What would settle it |
|---|---|---|
| iOS device | Hosted simulator build passed. MacBook Air connection work is held; test iPhone setup and physical execution remain pending. | Resume the designated Mac setup when requested, provision a real iPhone, and execute the device checklist. |
| Windows ARM64 app/core | Hosted app/core cross-compilation now passes. Local target tools are now installed; no local ARM64 app build or ARM64 hardware runtime check is claimed. | Execute runtime checks on ARM64 hardware. |
| Driver VM and 24-hour Verifier | No dedicated, snapshotted driver-test VM designated; no self-hosted WDK runner registered. | Identify/provision the VM and supported build host; execute and retain all driver acceptance evidence. |
| Hosted CI | All six jobs pass at `a38b556`; simulator artifact inspection also passes. | Rerun for a future release source revision; physical-device, kernel and signed-distribution gates remain separate. |
| Physical audio and UX | No PC/iPhone call, echo/one-way acoustic latency/Wi-Fi recovery, screen reader or large-text result. | Complete DEVICE_VALIDATION.md on actual target hardware. |
| Independent security review | Review packet and public fixtures prepared; no external reviewer engaged or approval obtained. | Reviewer independently reproduces vectors, resolves findings and signs off on the exact release revision. |
| Distribution | No Microsoft-approved driver signatures, signed installer lifecycle proof, App Store approval or resolved JUCE/App Store disposition. | Complete RELEASE_CHECKLIST.md and STORE_RELEASE.md. |

Historical driver failure, now resolved by the approved matching tool installation (both architectures):

```text
error MSB8020: The build tools for WindowsKernelModeDriver10.0
(Platform Toolset = 'WindowsKernelModeDriver10.0') cannot be found.
```

Compiler success, localhost tests and unsigned catalog generation do not establish kernel safety or a working physical speakerphone. No driver was installed, no host boot policy/firewall/default audio device was changed, and no release or store submission was published by these checks. The source branch is already public as recorded above.

The remaining execution procedures and release evidence requirements are in [device validation](DEVICE_VALIDATION.md), [pairing review](security/PAIRING_REVIEW.md) and [release checklist](RELEASE_CHECKLIST.md).

## Public CI publication checkpoint

The reviewed commits and all local artifacts remain preserved. A public GitHub push was stopped by automatic approval review before execution because explicit public-disclosure approval was required. At that earlier checkpoint no push or hosted CI run had occurred. Independent Windows/Linux archive validation continued successfully. Explicit public-push approval subsequently resolved the gate, and the hosted results are recorded above. The archive identified in `artifacts.json` corresponds to source/documentation revision `5439589`; these additional documentation results do not change the tested application source revision `fcc8ec6`.
