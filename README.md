# AudioWire

**Status: planning — no code yet.** This repository currently holds the implementation plan. The
SonoBus fork itself is created in Phase 0 (see [docs/07-roadmap.md](docs/07-roadmap.md)).

AudioWire turns an iPhone into a wireless speaker and microphone (a speakerphone) for a Windows PC,
over the local Wi-Fi network. It is an open-source (GPLv3) alternative to AudioRelay, built by
forking [SonoBus](https://github.com/sonosaurus/sonobus) and replacing SonoBus's musician-oriented
interface with a setup that takes three taps.

## What it will do (v1)

- **PC → iPhone ("speaker mode")** — an `AudioWire Speaker` output device appears in Windows.
  Anything the PC plays to it comes out of the iPhone's speaker (or whatever is plugged into it).
- **iPhone → PC ("mic mode")** — an `AudioWire Microphone` input device appears in Windows. Any
  app (Teams, Zoom, Discord, OBS, games) can pick it as its microphone.
- **Both at once ("speakerphone")** — with echo cancellation done on the iPhone, so the far end
  does not hear itself.
- **Zero-config pairing** — the iPhone finds the PC on the LAN by itself; you confirm a 6-digit
  code (or scan a QR code) once, then it reconnects automatically.
- **Both devices ship their own virtual audio devices** — no VB-Cable, Voicemeeter or manual
  routing on Windows.

Platforms at launch: **iOS 15+** (App Store) and **Windows 10 2004+ / Windows 11** (x64, ARM64).
macOS, Linux and Android are deliberately out of scope for v1 (see
[docs/00-overview.md](docs/00-overview.md#non-goals-for-v1)).

## Where the plan lives

| Doc | What it covers |
|---|---|
| [docs/00-overview.md](docs/00-overview.md) | Goals, non-goals, the AudioRelay feature parity list, success criteria |
| [docs/01-architecture.md](docs/01-architecture.md) | Components, audio/data flow, technology choices and why |
| [docs/02-windows-virtual-audio.md](docs/02-windows-virtual-audio.md) | The Windows kernel driver: endpoints, app↔driver interface, signing, install |
| [docs/03-ios-app.md](docs/03-ios-app.md) | iOS audio session, echo cancellation, background audio, App Store |
| [docs/04-networking.md](docs/04-networking.md) | Discovery, pairing, transport (AOO/Opus), security |
| [docs/05-ux-spec.md](docs/05-ux-spec.md) | Every screen, the three-tap setup, settings that survive simplification |
| [docs/06-fork-strategy.md](docs/06-fork-strategy.md) | What to keep, delete and rewrite in SonoBus; file-level plan; licensing |
| [docs/07-roadmap.md](docs/07-roadmap.md) | Phases, deliverables, exit criteria, effort estimates |
| [docs/08-risks-and-decisions.md](docs/08-risks-and-decisions.md) | Risk register and the decisions that still need a human "yes" |
| [docs/09-dev-environment.md](docs/09-dev-environment.md) | Tooling, accounts and certificates needed before Phase 0 |

## Licence

AudioWire is **GPL-3.0-or-later** ([LICENSE](LICENSE)) **with the same App Store additional
permission SonoBus uses** ([LICENSE_EXCEPTION](LICENSE_EXCEPTION), GPLv3 §7). SonoBus is GPLv3
with that exception; its bundled JUCE 6 fork is GPLv3; AOO is BSD-3-Clause. See
[docs/06-fork-strategy.md](docs/06-fork-strategy.md#licensing) for the details and the one open
legal question.

## Credit

AudioWire is based on [SonoBus](https://github.com/sonosaurus/sonobus) by Jesse Chappell
(Sonosaurus LLC), and uses [AOO](https://github.com/Spacechild1/aoo) (Audio over OSC) and the
[Opus](https://opus-codec.org/) codec.
