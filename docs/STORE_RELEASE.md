# Store and signed-release preparation

This document is a preparation checklist for the implemented pre-alpha. It is not an App Store submission or Microsoft approval.

Use the release gates in IMPLEMENTATION.md before producing a public v1. The AudioWire App Store permission covers AudioWire-owned contributions; it does not grant additional rights to JUCE or independently licensed dependencies.

The intended product description is: AudioWire turns an iPhone into a speaker and microphone for a Windows PC on the same Wi-Fi. The Windows installer provides the AudioWire audio devices. The phone selects the PC, pairs with the locally shown code, and offers speakerphone, speaker-only and microphone-only modes.

Store screenshots and a demo video must come from the validated device build. Do not submit the Windows-rendered phone preview as a physical iPhone screenshot or claim measured echo/latency results that have not been observed.

## Privacy behavior to review against the final binary

AudioWire's own source has no analytics, ads, accounts or public connection relay. PC and phone audio travels through the encrypted paired connection. Local discovery advertises a device identity, display name and listening ports. Settings retain names, identities, addresses and preferences on the device; pairing keys use DPAPI/Keychain. Forget removes the saved key and device record. Diagnostics contain connection state and aggregate audio statistics.

The native iOS build needs its final privacy manifest, required-reason API audit (including the pinned JUCE implementation), privacy policy URL, local-network/microphone descriptions, background-audio review notes and the App Store privacy declarations. Select declarations from the audited final binary and actual data handling, not from this description alone.

## Signing and review material

Use protected organisation credentials outside the repository. Configure the actual Apple development team, provision the intended bundle identifier, produce a signed archive and test it through TestFlight. Provide the reviewer with a validated Windows installer, setup instructions, working pairing flow, and an actual device demo.

Build and validate the Windows driver with the supported WDK/MSVC path, then obtain the required Microsoft kernel signing approval. Verify the app and catalog signatures with the release packaging script, build the installer, sign the completed installer and verify that signature. Test installation and uninstall on a clean VM with the intended Windows versions and elevation paths.

Keep the exact corresponding source archive and dependency notices available alongside each Windows binary and through the source channel required by the project's licence. Publish store listings and release claims only after the device evidence and signing/licensing gates are resolved.
