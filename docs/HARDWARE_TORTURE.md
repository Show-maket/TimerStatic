# TimerStatic hardware torture proof

`examples/HardwareTorture/HardwareTorture.ino` is a destructive-list and
time-boundary test of the current TimerStatic implementation. It does not
change library behaviour and does not access private list fields.

## Covered contracts

- zero-period policy: `delay(0)` fires once; periodic/finite zero-period
  configurations remain inert;
- callback mutation: self-rearm, `resetToEnd()`, active `std::function`
  replacement, and self-delete;
- intrusive-list mutation: delete next/head/tail, create during callback, and
  defer newly-created timers to the next global pass;
- move construction, move assignment, and move assignment of the timer whose
  callback is currently active;
- recursive `Timer::tick()` suppression;
- `OFF()`, `ON()`, `restart()`, `setPeriod()`, and `restartWithPeriod()`;
- late/catch-up semantics: at most one callback per check and phase-preserving
  O(1) deadline advance;
- `forCount`, `forTime`, `isForLast()`, and extension from the last callback;
- injected `millis` and `micros` wrap at `2^32`;
- 96 simultaneous timers, exact visit order, exact counts, and guarded memory;
- a final smoke test against the board's real `millis()` and `micros()`.

All loops have both a time limit and/or an iteration limit. The STM32 build
starts an 8-second independent watchdog if one is not already enabled, feeds it
between bounded operations, and keeps feeding it after the terminal record.
If list corruption hangs inside `Timer::tick()`, the watchdog resets the board;
repeated `TS,SUITE,START` without `TS,SUMMARY` is therefore a machine-visible
failure. The suite emits no per-event trace, only 19 bounded case records, one
summary, and `TS,SUITE,STOP`.

## Host reference

The host runner compiles the exact `.ino` test body with a minimal Arduino time
and serial shim, kills it after 15 seconds, and validates all terminal records:

```powershell
& .\test\host\run_reference.ps1
```

Success ends with:

```text
TS,SUMMARY,PASS,passed=19,failed=0,...
TS,SUITE,STOP
TS,HOST,PASS,cases=19,timeout_ms=15000
```

## Black car G491 compile command

Run from this worktree. `--library` is mandatory: it makes Arduino CLI use this
checkout instead of another `TimerStatic` installed under the sketchbook.

```powershell
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile `
  --fqbn 'STMicroelectronics:stm32:GenG4:pnum=CAR_G491KEUX_V4_0,F_CPU=PLL_SYS_12__HCLK_12,upload_method=dfuMethod,xserial=generic,usb=CDCgen,xusb=FS,opt=o3std,dbg=none,rtlib=nano' `
  --library 'C:\Users\User\Documents\Codex\2026-09-02\new-chat\work\timerstatic-proof' `
  --build-path 'C:\Users\User\Documents\Codex\2026-09-02\new-chat\work\timerstatic-proof\build\g491-hardware-torture' `
  'C:\Users\User\Documents\Codex\2026-09-02\new-chat\work\timerstatic-proof\examples\HardwareTorture'
```

This command targets the black-car v4.0 variant (`CAR_G491KEUX_V4_0`), 12 MHz,
generic USB CDC, DFU upload method, `-O3`, no debug runtime, and nano libc.
Compilation does not flash the board.

## Hardware capture and verdict

1. Open the USB CDC port before reset, or within the 1.5-second connection
   window. Baud is recorded as 115200 but is not timing-significant for USB CDC.
2. Capture from the first `TS,SUITE,START` through `TS,SUITE,STOP` without
   editing the raw bytes.
3. PASS requires exactly 19 `TS,CASE,...,PASS` records, zero case FAIL records,
   one PASS summary, and one STOP record from the same boot.
4. A reset, duplicate START, missing terminal line, timeout, or canary failure is
   FAIL, even if earlier cases passed.
5. Record firmware SHA-256, board/car identity, core version, full FQBN, compiler
   output, reset cause if available, and the raw CDC log with its SHA-256.

The sketch deliberately performs no motor, IR, I2C, EEPROM, DFU, or network
operation. Flashing and execution are separate, explicitly authorized hardware
steps.
