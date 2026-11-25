# Download a VM node and compile it to native UEEC

The loopback server provides a 65-byte compact UEEC file at `/nodes/call`.
It contains one VM program with a forward call and a nested backward call.
Each client invocation fetches the same bytes, runs the stage 18 compiler
for its target, and writes the generated CODE to an independent single-node
UEEC file using the stage 16 writer. The node records the CPU, code byte
order and Linux execution environment. Its container metadata uses
little-endian SH16 even when its native code is big-endian.

The compiler applies the PC-relative call fixups before writing CODE. Both
calls stay inside the same CODE section, so their displacements also work
when a fresh process loads the completed file at a different address.
The test program uses a small instruction subset with one CODE section.

After shutting down the server, the harness checks that the same URL is
unreachable. It then starts the appropriate offline loader as a separate
process for each native file. The loader checks the node's CPU and Linux
metadata, maps the generated CODE separately with read-execute permissions,
unmaps the source file and executes the machine code. With multiple targets
selected, each loader also rejects a different target's file.

## Reproduce on Linux x86-64

Use Python 3, Make and a host C compiler for the interpreter, Clang 19
and LLD 19, SPARC binutils, and QEMU user-mode executables for the eleven
foreign targets. Run from the archive root:

```sh
python3 19_ueec_client_compile/run_demo.py --isolate-network
python3 19_ueec_client_compile/run_demo.py --isolate-network --status 173
```

The default status is 42. Each complete run ends with `12/12 targets passed`.
The server and HTTP clients share the x86-64 host. The target compiler and
offline loader for x86-64 run directly. Their equivalents for the other
targets run under QEMU user mode.

For a shorter run, repeat `--arch` for the targets you want to select.
`--clang`, `--lld`, `--sparc-ld` and `--qemu-dir` accept alternate tool
locations. If local policy restricts unprivileged network namespaces,
omit `--isolate-network`. The server shutdown and failed connection checks
still run. The harness writes a separate `call-TARGET.ueec` file per target
in the stage's `build/` directory (or the directory set by `--out`).
See `results.txt` for both runs.

The offline reader in this stage intentionally accepts only the compact
one-CODE Linux profile used by the experiment. The original load-and-execute
mode remains available through the stage 18 compiler. Separate DATA sections,
imports, and on-disk relocation entries are outside this experiment's scope.
