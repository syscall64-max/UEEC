# UEEC selected-image loading benchmark

This experimental stage measures the **stage-16 loader**, using its exact
parser and allocation code. It does not run VM bytecode or compare UEEC with
ELF/PE/Mach-O. No container format, loader or VM ABI changes are introduced.

## Run

Linux, a C11 compiler, GNU Make and Python 3.10+ are required; no Python packages,
NASM, QEMU or privileged actions are needed. Keep stages 16 and 17 adjacent.

```sh
cd 17_ueec_bench
make test
make quick       # 24 configurations, 480 measured samples; plumbing/pilot run
make bench       # 72 configurations, 43,200 measured samples
```

Outputs in `build/results/`:

| File | Contents |
| --- | --- |
| `samples.csv` | Every raw phase sample, case, round and randomized order |
| `summary.csv` | Per-case median times in microseconds and allocation counters |
| `summary.json` | Full metadata, per-phase minimum/median/p95/maximum, per-round load medians, environment and hashes |
| `environment.json` | CPU, memory, OS, virtualization evidence, toolchain, binary hash and before/after observations |
| `fixtures/*.ueec` | Exact generated workload files; hashes in the report |

Each run overwrites these report names. Use distinct output directories for
independent runs:

```sh
python3 benchmark.py --output build/run-01
python3 benchmark.py --output build/run-02 --cpu 0 \
  --environment-note 'Host details supplied by the operator: VirtualBox version ..., assigned RAM ..., host RAM type/speed ...'
```

`--cpu` is optional and must name an allowed logical CPU. It only changes this
runner's affinity, inherited by its children. The report distinguishes operator
notes from detected evidence. Do not put private information in notes you share.

## Environment is part of the result

- **CPU:** visible model/vendor/features, logical CPU count, allowed affinity,
  topology/cache details from `lscpu --json` when available, a representative
  CPU's caches and available frequency/governor snapshots. These are guest-visible
  observations; frequency is not locked and physical host topology may be hidden.
- **Memory:** OS-visible RAM total/available, swap total/free, page size, and
  visible cgroup-v2 memory/CPU limits when their layout is supported. Before/after
  snapshots include available RAM and load averages. OS-visible RAM can exceed a
  container's usable allowance; neither is presented as physical host capacity.
  DIMM type and speed are `null` unless supplied separately in the operator note.
- **OS:** distribution/release, kernel, architecture and Python version.
- **Virtual environment:** separate `systemd-detect-virt --vm` and `--container`
  results, public DMI product identifiers if readable, container marker presence,
  and `lscpu` hypervisor evidence. This can identify KVM, VMware, Oracle/VirtualBox,
  Docker or another environment when exposed. A missing detector/result does not
  establish bare metal. Nested host details and hypervisor versions may remain
  unknown; the raw evidence and unavailable status are retained.
- **Build:** compiler/version, recorded optimized build command, binary/source
  SHA-256 hashes, UTC start timestamp, git HEAD when available, clock/resolution.

Use the same machine/guest resources, affinity, toolchain and background workload
for comparisons. Report whether the VM was otherwise busy and any host details
you actually know. This harness observes available settings; it does not change
governors, evict caches, disable services or alter VM resource limits.

## Workload matrix and timing

Default: 1 or 8 nodes; 1 KiB, 64 KiB or 1 MiB INIT_DATA; shared versus duplicated
INIT_DATA; file granules 1 or 4096; monolithic/scattered/hybrid loading. All files
use SH32 and little-endian metadata. Every node refers to one shared four-byte
CODE section containing exit, which is **never executed** here. Each selected
image has identical CODE/INIT_DATA contents and permissions for a given size.
The last node is chosen by explicit numeric index. Only one image is loaded at
a time, so this does not measure concurrent multi-program sharing benefits.

Each configuration gets 20 discarded warmups and 200 samples in each of three
rounds. Configuration order is shuffled per round with recorded seed 20261116.
Warm repeated reads are intentional; no cold-cache or physical-disk I/O result
is claimed. Fixture generation, process startup and CSV printing are outside
the timed regions. File creation before timing can itself warm filesystem caches.

| Phase | Included operations |
| --- | --- |
| `open_map_ns` | Open, fstat and map the input file |
| `parse_select_ns` | Parse/validate metadata and select one node |
| `load_ns` | Build selected runtime image with the explicit allocation policy |
| `input_close_ns` | Unmap/close the input file |
| `release_ns` | Release the runtime image |
| `total_ns` | Sum of these five phases; excludes allocation-counter bookkeeping |

`total_ns` is a sum of measured regions, not process startup time or one continuous
end-to-end interval. Clock calls, boundary checks and normal loader overhead
remain in the measurements. `CLOCK_MONOTONIC_RAW` is used where available,
otherwise `CLOCK_MONOTONIC`; clock resolution is reported, not assumed to imply
measurement accuracy. Raw samples are integer nanoseconds. Median aggregates all
rounds; p95 uses the nearest-rank definition. Per-round load medians help expose
drift. Samples within a process are not independent machine-level replications.

`mapped_bytes` reports page-rounded **runtime virtual allocations**, including
the runtime HEADER. It excludes the input-file mapping, allocator/Python overhead,
and other process memory. It is **not RSS**, peak RAM, physical page usage or a
claim that sharing reduces a selected image's allocations. Mapping counts should
be 1/3/2 for monolithic/scattered/hybrid. No speed threshold determines PASS.

## Validation and limits

`make test` checks allocation modes, selected payload equality, raw phase sums,
invalid inputs and the Python report pipeline. `make sanitize` checks the C
harness with ASan/UBSan; sanitizer timings are not accepted by the runner.
Local default disables leak detection because some supervised environments block
LeakSanitizer. CI explicitly enables leak detection. Do not benchmark sanitized
builds. The harness includes `../16_ueec_vm/ueec_vm.c` with its CLI entry renamed
to reuse its private parser/loader without editing or copying production code.

These are exploratory within-UEEC loading measurements. Interpret differences
against round-to-round variation; repeat on the author's host and additional
machines before general conclusions. Interpreter throughput, native execution,
relocations/linking, TLS transfer, MCU energy/memory behavior, peak RSS and other
format comparisons require separate experiments.

### ქართულად

ჯერ გაუშვი `make test`, შემდეგ `make bench`. შედეგებთან ერთად
`build/results/environment.json` ავტომატურად შეინახავს CPU-ის, მეხსიერების,
ოპერაციული სისტემისა და აღმოჩენილი ვირტუალური გარემოს მონაცემებს.
რაც გარემოდან არ ჩანს, უცნობად დარჩება; დამატებითი ცნობილი ინფორმაცია
შეგიძლია ჩაწერო `--environment-note` პარამეტრით.
იზომება არჩეული ნოდის **ჩატვირთვა და გათავისუფლება**; VM-ის ინსტრუქციების
შესრულების სიჩქარე ამ ტესტში არ იზომება. შეინახე მთლიანი შედეგების საქაღალდე.
