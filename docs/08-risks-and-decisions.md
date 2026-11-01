# 08 — Risk register and open decisions

> Original product plan. See [implementation status](IMPLEMENTATION.md) for verified behavior, deviations and open release gates.

## Risks

| ID | Risk | Likelihood | Impact | Mitigation / trigger |
|---|---|---|---|---|
| R1 | **Kernel driver takes far longer than 8 weeks** (WDK toolchain, PortCls subtleties, BSODs) | Medium | Schedule (critical path) | Week-1 checkpoint: unmodified SysVAD running on a VM. If not done in 5 working days, bring in a driver contractor for the trimming, or switch to ACX and re-estimate. Ship P1's speaker-mode-only build to testers meanwhile so the project has users. |
| R2 | **Cannot obtain EV cert / Partner Center account** (needs an organisation; individuals refused) | Low–Medium | Cannot ship the driver to normal users | Use rlab.ai as the legal entity (D2). Fallback: ship with instructions to enable test-signing (developer/enthusiast audience only) while sorting it out. |
| R3 | **Apple's voice-processing AEC fights the 48 kHz Opus path** (internal resampling, AGC pumping, 300 ms leak) | Medium | Speakerphone quality (G3) | Time-boxed one week in P3; Plan B is WebRTC AEC3 (`webrtc-apm`) on the phone with our own reference alignment. |
| R4 | **Old `essej/aoo` fork has bugs we cannot fix and the author says "don't use it"** | Low for v1 (SonoBus ships on it daily) | Maintenance | Keep the AOO surface behind `core/Transport`; migrate to upstream 2.x in P5. |
| R5 | **iOS build of the JUCE 6 fork breaks on current Xcode / iOS SDK** | Medium | P0 slips days | SonoBus 1.7.3 (Jul 2024) still ships, so it built recently. Pin Xcode version in CI; patch the JUCE fork as needed (we own it). |
| R6 | **Licence friction: JUCE 6 GPLv3 on the App Store** (no App Store exception in JUCE's licence) | Low | Legal / takedown | Options: (a) accept, as SonoBus and others do; (b) ask a lawyer for a one-hour opinion; (c) long-term, replace JUCE on iOS with SwiftUI + the C++ core (the core has no JUCE dependency by design), which also removes the issue. Recommend (a)+(b) now, keep (c) as the architectural escape hatch — enforce "no JUCE in `core/`" from P0. |
| R7 | **Discovery fails on many home networks** (AP isolation, mesh, guest SSIDs, iOS local-network prompt declined) | High for some users | Support load, bad reviews | Manual IP entry + QR fallback from day one; in-app diagnostics that say *which* step failed; troubleshooting doc. |
| R8 | **Latency target (≤ 60 ms) missed on 2.4 GHz / congested Wi-Fi** | Medium | G4 | Auto jitter buffer already adapts; presets; tell users 5 GHz; measure and publish realistic numbers rather than promise. |
| R9 | **App Store rejection** (reviewer cannot test without the Windows side; "local network" justification; GPL) | Medium | Release delay 1–3 weeks | Demo video + detailed review notes; TestFlight first to shake out metadata issues; SonoBus precedent for GPL. |
| R10 | **Windows Defender / SmartScreen flags a new signed-but-unknown installer** | Medium | First-run friction | EV/Trusted Signing reputation builds over time; document the "More info → Run anyway" step; publish hashes. |
| R11 | **Scope creep back toward SonoBus** (someone wants multi-peer, effects, macOS on day one) | High | Never ships | Non-goals list in `00-overview.md` is the contract; anything else is a P5 issue. |
| R12 | **Name collision** — "AudioWire" may exist as a product/trademark | Unknown | Rename cost | Check before publishing anything public (D1). |

## Decisions needed from Bryan

Format follows the global response-style rules: each item has lettered options, one recommended.

* **D1 — Confirm the project name.**
  "AudioWire" is a placeholder. Renaming after the App Store listing, bundle id, driver name and
  mDNS type exist is a multi-day chore.
  * **A (recommended):** Run a quick check (App Store, Google Play, GitHub, USPTO/TESS, domains) this week; keep "AudioWire" if clear.
    *Impact:* 30 minutes now; avoids a rename later.
  * **B:** Keep "AudioWire" as internal codename and choose the public name in P4.
    *Impact:* free now, but the driver/INF/bundle ids get renamed in P4 (≈1 day) and early testers see two names.

* **D2 — Which legal entity holds the certificates and store accounts.**
  Partner Center (driver signing) and an EV certificate require an organisation; Apple's Developer
  Program can be individual or organisation. Both should match the copyright holder on the licence.
  * **A (recommended):** rlab.ai (existing org): Apple org enrolment ($99/yr), EV cert (~$300–500/yr), Partner Center (free).
    *Impact:* ~$400–600/yr recurring; D-U-N-S lookup and identity checks take 1–3 weeks — start in P0.
  * **B:** Personal Apple account + no EV cert; ship the driver test-signed only.
    *Impact:* $99/yr, but every Windows user must put their PC in Test Mode — kills G1 and G7 for normal users.

* **D3 — UI framework for v1.**
  * **A (recommended):** JUCE 6 (the SonoBus fork) on both platforms.
    *Impact:* one UI codebase, reuses SonoBus widgets, fastest to ship; iOS look is "close to native", not native. Keep `core/` JUCE-free so option B stays open.
  * **B:** SwiftUI on iOS + JUCE (or WinUI) on Windows.
    *Impact:* better iOS feel and removes R6, but roughly doubles UI work (+2–3 weeks) and splits the team's attention.

* **D4 — Windows driver base.**
  * **A (recommended):** Microsoft SysVAD sample (PortCls/WaveRT), trimmed, with `Virtual-Audio-Driver` as a worked reference.
    *Impact:* most examples, widest OS support (Win10 1903+/11, x64/ARM64); "legacy" model but Microsoft still ships it.
  * **B:** ACX (Audio Class eXtensions) from scratch.
    *Impact:* Microsoft's recommended future path, but fewer virtual-capture examples; higher research risk in an already-risky phase.

* **D5 — Pairing security level.**
  * **A (recommended):** SPAKE2 pairing + ChaCha20-Poly1305 encryption from P1.
    *Impact:* ~1 week of work; mic stream is private even on shared Wi-Fi; retrofitting crypto later touches every layer.
  * **B:** Simple code check, no encryption (AudioRelay-level).
    *Impact:* saves ~1 week; anyone on the LAN can record the PC or the mic. Not appropriate for an open-source project whose selling point is trust.

* **D6 — Echo cancellation approach.**
  * **A (recommended):** Apple `AUVoiceProcessingIO` on iPhone; WebRTC AEC3 only as Plan B.
    *Impact:* zero DSP code to maintain for the common case; time-boxed fallback if it misbehaves (R3).
  * **B:** WebRTC AEC3 on the phone from the start.
    *Impact:* full control and cross-platform (helps Android later) but adds a DSP dependency and tuning work in P3 (+1 week).

* **D7 — When to publish the repository.**
  * **A (recommended):** Public from P0 with a clear "pre-alpha" README, after D1 passes.
    *Impact:* GPL compliance is trivial, early contributors possible, and it honours the SonoBus author with visible credit from day one; risk is people trying it too early — the README handles that.
  * **B:** Private until P2's driver works.
    *Impact:* avoids early "it doesn't work" issues but loses months of potential help on the hardest part.

## Assumptions to verify early (cheap checks)

- `[assumed]` Microsoft Trusted Signing does **not** satisfy Partner Center's EV requirement → check the Partner Center registration page before buying an EV cert.
- `[assumed]` Current WDK includes `portcls.lib` for the chosen SKU → build unmodified SysVAD in P2 week 1.
- `[assumed]` SonoBus's `mobile/` project contains a prebuilt iOS Opus library reusable by our CMake target → inspect during P0.
- `[assumed]` JUCE 6 fork builds against iOS 17/18 SDK without patches → P0 day 1–2.
- `[assumed]` `NWBrowser` discovery works with the `_audiowire._udp` type when `NSBonjourServices` is set → P1; the common failure is a missing plist key.
