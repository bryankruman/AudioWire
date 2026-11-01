# AudioWire release checklist

Status: **release blocked**. The project has a preserved pre-alpha source baseline, not a validated v1. A row closes only with linked evidence for the exact proposed release commit and artifact hashes. This checklist does not authorize publishing, signing, driver installation, or infrastructure changes.

| ID | Gate | Current evidence | Closure requirement |
|---|---|---|---|
| REL-01 | Source preserved | Local implementation baseline `305fe54` on the task branch; original development checkout preserved. | Commit all reviewed follow-up changes; attach final source commit and source archive hash. |
| REL-02 | Windows reproducibility | Fresh dependency-download MSVC x64 app build passes. The corresponding-source archive also rebuilt offline with 3/3 Windows and 2/2 Ubuntu WSL suites passing; see VALIDATION_STATUS.md. | Clean-machine MSVC x64/ARM64 builds, all tests green, exact sources and required dependency notices accompany each artifact. |
| REL-03 | Actual CI execution | GitHub had no workflow runs when queried on November 1, 2026; workflows are present locally. | Publish the reviewed branch with approval, collect core Windows/Linux/macOS, Windows x64/ARM64 and iOS simulator run URLs and artifact hashes. A locally linted workflow is not a green run. |
| REL-04 | iOS device validation | Reachable inventoried Mac mini has no selected Xcode; no test iPhone designated. | Complete IOS-01 through IOS-05 in DEVICE_VALIDATION.md on the intended minimum and current iOS versions. |
| REL-05 | Driver builds and runtime | x64 and ARM64 supported attempts fail with missing WindowsKernelModeDriver10.0 toolset. Existing LLVM binaries remain unsigned and runtime-unvalidated. | Complete DRV-01 through DRV-06, including supported builds, snapshotted VM, 24-hour Verifier, HVCI and physical ARM64 loading. |
| REL-06 | Real audio acceptance | Localhost encrypted audio and Windows loopback checks are available; no acoustic acceptance evidence. | Pass AUDIO-01 through AUDIO-03 with real Windows/iPhone call, no echo, measured one-way latency and timed network recovery. |
| REL-07 | Pairing and parser security | Review packet and public synthetic candidate vectors in docs/security/PAIRING_REVIEW.md and tests/vectors. | Independent cryptographic review and separately reproduced vectors; resolve findings; parser fuzzing/sanitizers and no-secret diagnostics checks. Test vectors alone do not close this gate. |
| REL-08 | Accessibility and first run | Shared UI source and desktop previews exist. | Pass UX-01 with physical-device VoiceOver, Windows Narrator, large text, route permissions and a timed first call. |
| REL-09 | Installer lifecycle | Developer installer source and earlier unsigned package exist; no clean-VM lifecycle acceptance. | Pass PKG-01 for install/update/uninstall, alternate-admin elevation, autostart and default-device restoration; retain artifact hashes. |
| REL-10 | Licensing | AudioWire exception and dependency notices present; JUCE/App Store disposition still open. | Resolve the dependency licensing question for actual distribution and keep corresponding-source availability with the binaries. |
| REL-11 | Signing | Inspected app, installer and x64/ARM64 drivers are unsigned. | Protected organisation identities; Microsoft-approved driver signatures; app and completed installer signatures verified. No private signing material in source or logs. |
| REL-12 | Apple review/privacy | STORE_RELEASE.md describes required material. | Final privacy manifest/API audit, privacy policy/declarations, provisioning, TestFlight device validation, real screenshots/demo and approved App Store review. |
| REL-13 | Release publication | No release publication was performed by this workstream. | All preceding gates pass; explicit release approval; publish the signed binaries, exact sources and truthful platform evidence. |

Record: proposed release commit, validation operator/date, complete CI run URLs, device/VM identities, test result links, app/driver/installer/source SHA-256 values, reviewer signoffs, known residual limitations and publication approval. A platform that could not be tested stays blocked.

Current evidence: [validation status](VALIDATION_STATUS.md), [device/VM procedures](DEVICE_VALIDATION.md), [pairing review](security/PAIRING_REVIEW.md), [implementation scope](IMPLEMENTATION.md), [store preparation](STORE_RELEASE.md).
