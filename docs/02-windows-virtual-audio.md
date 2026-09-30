# 02 — Windows virtual audio devices (the kernel driver)

This is the hardest and riskiest part of the project. Nothing in SonoBus helps here; it is new
work. Read `08-risks-and-decisions.md` R1 and R2 alongside this.

## Why a kernel driver is unavoidable

Windows has no user-mode API for creating an audio *capture* endpoint (a "microphone") that other
applications can select. Every product that does this — VB-Cable, Virtual Audio Cable, AudioRelay,
Voicemeeter — ships a kernel-mode audio driver. WASAPI loopback (user mode) covers the *speaker*
direction, but not the mic direction.

## Design

### Endpoints exposed

| Endpoint | Direction as Windows sees it | Who uses it | Formats |
|---|---|---|---|
| `AudioWire Speaker` | Render (playback) | Any app / Windows default output | 48 kHz, 2 ch, 16/24/32-bit + float; also 44.1 kHz for apps that insist |
| `AudioWire Microphone` | Capture (recording) | Any app / Windows default input | 48 kHz, 1 ch (mono) and 2 ch; 16-bit + float |

Exactly two devices appear in Sound settings. No hidden "cable input" the user could pick by
mistake (a common VB-Cable confusion).

### How `AudioWire.exe` exchanges audio with the driver

- **Speaker direction — no custom interface.** The app opens the `AudioWire Speaker` render
  endpoint in **WASAPI loopback** mode and reads what Windows mixes into it. SysVAD already
  implements loopback pins; this path is standard and needs no IOCTLs.
- **Mic direction — one private device interface.** The driver registers a device interface GUID
  (`{AUDIOWIRE-…}`); the app opens it with `CreateFile` and:
  1. `IOCTL_AUDIOWIRE_MAP_RING` — driver allocates a lock-free ring buffer (e.g. 200 ms at 48 kHz
     float mono ≈ 38 KB) and maps it into the caller with `MmMapLockedPagesSpecifyCache`, returning
     a user-mode pointer plus a shared read/write position header.
  2. The app writes decoded PCM into the ring; the WaveRT capture pin's timer DPC copies from the
     ring into the pin's cyclic buffer at the device's clock.
  3. On underrun the driver emits silence (or comfort noise) and bumps a counter the app can read
     with `IOCTL_AUDIOWIRE_GET_STATS` for the diagnostics screen.
  4. A single app handle at a time; closing it drops the mapping and the mic goes silent.

  *Why not a paired hidden render endpoint (the VB-Cable model)?* It avoids the IOCTL code but
  leaves a third device visible to users and to apps like Zoom that enumerate everything. The
  IOCTL path is ~300 lines of kernel code and keeps the device list clean.

### Clocking

Virtual devices have no crystal. The driver runs both endpoints from one high-resolution timer
(`ExSetTimerResolution`/`KeSetTimerEx` at 10 ms, or the WaveRT "timer-driven" mode SysVAD
demonstrates). The app-side AOO sink already does **dynamic resampling** to slave the network
stream to the local device clock, so drift between phone and PC is absorbed there, not in the
driver.

### Base to start from

| Option | What it is | Pros | Cons |
|---|---|---|---|
| **A (recommended): Microsoft SysVAD sample** (`Windows-driver-samples/audio/sysvad`, MS-PL) | Reference PortCls/WaveRT virtual audio adapter with speaker, mic, loopback pins and a tone generator | Most documented virtual audio code in existence; every open virtual-audio driver on GitHub descends from it; supports Win10 1903+ and 11, x64 + ARM64 | Large (also contains HDMI, Bluetooth, cellular endpoints to delete); PortCls is the "legacy" model |
| **B: `VirtualDrivers/Virtual-Audio-Driver`** (MIT, SysVAD-derived, 415★) | Already trimmed to one speaker + one mic | Someone did the trimming; MIT | Test-signed only; devices are independent (no app feed path) so the IOCTL work is the same as A; small maintainer base — treat as a *reference* while doing A |
| **C: ACX (Audio Class eXtensions) from scratch** | Microsoft's KMDF-based successor to PortCls, v1.1, Win10 2004+ | Cleaner object model, what Microsoft recommends for new drivers | Fewer worked examples of virtual capture + user-mode feed; higher chance of hitting undocumented corners; equivalent OS support to A |
| **D (skip): SAR's `sar-virtual-wasapi-driver`** | Pre-alpha, 3 commits, unsigned | — | Not functional yet |

Decision: **A**, using B as a worked example of the trimming. Re-evaluate C only if A's timer/
loopback behaviour blocks the speakerphone latency target.

### Trimming SysVAD to `audiowire.sys`

1. Delete every endpoint except `SpeakerTopology/Wave` and `MicArray1Topology/Wave` (rename
   to `AwSpeaker*`, `AwMic*`). Remove HDMI, SPDIF, Bluetooth, cellular, handset, FM, tablet variants.
2. Remove the sine-wave "tone generator" and the `SaveData` WAV writer used for testing.
3. Add `AwRing.cpp/h` (shared ring + mapping) and `AwIoctl.cpp/h` (device interface, IOCTLs).
4. Hook `CMiniportWaveRTStream` capture path: replace tone generation with a ring copy.
5. New `audiowire.inf`: two endpoints, friendly names, icons, `KSCATEGORY_AUDIO/RENDER/CAPTURE`,
   `PKEY_AudioEndpoint_*` defaults (form factor Speakers / Microphone).
6. Keep the sample's `.vcxproj` layout; build with **WDK for Windows 11 (10.0.26100+) on VS 2022**.
   Note: recent WDKs dropped `portcls.lib` from some SKUs (see Windows-driver-samples issue
   #1180) — pin the WDK version in `docs/09-dev-environment.md` once verified.

## Signing — the real cost of this component

Windows 10/11 x64 refuse to load a kernel driver that Microsoft has not countersigned. There is
no free path for end users.

| Stage | Mechanism | Cost | Who it works for |
|---|---|---|---|
| Development | `bcdedit /set testsigning on` + self-signed test cert | Free | Developers only; shows a "Test Mode" watermark |
| **Release** | **Attestation signing via Partner Center (Hardware Dev Center)** | Free service, **but requires an EV code-signing certificate** to register and to sign the submission CAB. EV certs: roughly $290–$500/year (Sectigo, DigiCert, GlobalSign); max lifetime now 459 days. Partner Center also requires a **company/organisation** identity — individuals cannot register. | Any Windows 10/11 desktop; *not* distributed via Windows Update (fine — we ship via installer) |
| Not needed | Full WHCP/HLK certification | Weeks of lab work | Only for Windows Update distribution |

Microsoft **Trusted Signing** ($9.99/month) covers *user-mode* code signing (the installer and
`AudioWire.exe`) but `[assumed]` does **not** substitute for the EV certificate in the Hardware
Program — verify on Partner Center before buying an EV cert; if it has changed, that saves ~$400/yr.

Implications:
- `08-risks-and-decisions.md` **D2** asks for an organisation (rlab.ai) to hold the EV cert and the Partner Center account.
- Every driver change that ships needs a resubmission (automated checks, usually same-day).
- Until the EV cert exists, testers must enable test-signing. The installer will detect this and explain it.

## Installer behaviour

1. Installer is signed (Trusted Signing or the EV cert).
2. Installs `audiowire.sys` + `.inf` with `pnputil /add-driver audiowire.inf /install` and creates the
   root-enumerated device (`devcon`-equivalent via SetupAPI: `SWD\AUDIOWIRE\0000`).
3. Installs `AudioWire.exe` to `%ProgramFiles%\AudioWire`, autostart via `HKCU\...\Run` (user can toggle).
4. Adds inbound firewall rules for the app (UDP for audio, TCP for pairing, UDP 5353 mDNS).
5. Does **not** change the Windows default audio devices silently. The app asks on first run:
   *"Use the iPhone for all PC sound? [Yes] [Only when apps pick it]"*.
6. Uninstall removes the device, driver package (`pnputil /delete-driver`), firewall rules and autostart.

## Fallback when the driver is absent or blocked

Some machines (managed corporate PCs, S-mode, HVCI-strict) may refuse the driver even when
signed. The app must still be useful:

- **Speaker direction works without the driver**: loopback-capture the *default* output device instead. Downside: PC speakers also play unless the user mutes them. The UI says so.
- **Mic direction cannot work without a driver.** The app shows a clear message and a link to the
  explanation, and offers to use an already-installed VB-Cable if one is detected (detect by
  endpoint name `CABLE Input`), writing to it via normal WASAPI render. This keeps power users
  unblocked without us shipping VB-Cable.

## Test plan for the driver

- Driver Verifier (standard + IRQL + pool tracking) clean for 24 h of connect/disconnect cycling.
- HLK "Audio" basic tests optional but a good smoke test even without certification.
- Format matrix: 44.1/48 kHz × 16/24/32/float × mono/stereo, shared and exclusive mode, in `foobar2000`, Teams, Zoom, Discord, OBS, Audacity.
- Sleep/resume, fast user switching, RDP session (audio redirection interacts with virtual devices).
- ARM64 build boots on a Snapdragon X laptop (Windows 11 ARM) — SysVAD supports ARM64.

## Effort estimate

Solo developer with strong C++ but no prior WDK experience, with AI assistance: **4–8 weeks** to a
test-signed driver that passes the format matrix; plus **1–3 weeks** of calendar time for the EV
cert (identity verification is slow) and first attestation. This is the critical path of the whole
project; start the certificate process in Phase 0.
