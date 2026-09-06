# AGENTS.md

## Project
Zephyr/NCS firmware for Tiresias DK.

## Agent Rules
- Do not create or add tests in this repository, including host-side tests,
  mocks, shims, or test harnesses. Use compile-only builds and the inspection
  checks listed below for verification.
- Compile-only builds are allowed to verify that code compiles without errors.
- Never flash, program, recover, reset, or debug a device, and never run hardware tests.
- Keep changes scoped to the requested files/behavior.
- Prefer compile-time/devicetree configuration over runtime parsing when simple.
- Preserve existing Zephyr style and error handling patterns.
- Do not rewrite public APIs unless explicitly requested.
- Do not revert unrelated worktree changes.
- Leave `src/codec/hw_codec.c` unchanged. It is the legacy, tested, known-good
  hardware-codec implementation and must be used only as a behavioral reference.
- Implement new hardware access through `src/codec/codec_adapter.c`, using
  `hw_codec.c` as the reference implementation. The adapter is intended to
  replace `hw_codec.c` completely once it is implemented and validated.

## DSP Parameter Proof of Concept

- The current DSP parameter scope is only BLE communication with the workstation
  and per-parameter persistence in internal flash.
- Keep `codec_contract`, `codec_values`, `codec_parameters`, and
  `codec_settings` small, single-purpose, and independent of codec hardware. Do
  not add codec communication during this stage.
- Prefer the simplest compile-time representation with a low RAM footprint. Avoid
  dynamic allocation, general-purpose abstractions, and speculative extension
  points.
- Implement only validation and recovery required for the proof of concept to
  operate safely. Record deferred robustness or hardware integration with a
  concise `TODO` comment instead of adding complexity now.

## Useful Checks
Allowed:
- `git diff --check`
- `clang-format -i <edited files>`
- `rg`/`sed`/`git diff` for inspection

Run commands that need the NCS environment through `scripts/ncs-env`. To compile
the existing build configuration as a verification check, run:

```sh
scripts/ncs-env west build -d build
```

Use `scripts/ncs-env west build -d build -p auto` when configuration-sensitive
changes require CMake to determine whether a pristine rebuild is necessary. Run
`scripts/ncs-env` without arguments to open an interactive shell in the repository
with the matching SDK and toolchain.

Strictly prohibited:
- Flashing or programming commands, including `west flash`, `nrfjprog`, and
  `nrfutil device program`.
- Device recovery, reset, erase, or debug commands.
- Hardware tests or any command that communicates with attached hardware.
