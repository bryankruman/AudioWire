# AudioWire validation status — November 1, 2026

Overall status: **partial**. Source preservation and pairing-review preparation are complete locally. Native Windows x64 and local Ubuntu WSL portable-core validation pass. Hosted Windows x64/ARM64 builds, the iOS simulator build, and three-platform core CI now pass. iPhone acceptance, supported driver builds/runtime, ARM64 hardware acceptance and signed distribution remain open.

## Hosted CI after branch publication

**All six hosted jobs passed at source revision `a38b556`.** The reviewed branch and fixes are public. The Windows x64 tests ran; Windows ARM64 and iOS simulator compilation do not establish physical hardware behavior.

| Check | Actual hosted evidence |
|---|---|
| Three-platform portable core | [Successful run 41351298121](https://github.com/bryankruman/AudioWire/actions/runs/41351298121): Windows 2022, Ubuntu 24.04 and macOS 15 all passed. |
| Windows x64 and ARM64 app | [Successful run 41351298096](https://github.com/bryankruman/AudioWire/actions/runs/41351298096): build, x64 tests, corresponding-source/licence bundling and artifact upload passed for both architectures. |
| iOS simulator | [Successful run 41351298118](https://github.com/bryankruman/AudioWire/actions/runs/41351298118): configuration, simulator build and artifact upload passed. Downloaded bundle inspection confirmed ARM64 Mach-O with the iOS Simulator platform marker, bundle ID `ai.rlab.audiowire`, and minimum iOS 15.0. No physical iPhone execution was claimed. |

The first macOS jobs were cancelled by GitHub's scheduled macOS 14 image brownout. Moving to macOS 15 resolved that runner issue. The actual platform builds then exposed old-JUCE assumptions: Windows ARM64 was classified as Intel and attempted x86 intrinsics; the macOS juceaide build helper used a removed window-snapshot API; and JUCE's iOS asset-catalog directory conflicted with CMake's target installation rule. Narrow compatibility patches fixed the ARM64 build and host helper. Xcode now owns iOS bundle packaging/installation instead of the CMake desktop install rule. All target jobs subsequently passed.

The pinned Windows ARM64 port reports unavailable CPU clock metadata as unknown and disables x86 capability probing on ARM. The macOS snapshot fallback is used only by the headless host build helper; AudioWire does not ship a macOS GUI application. Audio transport and pairing protocol code were not changed by these compatibility fixes.

The M1 MacBook Air has been selected for physical iOS development. Connection setup and protected local Apple provisioning are pending; the inventoried 2014 Mac mini was freshly checked as macOS 12.7.6 with no Xcode. A local Windows SDK/WDK installation plan and Microsoft-signed installers are staged for review. No developer-tool installation, SSH access change, host driver load or VM provisioning has occurred yet.

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
| Mac inspection | The documented Mac mini responded over its pinned LAN SSH identity: macOS **12.7.6**, no selected Xcode/developer directory. No Xcode or SDK was installed by this workstream. | Read-only SSH `hostname`, `sw_vers`, `xcode-select -p`, `xcodebuild -version` |

The initial local development artifacts described below were **unsigned**; the latest CI builds also disable signing. The app SHA-256 is `7D683D5DE10775E981EFEDF9318DF1CE9DDF68ADA3ED18C8F69134EF6D61F174`. The development installer SHA-256 is `FB2689031FD629252C3844BC0359DDE76EFB1F17CECEFF246F4D45FA03718EDB`. `build/validation/artifacts.json` records final package/source hashes when bundled. These hashes identify development artifacts, not an approved release.

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
| iOS simulator/device | No usable Xcode host or test iPhone designated; inventoried Mac mini lacks Xcode. | Run the prepared iOS build script on a compatible Mac, provision a real iPhone, and execute the device checklist. |
| Windows ARM64 app/core | Hosted app/core cross-compilation now passes. Local target tools are still absent, and no ARM64 hardware runtime check ran. | Install the reviewed local target tools if needed; execute runtime checks on ARM64 hardware. |
| Supported x64 and ARM64 driver builds | Both MSBuild attempts failed before compilation with **MSB8020**, missing `WindowsKernelModeDriver10.0`. | Matching supported Visual Studio/SDK/WDK integration and libraries, followed by successful builds. |
| Driver VM and 24-hour Verifier | No dedicated, snapshotted driver-test VM designated; no self-hosted WDK runner registered. | Identify/provision the VM and supported build host; execute and retain all driver acceptance evidence. |
| Hosted CI | All six jobs pass at `a38b556`; simulator artifact inspection also passes. | Rerun for a future release source revision; physical-device, kernel and signed-distribution gates remain separate. |
| Physical audio and UX | No PC/iPhone call, echo/one-way acoustic latency/Wi-Fi recovery, screen reader or large-text result. | Complete DEVICE_VALIDATION.md on actual target hardware. |
| Independent security review | Review packet and public fixtures prepared; no external reviewer engaged or approval obtained. | Reviewer independently reproduces vectors, resolves findings and signs off on the exact release revision. |
| Distribution | No Microsoft-approved driver signatures, signed installer lifecycle proof, App Store approval or resolved JUCE/App Store disposition. | Complete RELEASE_CHECKLIST.md and STORE_RELEASE.md. |

Driver failure output (both architectures):

```text
error MSB8020: The build tools for WindowsKernelModeDriver10.0
(Platform Toolset = 'WindowsKernelModeDriver10.0') cannot be found.
```

Compiler success, localhost tests and unsigned catalog generation do not establish kernel safety or a working physical speakerphone. No driver was installed, no host boot policy/firewall/default audio device was changed, and no public source or store submission was published by these checks.

The remaining execution procedures and release evidence requirements are in [device validation](DEVICE_VALIDATION.md), [pairing review](security/PAIRING_REVIEW.md) and [release checklist](RELEASE_CHECKLIST.md).

## Public CI publication checkpoint

The reviewed commits and all local artifacts remain preserved. A public GitHub push was stopped by automatic approval review before execution because explicit public-disclosure approval was required. At that earlier checkpoint no push or hosted CI run had occurred. Independent Windows/Linux archive validation continued successfully. Explicit public-push approval subsequently resolved the gate, and the hosted results are recorded above. The archive identified in `artifacts.json` corresponds to source/documentation revision `5439589`; these additional documentation results do not change the tested application source revision `fcc8ec6`.
