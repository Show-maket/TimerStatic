# HardwareTorture DFU-safe build provenance

This record describes the software-only validation build prepared on
2026-09-02. No serial port, DFU device, or other hardware was opened while
creating it.

## Source identities

- Build identity emitted by the firmware:
  `timerstatic-hwtorture-dfu1` (protocol 2).
- TimerStatic harness parent:
  `84ad1a10b9c8cff6fae7b7e0ee5c660791edb256`.
- TimerStatic production implementation actually under test:
  `c28bd1be0e97e9c10474b49009dab877838cb19c` (the hardware harness and
  DFU-safety changes do not modify `src/`).
- Installed DfuGate library:
  `04754dd4f7bbb47c7250aaf84633e235ac0f6a40`, clean `main` worktree.
- Arduino STM32 core: `2.8.1`.
- Arduino CLI: `0.35.3`, commit `95cfd654`.
- Compiler: xPack GNU Arm Embedded GCC `13.2.1 20231009`, package
  `13.2.1-1.1`.

The firmware identity emits these separately as `timerstatic_code` and
`harness_base`; it does not mislabel the harness commit as library code. The
final TimerStatic proof commit is reported alongside the artifact. It cannot be
embedded in the commit that defines the firmware without creating a
self-reference; the immutable protocol/build identity above is embedded in the
binary and checked by the host reference.

## Exact target

```text
STMicroelectronics:stm32:GenG4:pnum=CAR_G491KEUX_V4_0,F_CPU=PLL_SYS_12__HCLK_12,upload_method=dfuMethod,xserial=generic,usb=CDCgen,xusb=FS,opt=o3std,dbg=none,rtlib=nano
```

Both explicit library paths from `HARDWARE_TORTURE.md` were supplied. Arduino
CLI reported TimerStatic from this proof worktree and DfuGate from
`C:\Users\User\Documents\Arduino\libraries\DfuGate`.

## Verification

- Host reference: 19/19 cases PASS, 948 checks, exact identity accepted.
- G491 compile: PASS.
- Flash: 53,880 bytes reported by Arduino CLI (10%).
- Globals: 5,084 bytes reported by Arduino CLI (4%).
- Embedded strings independently found in the BIN: build identity, complete
  DfuGate revision, protocol-2 START, and `TS,DFU,REQUEST,ACCEPTED`.

## Artifacts

Build directory:

```text
C:\Users\User\Documents\Codex\2026-09-02\new-chat\work\timerstatic-proof\build\g491-hardware-torture-dfu1
```

| File | Bytes | SHA-256 |
|---|---:|---|
| `HardwareTorture.ino.hex` | 153061 | `25E19A7E7DF8029DFE7B1AC1213550EE5F1A27E674FC76CB373CE229A58352AC` |
| `HardwareTorture.ino.bin` | 54392 | `D24B0FF2E7867622361B9B3CF2A38E4E46AFFEA0ABDE284515FC0A6707BA23DE` |
| `HardwareTorture.ino.elf` | 99180 | `6B5BA6B34D3E858F3AD766A340E4DFAEDBBEBE73A3E16D0EA0FFA2E7D459F160` |

The STM32 build uses LTO and is path-sensitive: a clean build in a different
absolute build directory produced the same memory usage but different binary
hashes. The table is therefore tied to the exact build directory and command
above. Rebuilding with `--clean` in that same directory is the hash-stability
check used before hardware flashing.
