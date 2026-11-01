# AudioWire validation status — November 1, 2026

Overall status: **partial**. Source preservation and pairing-review preparation are complete locally. Native Windows x64 and local Ubuntu WSL portable-core validation pass. Actual hosted CI, iPhone acceptance, supported driver builds/runtime, ARM64 and signed distribution remain open.

## Revisions and evidence scope

The original development checkout at `C:\Users\Bryan\Projects\AudioWire` was copied into the task worktree and all 109 inventoried source files were SHA-256 compared before changes. Commit `305fe54` preserves that source. Commit `fcc8ec6` contains the tested compiler/security/build changes and public protocol fixtures. Documentation-only follow-up changes do not constitute a new app or driver validation.

Evidence is under ignored `build/validation/` in the task worktree. Logs are local artifacts, not hosted CI results. No implementation was pushed to GitHub and no release was published. The original checkout's uncommitted work remains intact.

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
| GitHub inspection | Repository is public; workflow run list empty; registered self-hosted runner count **0** when queried. | Current-session `gh repo view`, `gh run list`, `gh api .../actions/runners` |
| Mac inspection | The documented Mac mini responded over its pinned LAN SSH identity: macOS **12.7.6**, no selected Xcode/developer directory. No Xcode or SDK was installed by this workstream. | Read-only SSH `hostname`, `sw_vers`, `xcode-select -p`, `xcodebuild -version` |

Inspected final artifacts are **unsigned**. The app SHA-256 is `7D683D5DE10775E981EFEDF9318DF1CE9DDF68ADA3ED18C8F69134EF6D61F174`. The development installer SHA-256 is `FB2689031FD629252C3844BC0359DDE76EFB1F17CECEFF246F4D45FA03718EDB`. `build/validation/artifacts.json` records final package/source hashes when bundled. These hashes identify development artifacts, not an approved release.

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
| Windows ARM64 app/core | Local ARM64 CMake probe fails while resolving VCTargetsPath with no valid Debug/ARM64 configuration; Visual Studio component query found no ARM64 C++ tools. | Provision ARM64 target tools or run the committed ARM64 CI job; then execute runtime checks on ARM64 hardware. |
| Supported x64 and ARM64 driver builds | Both MSBuild attempts failed before compilation with **MSB8020**, missing `WindowsKernelModeDriver10.0`. | Matching supported Visual Studio/SDK/WDK integration and libraries, followed by successful builds. |
| Driver VM and 24-hour Verifier | No dedicated, snapshotted driver-test VM designated; no self-hosted WDK runner registered. | Identify/provision the VM and supported build host; execute and retain all driver acceptance evidence. |
| Actual hosted CI | Workflows exist and lint, but the code is committed locally only. | Approve publishing the reviewed task branch, then retain actual run URLs/results for Windows/Linux/macOS/iOS. |
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

The reviewed commits and all local artifacts remain preserved. A public GitHub push was stopped by automatic approval review before execution because explicit public-disclosure approval was required. No push or hosted CI run occurred. Independent Windows/Linux archive validation continued successfully while that approval remained pending. The archive identified in `artifacts.json` corresponds to source/documentation revision `5439589`; these additional documentation results do not change the tested application source revision `fcc8ec6`.
