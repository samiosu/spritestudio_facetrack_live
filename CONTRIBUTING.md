# Contributing

Changes and bug reports are welcome. Run the native/Python tests and the affected
mobile project's tests before proposing a change. Keep protocol changes versioned
and compatible across all producers. Do not commit models, signing keys, session
tokens, build outputs, or assets you cannot redistribute.

New dependencies must have a pinned version, their original license/notice files,
and an entry in `docs/DEPENDENCIES.md` and `licenses/manifest.json`. GPL/LGPL/AGPL
dependencies are considered according to how they are combined and distributed;
they are not rejected merely because they use copyleft.

By submitting your own contribution, you agree to license it under the license
applicable to the changed files: MIT for the application and demo, and
GPL-3.0-or-later for `tools/openseeface_adapter.py`, `tools/send_demo.py`, and
`tests/test_adapter.py`. Preserve the licenses and copyrights of third-party code.
