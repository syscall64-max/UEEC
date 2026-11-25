# STM32 validation scope

The functional suite runs the C UEEC selector and real Renode CPU, NVIC,
SysTick, GPIO and LED models. It does not simulate GPIO events in host Python.
Firmware uses memory-mapped BSRR writes and a SysTick interrupt with WFI.

## Environment

- Linux x86-64; Python 3.12.14.
- Native C: Debian GCC 14.2.0, strict warnings including `-Werror`.
- Bare-metal ARM: GCC 14.2.1 20241119; binutils 2.44.
- Official Renode 1.17.0 portable, build `20260907gitf1dd1b4af`, .NET 8.0.12.
- Portable archive SHA-256:
  `4ba7c68b59e2447f188ef4b4b112fcccd2802582c460beedceb63b84e5605a5f`.

## What constitutes a check

The normal suite contains **95 counted checks**: 87 selector subprocess
invocations and 8 Renode scenarios. Additional assertions about raw extraction,
the manifest, trace counts and timing are mandatory but are not separately
inflated into the check count. The sanitizer target reruns these same 95 checks;
it is not 95 additional independent feature tests.

The 87 C checks cover:

- Three model selections for every combination of outer little/big endian and
  metadata count/offset width 16/32/64 bits.
- Reversed node order, single-node files, missing compatible model, listing,
  selected raw CODE extraction, and refusal to overwrite an existing output.
- A 257-node carrier that selects **node 256**, verifying the node index is not
  accidentally narrowed to 8 bits.
- Wrong model/core, Flash/RAM geometry, clock, GPIO routing or required features.
- Invalid metadata formats, designs, lengths/counts, node indices, record types,
  offsets, sentinel values, and malformed foreign-node metadata.
- Invalid initial SP and reset vectors, truncated containers and invalid device
  profile fields.

The 8 Renode scenarios are:

1. F103RB: select node 0, write/read back 252 CODE bytes, execute 8 GPIO edges.
2. F407VG: select node 1, write/read back 256 CODE bytes, execute 8 GPIO edges.
3. F746NG: select node 2, write/read back 248 CODE bytes, execute 8 GPIO edges.
4. Big-endian container, 64-bit metadata offsets, reversed nodes: execute
   another 8 GPIO edges on F407VG.
5. No-run inspection: probe and select, with no Flash write or execution.
6. Node listing: probe and report compatibility, with no Flash write/execution.
7. Incompatible container: reject before any Flash write.
8. Deliberately false 200 ms metadata for 100 ms firmware: reject based on
   actual observed LED timing.

The three primary traces contain **24 GPIO transitions**; the endian scenario
adds 8. LED states alternate, and consecutive transitions must be within 2%
plus 100 microseconds of the metadata half-period. The Renode LED tester also
checks blinking. The output marker alone is insufficient: the host rejects
Renode error diagnostics and independently validates the event trace.

Renode can return exit code zero for a monitor-command error. The bridge requires
an explicit success marker, checks error messages, and fails on absent or
malformed events. Callbacks catch their own errors to avoid propagating a host
exception through the emulator's event thread. Each run has a private TMPDIR.

## Sanitizer boundary

`make test-sanitize` instruments the native **C parser/selector/extractor** with
AddressSanitizer and UndefinedBehaviorSanitizer. All synthetic profile cases and
the C selector subprocesses inside the Renode scenarios use that executable.
Renode, the .NET runtime and guest firmware are not ASan/UBSan-instrumented.
The harness defaults `ASAN_OPTIONS=detect_leaks=0` because leak reporting is not
part of this parser-boundary check.

## Target identity and hardware limits

The STM32 model is selected as a virtual platform using `--mcu`. CPUID part,
mapped Flash/RAM sizes and NVIC frequency are read inside that machine. Exact
SKU identity, the capability catalog and LED routing are configured platform
facts; CPUID does not uniquely identify an STM32 SKU. Synthetic profiles are
only unit-test fixtures, and are never presented as hardware observations.

The platforms are deliberately limited to this blink demonstration. F103 RCC
is an enable-register shadow block. F4/F7 reuse Renode's F4 RCC model. Full PLL,
clock gating, caches, bus timing and every MCU peripheral are outside the test.
F407 main SRAM is 128 KiB; its separate 64 KiB CCM is omitted. F746's 320 KiB
RAM is modeled contiguously, without TCM/cache timing distinctions. All three
firmwares require Thumb-2 and use soft float; DSP/FPU/cache capability flags do
not imply those facilities were executed by this test.

Virtual Flash is populated directly using the Renode system-bus API and read
back byte for byte. No Flash-controller erase/program algorithm, ST-Link,
physical MCU, voltage, wiring or electrical behavior was tested. This stage
does not implement the stage-13 TLS transport or merge AVR and STM32 into one
carrier; it demonstrates the same shared metadata design for three STM32 nodes.

## Reproduction and evidence

Run `make test-all` to rebuild all three firmwares and rerun every check.
Run `make test` to use the included checksummed payloads.
Logs are in `validation/test-all.log` and `validation/test-sanitize.log`.
The included `examples/traces/` holds actual Renode JSON reports and VCDs;
`examples/blink.ueec` is the default 1070-byte carrier. Current run results go
to `build/results/`. No simulator target is skipped.

This additional demonstration was produced with AI assistance. The UEEC
reference parser and format are from George Modebadze's MIT-licensed project.
