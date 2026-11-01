# Contributing

Read the numbered product plan and `docs/IMPLEMENTATION.md` first. AudioWire v1 is a LAN-only, one-PC/one-iPhone product. Keep internet relays, additional platforms, multi-peer mixing and music-collaboration features outside this workstream.

Build and run CTest before submitting changes. Add meaningful tests for protocol parsing, security boundaries, thread ownership, reconnect behavior and audio-format changes. Do not use passing portable tests as evidence of kernel or physical iPhone validation.

Keep `core/` free of JUCE and platform headers. The audio callback must not allocate, perform network/file operations, or acquire an application mutex. Network processing, encryption, framing and platform persistence belong on their own threads. Retire transports only after the callback has stopped using them.

Use protocol versioning and reject malformed inputs before parsing them into AOO. Never log audio, pairing codes, pair/session keys, certificate credentials or provisioning material. Driver tests happen in a snapshotted VM.

Application/core contributions use GPL-3.0-or-later with the additional permission in `LICENSE_EXCEPTION`. New driver helpers are MIT; preserved Microsoft source remains MS-PL. Keep the original authors' notices in retained code. CI artifacts are development candidates until their release gates are satisfied.
