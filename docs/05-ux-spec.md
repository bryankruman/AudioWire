# 05 — UX specification

Design principle: **the default path has no decisions in it.** Every setting that SonoBus puts on
the main screen (codec, bitrate, buffer, per-peer gain, group/server) either becomes automatic or
moves behind "Advanced". Target: a non-technical user completes setup without reading anything but
the buttons.

## The three-tap setup (happy path)

| Step | Where | What the user sees | What happens underneath |
|---|---|---|---|
| 0 | PC | Runs `AudioWire-Setup.exe`. UAC prompt (driver). Installer finishes; tray icon appears with a balloon: *"Open AudioWire on your iPhone to connect."* | Driver installed, service advertising on mDNS, code generated |
| 1 | iPhone | Opens app. Screen shows *"Looking for PCs…"* then a card **"BRYAN-DESKTOP"** with a **Connect** button. iOS asks for Local Network permission (once). | `NWBrowser` found the service |
| 2 | iPhone | Taps **Connect**. Screen: *"Enter the code shown on your PC"* with a 6-box code field, or **Scan QR**. PC tray window pops up showing `4 8 2 9 1 7` and a QR. | SPAKE2 pairing |
| 3 | iPhone | Big green **Connected** state. Mode selector defaults to **Speakerphone** (both). PC shows toast: *"iPhone connected. Use it for all PC sound? [Yes] [Not now]"*. | Session key, AOO streams start; optional default-device switch |

After that: opening the app (or it being in the background with the PC on) reconnects silently.

## iPhone screens

### S1 — Home / Connect (only screen most people see)

- Top: status pill — *Looking for PCs* / *Connected to BRYAN-DESKTOP* / *Reconnecting…* / *PC not found*.
- Middle: **one big circular button**: *Connect* → *Connected* (tap to disconnect, with confirmation only when a stream is active).
- Mode segmented control: **Speakerphone** · **Speaker only** · **Mic only**.
- Two compact rows: **Mute mic** toggle (large, thumb-reachable), **Volume** slider (phone output).
- Small live meters (input/output) so the user can see it is working. No numbers.
- Bottom: *Playing through: iPhone Speaker* (route label), gear icon → Settings.

### S2 — Pairing

- Code entry (6 boxes, numeric keypad auto-focus), **Scan QR** button, *"Can't see a code? Click the AudioWire icon in the Windows tray."*
- Failure: *"That code didn't match. 4 tries left."*

### S3 — Settings

- **Audio quality**: Balanced (default) · High · Max (wired-quality, needs strong Wi-Fi) — one sentence each.
- **Keep playing when I switch apps** (background audio) — on by default; explanation of the red status bar.
- **Mix with other audio on this phone** — off by default.
- **Advanced** (collapsed): Latency vs. stability slider (Auto default), Show technical stats, Echo cancellation (On/Off — off means RemoteIO), Force 44.1 kHz.
- **Paired PCs**: list with *Forget*.
- **About**: version, licence (GPLv3 + App Store exception), source link, *Copy diagnostics*.

### S4 — Diagnostics (from Advanced)

- Round-trip latency (from the probe), packet loss %, jitter buffer ms, bitrate, Wi-Fi band/RSSI if available, driver status reported by PC.

### Empty/error states (each with one action)

- No PC found after 10 s → *"Make sure AudioWire is running on your PC and both devices are on the same Wi-Fi."* + **Enter PC address manually**.
- Local Network permission denied → deep link to iOS Settings.
- Microphone permission denied → deep link to iOS Settings; app falls back to Speaker only.
- PC says driver missing → *"Your PC needs the AudioWire audio driver for microphone mode. Speaker mode still works."* + link.

## Windows tray app

### W1 — Tray icon states

Grey (waiting for phone) · Green (connected) · Amber (reconnecting) · Red (driver problem). Tooltip: *AudioWire — Connected to "Bryan's iPhone" · Speakerphone · 38 ms*.

### W2 — Popover (left-click)

- Header: phone name + battery %, mode, latency.
- **Pair a new phone** → shows 6-digit code + QR, 2-minute countdown.
- **Use iPhone for all PC sound** toggle (sets default output/input devices; reverts on disconnect if "auto-revert" is on — default on).
- **Also play on this PC's speakers** toggle (mirror).
- Volume slider for phone output (remote control).
- *Open Windows Sound settings* link.

### W3 — Settings window

- Start with Windows (on), Show notifications, PC display name, Audio quality preset (shared with phone; either side can change), Advanced (same as phone + "Loopback device" chooser for the no-driver fallback), Paired phones (Forget), Driver status + *Reinstall driver*, Diagnostics + *Copy logs*, About.

### W4 — First-run and driver problems

- If test-signing is off and the driver is not attestation-signed (pre-release builds), show a plain-language dialog with the exact `bcdedit` command and what "Test Mode" means.
- If the driver failed to install (blocked by policy), fall back to speaker-only and say so in the tray.

## Copy rules

- Never say: peer, codec, jitter, buffer, endpoint, group, server, WASAPI, loopback, PortCls.
- Say: PC, iPhone, speaker, microphone, quality, latency, connected.
- Every error names the fix.

## Accessibility

- All controls labelled for VoiceOver / Narrator; the connect button reports state changes.
- Minimum 44 pt touch targets; respects Dynamic Type at least to XXL; dark mode on both platforms.
