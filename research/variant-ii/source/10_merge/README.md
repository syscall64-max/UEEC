# Stage 10.4 — Merge utility

Run `make x86_64`, `make all`, or `make sanitize`. Each architecture runs the existing 105 checks and 72 new merge checks (36 executions, 36 rejection cases). The utility is Python 3 and runs on the host; loader is unchanged from Stage 10.3.

Example after build:

```
./ueec_merge.py combined.ueec build/x86_64/f16-le/program.ueec build/x86_64/f16-le/libmath.ueec build/x86_64/f16-le/libdata.ueec
build/x86_64/loader combined.ueec
```

Output must not exist. Partial merge is supported; leave unmerged libraries beside the output. Matching external filenames are mapped to single-node library names. Container section width is the widest input width, widened if necessary. Local indexes, resident bytes and exports are preserved. The import name pool is rebuilt with adjusted offsets. Inputs must have common wire endian; no shared sections, granulation, relocations, recursive library dependencies or general multi-program linking in this experiment. This is not a full security validation of arbitrary executable files. Native x86-64: 105 prior checks and 72 new checks passed; separate CLI smoke passed. Other targets and sanitizer are not yet verified for this stage.
