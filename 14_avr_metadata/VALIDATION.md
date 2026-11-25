# AVR virtual execution validation — 2026-11-07

All three requested device profiles were built and actually executed. This is
simulated hardware validation, not a physical-device programming result.

| Model | Signature | GCC core | CODE bytes | Observed edges | Half-period |
| --- | --- | --- | ---: | ---: | ---: |
| ATtiny85 | 1e930b | avr25 | 84 | 8 | 100 ms |
| ATmega328P | 1e950f | avr5 | 164 | 8 | 100 ms |
| ATmega2560 | 1e9801 | avr6 | 290 | 8 | 100 ms |

Toolchain: Debian 13, host GCC 14.2, AVR GCC 14.2, AVR binutils
2.43.50.20250108, avr-libc 2.2.1, simavr 1.6 (Debian 1.6+dfsg-3+b3).

`make test-all`: **76 selection/format/simulation checks PASS**, no skipped models.
The firmware was rebuilt for each model, including its actual reset vectors
and C startup, rather than reusing host code or one payload with different names.

The same **76 checks** also passed with the host loader compiled under
AddressSanitizer and UndefinedBehaviorSanitizer. The external simavr shared
library itself was not rebuilt with instrumentation. LeakSanitizer cannot run
under the environment's ptrace supervision and was disabled; no leak-test claim.

## Independent observations

The loader reads the configured simavr MCU's actual signature and memory/PC
properties. Metadata is derived separately from AVR compiler device headers.
The selected raw CODE is copied to virtual Flash address zero and byte-compared.
The UEEC input buffer is freed before AVR instructions execute.

GPIO IRQ notifications are read from the emulated output port, with DDR output
direction checked. Eight alternating ON/OFF transitions are verified per model;
their cycle differences match the declared half-period within 1% plus a small
instruction-overhead allowance. VCD records are generated from those actual
notifications. The Python test independently checks sequence, cycle timing,
VCD ordering and exact extracted CODE bytes.

Evidence: `validation/test-all.log`, `validation/sanitizers.log`, and the
model-specific logs, VCDs and `results.json` under `validation/results/`.
Convenience copies of normal traces are in `examples/traces/`.

## Coverage

- Exact device signature, core, PC/SP, Flash/SRAM/EEPROM and configured clock.
- Required ISA feature mask; unsupported/unknown requirements rejected.
- Three-node shared MD, twelve typed records, fixed uint16 node indexes.
- Both container byte orders and all three MD count/offset widths; reversed nodes.
- Real selection of node 256, after 256 incompatible-device nodes.
- Wrong-device-only file and wrong virtual clock rejected before flashing.
- Invalid node index, duplicate record, unknown type, wrong design/format,
  invalid offsets/sentinel/count, odd CODE size, invalid node class and truncation.
- Huge count values tested in MD16/32/64 with both byte orders.
- Target identity/core/PC/SP/memory mismatches rejected.
- A deliberately wrong GPIO pin fails by lack of real transitions.
- A deliberately wrong period fails by observed timing mismatch.
- Existing trace/extraction files are not overwritten.

## Limits

There is no physical AVR, ISP/STK500/Optiboot protocol, fuse or EEPROM programming,
or analog LED circuit simulation in this result. The virtual LED is the observed
digital GPIO level. The simulator's profile is configured by `--mcu`; the UEEC
node is then chosen automatically using signature and metadata.

The instruction mask is a conservative compiler-target profile, not a minimal
instruction inventory recovered from the binary. Blink does not exercise every
advertised instruction group or the high end of ATmega2560 Flash. Only these
three MCU models are supported by this example's target-profile catalog.

The generator uses SH16. A full 256 KiB CODE payload is not tested; the large
device's capacity in metadata must not be mistaken for that coverage.
