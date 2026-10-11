# Host verification — 2026-10-12

Environment: Windows, Python 3, MinGW GCC/G++; not an Xbox emulator.

The repository test scripts passed against the committed SS2 source:

```text
PASS: registered data, CPU tags, pre-save/post-load callbacks round trip
PASS: invalid sizes/header/version/ROM/signature/flags/sound and short reads rejected before mutation
PASS: allocation failures, short writes, and recovery after failure
PASS: legacy 10-byte ROM-name boundary
PASS: byte-for-byte identical save file to unmodified MAME72 serializer
PASS: four controllers, simultaneous edges, chord edges, 300-frame holds, rearm, both buttons, disconnect, new game/reset
PASS: recursive state directories, unbuffered state writes, flush/write failure and short-write reporting
PASS: favorites persistence/reload, duplicate labels, missing ROM preservation, repeat suppression
PASS: empty filter recovery, selection identity, open/short-write/flush/rename/read failure protection
```

The favorites harness emits a compiler warning because its fake `HXUIOBJ` is an integer initialized with `NULL`. This is a test-adapter warning, not evidence of an XDK source compilation failure.

The original serializer reference is pinned to upstream commit `63a21cfce35d42333885031cf2fe221808c44016`; it does not follow this fork's HEAD.

Xbox save/load success was reported by the user. The played game was subsequently identified as Shinobi bootleg. No comprehensive driver compatibility test, independent audio comparison, or explicit on-console favorites persistence result has been reported. No sound fix is included.
