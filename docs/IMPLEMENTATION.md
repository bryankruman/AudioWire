# AudioWire implementation and release status

AudioWire is an implemented **pre-alpha development candidate**, not a completed or released v1. The original numbered documents remain the product contract. Writing the native iOS and driver code does not complete their physical-device exit criteria.

## Implemented source

| Area | What is present |
|---|---|
| Shared C++17 core | JUCE-free AOO source/sink integration; stereo PC sound and mono phone microphone; Opus and PCM presets; dynamic resampling; adaptive reserve; lock-free audio ring |
| Pairing/security | Six-digit local codes; two-minute/five-exchange gate; Ristretto255 SPAKE2-style exchange with mutual confirmation; directional session/control keys; authenticated full-packet encryption; replay protection |
| Connection runtime | TCP framing and encrypted controls; one-phone enforcement; saved-key resumption; heartbeat loss detection; reconnect backoff; immediate retry on rediscovery; silence when disconnected; explicit disconnect acknowledgement |
| Windows app | WASAPI loopback capture and format conversion; 10 ms clock with a high-resolution timer and multimedia thread scheduling; optional speaker mirror; private microphone driver feed; DPAPI; tray and notifications; autostart; explicit default-device switch and conditional restoration |
| iOS source | VoiceProcessingIO for speakerphone/AEC and RemoteIO for other modes; microphone permission fallback; Keychain; NWBrowser/Bonjour resolution and removals; audio route, interruption, foreground/background, low-power and battery handling |
| Shared UI | Connection, code and local QR; manual PC address and pairing URLs; modes, mute, volume, meters; saved devices, names, quality and reserve; phone permission settings; diagnostics; light/dark themes, labelled controls, 44-point controls and safe-area scrolling |
| Driver candidate | SysVAD-derived speaker and microphone; advertised format matrix; bounded pointer-free buffered-I/O feed; exclusive writer; kernel ring, underrun silence, overflow recovery, integer-only sample conversion and statistics; x64/ARM64 projects |
| Build/packaging | Pinned sources; CMake app/core targets; WDK project; optional LLVM compile/link check; SetupAPI helper; VM feed/probe tool; Inno Setup; source/licence bundling; Windows/core/iOS/driver CI definitions |

Phone mode, quality, volume and microphone privacy state establish the initial session settings. After connection, either UI can change shared controls. The PC restricts microphone modes when its microphone connection is unavailable.

## Evidence from this workspace

The [November 1 validation record](VALIDATION_STATUS.md) supersedes historical environment limitations below: a fresh dependency-download MSVC x64 app build, all four current test suites, Windows audio smoke check, workflow lint and developer-installer extraction now pass. Clean native x64/ARM64 MSVC/WDK Release builds and INF/catalog checks also pass after the approved local tool installation. iPhone, kernel runtime and distribution release gates remain open.


These checks have run locally on the Windows development host. The final packaged run is recorded in the ignored `build/` logs.

| Check | Result and scope |
|---|---|
| Windows x64 app/helper/feed-tool compilation | CMake Release build with portable w64devkit GCC 16.2.0 |
| Core CTest | Pairing and gate behavior, malformed inputs, encryption/authentication/replay, session backoff, control validation, concurrent audio ring, real bidirectional encrypted Opus, and 44.1-to-48 kHz exchange |
| App network CTest | Real TCP and UDP on localhost: abandoned exchanges consume attempts; wrong codes fail; unconfirmed pairs are not stored; a second phone is rejected; differing initial modes reconcile; volume and live mute/unmute work; spoof packets fail; explicit and automatic stored-key reconnect work; forget removes remembered pairs |
| Driver ABI CTest | Request structure sizes and buffered-I/O layout; all 65,536 PCM16-to-float conversions match exactly |
| Windows audio smoke check | Existing WASAPI loopback opens and 10 ms callbacks run; microphone injection disabled |
| UI rendering | Windows connection/settings/diagnostics and a narrow phone layout rendered in light/dark themes and inspected |
| Native x64/ARM64 driver builds | Clean VS 2026 Build Tools/MSVC 19.50.35729, SDK/WDK 10.0.28000.0 Release builds: zero warnings/errors; INF verification and explicit Windows target catalog checks pass; unsigned packages |
| Earlier x64/ARM64 driver compile/link | LLVM `clang-cl` and `lld-link` against Microsoft WDK 10.0.26100.1 headers/libraries; unsigned binaries produced |
| Driver metadata/catalog | Microsoft `Inf2Cat` accepts both candidates for the selected Windows 10/11 targets without errors or warnings and generates unsigned catalogs |
| Installer | Inno Setup development installer compiled; extracted app/helper hashes match; 25 dependency notices; default package excludes the kernel driver; installer not executed |
| FetchContent build path | Configuration and full app/tests pass without `AUDIOWIRE_VENDOR_DIR`, using CMake source overrides; lowercase dependency lookup verified |
| Corresponding source | Exact project and patched dependencies bundled; archive integrity checked; extracted offline core rebuilt from scratch and both core/ABI tests pass |

The LLVM driver checks are compile/link evidence, not proof of a supported WDK/MSVC build or kernel safety. The Windows audio smoke check does not test an injected microphone or audible Wi-Fi playback. Localhost audio is not a physical PC/iPhone call, discovery test, echo measurement, or acoustic latency measurement.

No driver was installed, no host boot policy was changed, no firewall rule or default audio device was changed by these checks, and no store/release was published.

## Build limitations

Xcode, the Apple SDK, an iPhone and an isolated driver-test VM are not available through this workspace. The approved local installation now supplies Visual Studio C++, x64/ARM64 target tools, Spectre libraries and matching SDK/WDK integration. Clean native driver builds and metadata checks pass; the earlier LLVM evidence is retained as historical validation. Hosted CI now builds both Windows architectures and the iOS simulator. Signed physical-iPhone builds, ARM64 hardware execution and kernel validation remain open; see VALIDATION_STATUS.md.

The CI files have now run on GitHub: all six core/application/simulator jobs pass at `a38b556`; see VALIDATION_STATUS.md. The extracted corresponding-source archive was rebuilt offline and its core/ABI tests passed. A fresh dependency-download MSVC x64 app build has now passed. The original builds used pinned checkouts through `AUDIOWIRE_VENDOR_DIR`; the latest validation did not. A separate clean-machine build remains open.

GCC 16 reports warnings inside the pinned JUCE empty-string reference-count code. The source retains an explicit empty-string deletion guard. The tests pass; these warnings and current MSVC/Xcode compatibility remain part of platform validation.

## Deliberate changes from the original plan

| Original proposal | Current candidate |
|---|---|
| Replace the repository with a full SonoBus fork/history | Preserve the existing AudioWire repository and consume pinned SonoBus JUCE/AOO forks; adapt the small audio integration and keep attribution |
| Mapped user/kernel shared ring | Fixed-size `METHOD_BUFFERED` feed into a kernel-owned ring; no shared mappings or user pointers |
| Clear AOO header with encrypted payload | Encrypt/authenticate the complete AOO datagram before any AOO parsing |
| SPAKE2 via libsodium | Application-specific SPAKE2-style Ristretto composition using libsodium primitives; independent review remains mandatory |
| Built-in acoustic latency result | Honest control-channel round-trip result; no claim that RTT is one-way acoustic latency |
| Direct SysVAD trimming | Use the already trimmed VirtualDrivers SysVAD derivative as a reference and retain its MIT/MS-PL provenance |
| Detailed mobile screen treatment | Shared JUCE screens with selector controls, one numeric code field, system-Camera QR links and scrolling settings; native segmented controls, an in-app scanner, Dynamic Type XXL and full VoiceOver/Narrator behavior still require the platform UX pass |

Default-device restoration journals prior role selections before switching. Normal startup recovers unfinished selections after a force-kill, and restoration preserves a manual user switch. Crash/restart recovery and uninstall still require VM validation, including alternate-admin elevation and autostart cleanup. The safe developer modes never open the user journal.

## Release gates

| Gate | Evidence still required |
|---|---|
| Apple build and device validation | Current Xcode simulator/device builds; provisioned iPhone; real Bonjour/manual/QR pairing; permission denial/recovery; route changes, Bluetooth, background/lock, interruptions and low-power behavior |
| Windows kernel validation | Native MSVC/WDK x64/ARM64 builds and INF/catalog checks now pass. Remaining: VM installation with protected test-signing material; shared/exclusive advertised formats; malformed requests, exclusive ownership, underrun/overflow, close/crash cleanup; 24-hour Driver Verifier; sleep/resume, RDP, HVCI and physical ARM64 loading |
| Real call/audio acceptance | Teams/Zoom/Discord/OBS/Audacity; both directions together; echo at arm's length; measured one-way acoustic latency at or below 60 ms; 30-second Wi-Fi loss/recovery within the product's five-second target; extended clock-drift tests |
| Security and accessibility | Independent PAKE/protocol review and vectors; packet/parser fuzzing; no-secret diagnostics review; VoiceOver/Narrator and Dynamic Type XXL; nontechnical first-run setup timing |
| Distribution | Resolve JUCE/App Store licensing; protected organisation signing identities and Apple/Microsoft accounts; Microsoft-approved kernel signatures; signed app/installer; VM install/uninstall; privacy manifest and policy/declarations; real review/demo material; TestFlight/App Store and Windows release publication |
| External reproducibility | Actual CI runs; fresh Windows/Xcode builds following the published instructions; exact corresponding source and notices accompany binaries |

The roadmap's phases are not marked complete: walking-skeleton evidence exists on localhost, driver compile/catalog evidence exists, and release source is present. Physical acceptance and public distribution remain open.
