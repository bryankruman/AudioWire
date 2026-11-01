# AudioWire third-party notices

AudioWire app/core modifications are GPL-3.0-or-later with the App Store additional permission in `LICENSE_EXCEPTION`.

The AOO integration is adapted from SonoBus by Jesse Chappell, Sonosaurus LLC. SonoBus is GPL-3.0-or-later with an App Store additional permission. Dependencies are retrieved from the pinned sources documented in `docs/BUILDING.md`; their own licence files and notices remain in their source distributions.

| Component | Authors/source | Licence |
|---|---|---|
| SonoBus integration/reference | Jesse Chappell / Sonosaurus LLC | GPL-3.0-or-later + additional permission |
| JUCE 6 SonoBus fork | Raw Material Software / essej | GPLv3/commercial (GPL path used); some modules ISC |
| AOO SonoBus fork | Christof Ressi, Winfried Ritsch and contributors | BSD-3-Clause |
| oscpack (inside AOO) | Ross Bencina | MIT |
| MD5 helper (inside AOO) | Alexander Peslyak | Public domain / permissive fallback |
| Embedded image/compression libraries | libpng, Independent JPEG Group, zlib authors | Retained texts under the generated licenses directory |
| MinGW-w64 and GCC runtime (portable development binary) | MinGW-w64 contributors / Free Software Foundation | Runtime notices and GCC Runtime Library Exception 3.1 |
| Opus | Xiph.Org, Skype and contributors | BSD-3-Clause |
| libsodium | Frank Denis and contributors | ISC |
| mdns | Mattias Jansson and contributors | Public domain |
| QR-Code-generator | Project Nayuki | MIT |
| Trimmed virtual audio driver reference | VirtualDrivers / MikeTheTech | MIT; Microsoft-derived parts MS-PL |
| Microsoft SysVAD-derived source | Microsoft Corporation | MS-PL |

The driver reference's full MIT licence and Microsoft Public License notices are retained under `driver/vendor/`. New files under `driver/` and the SetupAPI helper are MIT. The kernel driver is a separate program from the GPL application.

Binary distribution must provide the complete corresponding application source and the pinned dependency sources, licence texts and required notices. The small notice table in this repository does not replace those source/licence obligations. The installer is a packaging definition, not proof that an App Store, JUCE licensing question or driver-signing gate has been resolved.

CMake copies the required upstream licence texts and module notices to `build/<target>/licenses`; the installer and CI artifact include that directory. The locally built GCC binary also includes MinGW-w64 runtime notices and the GCC Runtime Library Exception.

The AudioWire App Store permission does not relicense JUCE or other third-party code. Resolving a compatible JUCE licence for the store build is a release gate.
