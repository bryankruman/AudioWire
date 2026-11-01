# AudioWire Windows driver candidate

**Status: clean native MSVC/WDK Release builds, INF verification and Microsoft catalog checks pass for x64 and ARM64. Packages remain unsigned; VM/device behavior is unverified. Do not install this candidate on the development host.**

The source is based on VirtualDrivers/Virtual-Audio-Driver at `bb34fba15faf569a6ae9bdea360bc1cf4821354e`, itself derived from Microsoft's SysVAD sample. Copyright and MS-PL/MIT notices are retained in `vendor/`.

There are two audio devices: AudioWire Speaker and AudioWire Microphone. The speaker advertises 44.1/48 kHz stereo PCM16/24/32 and float32. The microphone advertises 48 kHz mono/stereo PCM16 and float32. Capture duplicates a mono phone feed into stereo when requested.

## App-to-driver feed

The original plan proposed mapped shared memory. This candidate uses bounded `METHOD_BUFFERED` requests instead: 480 mono PCM16 samples every 10 ms, a versioned pointer-free ABI, and a kernel-owned 200 ms ring. This is about 100 KB/s of copied input. It avoids per-process memory mappings and their teardown/IRQL complications.

The DPC reads from the ring, converts into the negotiated capture format using integer operations, and supplies silence on underrun. Overflow discards the oldest samples. The app handle is exclusive; close/cleanup clears data. The control device has a dedicated security descriptor and unique class GUID.

The tone generator and render-to-disk recording have been removed. Speaker data is captured by the application using standard Windows audio APIs.

## Build

Install a matching Visual Studio C++/SDK/WDK combination, WDK Visual Studio integration, target compiler tools and Spectre libraries, including `portcls.lib`. The November 1, 2026 clean builds used Visual Studio 2026 Build Tools (18.x), MSVC 19.50.35729 (toolset 14.50.35717) and SDK/WDK 10.0.28000.0 (WDK assembly 10.0.28000.2526). From PowerShell:

```powershell
./scripts/build-driver.ps1 -Architecture x64
./scripts/build-driver.ps1 -Architecture ARM64
```

The script finds an installed Visual Studio instance with the requested WDK toolset, selects 64-bit MSBuild, and chooses the latest installed matching kernel headers/PortCls libraries. Use `-KitVersion 10.0.28000.0` to select a kit explicitly. An EWDK or other configured environment can supply `-MSBuildPath <absolute-64-bit-MSBuild.exe>` and `-KitVersion <installed-kit-version>`. The script does not install developer tools.

The project outputs under `build/driver/<architecture>/Release`; its `audiowire/` subdirectory contains the SYS/INF/CAT package. Signing stays off. `InfTimeStamp` defaults to driver version `0.1.0.0`; StampInf uses the build date, so package hashes identify the exact generated files rather than promising identical bytes across dates. The native Inf2Cat target checks the explicit architecture-specific VB/CO/NI/GE Windows target list. A build result is a candidate, not an attestation-signed end-user package.

## VM-only development installation

Use a snapshotted Windows VM with kernel debugging. Test signing and the VM's Secure Boot configuration are developer prerequisites. Do not change host boot policy.

Test-sign the candidate with your own protected development certificate. In the VM, the compiled `AudioWireDriverSetup.exe install <absolute-path-to-inf>` helper registers the exact `ROOT\AudioWire` device and installs the package. `uninstall` removes only that hardware ID and its exact installed OEM INF. The helper requires elevation and is never executed by automated app/core tests.

## Required validation

Build both architectures. Run Driver Verifier standard/IRQL/pool checks for 24 hours of cycling. Check every advertised format in shared and exclusive mode, no-feed silence, malformed IOCTL lengths/versions/counts, exclusive handle ownership, close/crash cleanup and overflow. Then check sleep/resume, fast user switching, RDP and HVCI on an isolated machine.

Verify simultaneous speaker/microphone behavior in Teams, Zoom, Discord, OBS and Audacity. Verify audio clock drift over an extended call. A physical ARM64 PC must load the ARM64 candidate.

Only after those checks should a driver package go through Microsoft signing/attestation and the release installer. The driver workflow needs an explicitly provisioned WDK runner. Compile/link and catalog checks do not validate runtime kernel safety. App tests only check the feed ABI and integer sample conversion.

## Optional local LLVM check

`scripts/check-driver-llvm.ps1 -WdkRoot <unpacked-x64-WDK> -Arm64WdkRoot <unpacked-ARM64-WDK> -Architecture x64` checks this source against WDK headers/libraries without installing or signing a driver. Repeat with `-Architecture ARM64`. The script copies WDF headers into its output directory and applies the compatibility adjustments there. It leaves installed SDK/WDK headers untouched. Output is `build/driver/clang/<architecture>/audiowire.sys` and its architecture-specific INF. Run the matching Microsoft `Inf2Cat` target checks to generate an unsigned catalog.

## VM feed/probe tool

The application build includes `AudioWireDriverFeed.exe`. **Use it only inside the driver-test VM after the candidate is installed.** `--probe` checks version, length and sample-count rejection, unknown requests, exclusive ownership and statistics. A mono 48 kHz PCM16 WAV path feeds known test audio in 10 ms blocks. The tool never installs a driver or changes boot policy. Check no-feed silence and record the AudioWire microphone in the VM; a probe passing does not replace format, cleanup or Verifier tests.
