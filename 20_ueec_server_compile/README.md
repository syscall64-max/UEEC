# Server-side VM compilation for a client's Linux target

The server reads a compact UEEC VM file from disk when it starts. A client
requests the `call` node with an OS, ABI, architecture and list of usable CPU
features. The server runs the target emitter on the VM bytecode for each
request and returns a compact, single-node UEEC containing native CODE. The
server and client are separate processes on the same x86-64 host. The server's
emitters for the other eleven architectures run under QEMU user mode.

The client verifies the target and feature metadata in the compact response.
It can save those bytes directly or pad all physical sections to a local
granule. A 4096-byte granule is used in the matrix. The client changes the
stored section spans and adds padding while preserving the meaningful CODE
and feature metadata. The granule is purely local and never enters the
request. The offline loader reads either layout.

The test requests the baseline profile for all 12 Linux CPU targets and
SSE2 and AVX2 profiles for x86-64. For the latter two, the emitter deliberately
uses SIMD instructions to lower the VM's immediate register assignment.
Both profiles compute the same value. The comparison tests instruction
selection, with no performance claim. An indexed METADATA section records the
required features for SSE2 and AVX2. The offline x86-64 loader evaluates the
required bits and, for AVX2, checks the OS-enabled XMM/YMM state. The harness
also disables AVX2 in QEMU and verifies that execution is rejected before
entry.

## Reproduce on Linux x86-64

Use Python 3, Make, a host C compiler, Clang 19 and LLD 19, GNU SPARC binutils,
and QEMU user-mode executables for the foreign targets. The mismatch check
also uses `qemu-x86_64`. From the root of the archive:

```sh
python3 20_ueec_server_compile/run_stage20.py --isolate-network --status 42
python3 20_ueec_server_compile/run_stage20.py --isolate-network --status 173
```

The harness builds the tools, starts the HTTP server on loopback, fetches
both layouts under each of the 14 profiles, and verifies that the two
responses received for every profile had the same compact hash. Then it
stops the server, verifies a connection failure, and runs both saved files
in fresh network namespaces. On systems that restrict unprivileged
namespaces, omit `--isolate-network` to run only the server shutdown and
connection checks. See `results.txt` for the full results.

The fixture uses the existing VM's CALL, MOV, RET and SYSCALL 0 subset.
The two CALLs are relocated within CODE on the server. The experiment
covers the generated single-node profile and Linux ABI. SSE2 and AVX2 are
the two non-baseline code-generation profiles implemented here. The harness
supplies explicit feature lists, and the offline loader verifies them on
the execution CPU.
