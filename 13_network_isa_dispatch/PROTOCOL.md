# UEEC architecture/OS/ISA delivery protocol, UEC2

All frames travel inside an authenticated TLS 1.2+ connection. The client
verifies the server certificate chain and DNS/IP identity. One request and
one response are exchanged per connection. Numeric frame fields are big-endian;
reserved bytes must be zero. These frames are independent of the source UEEC
file's byte order and metadata integer width.

## Request: exactly 64 bytes

| Byte offset | Size | Meaning |
| --- | ---: | --- |
| 0 | 4 | ASCII `UEC2` |
| 4 | 1 | UEEC CPU machine ID |
| 5 | 1 | UEEC general-OS ID; current automatic client reports Linux = 2 |
| 6 | 1 | Required CODE endian: 0 little, 1 big |
| 7 | 1 | Flags: bit 0 permits automatic fallback; all other bits zero |
| 8 | 8 | OS-usable ISA capability mask in the requested architecture's UEEC bit space |
| 16 | 32 | Technology name: NUL-terminated lower-case ASCII identifier, zero padded; empty means automatic |
| 48 | 16 | Reserved, zero |

The mask uses the same UEEC-defined family bit numbers as metadata, not raw
CPUID or AT_HWCAP bit numbers. A fallback flag without a technology is invalid.
The request identifies a fixed demo application/ABI: Linux CPU user/library
`sum`, with the same per-architecture ABI as the stage-12 loaders. There is no
general application-name or arbitrary ABI negotiation in this prototype.

The server validates all source metadata before listening. It filters nodes by
machine, OS, CODE endian, application/ABI and `(required & ~usable) == 0`.
Without a requested technology, the compatible node with highest metadata
priority is selected. An explicit technology is honored or rejected; fallback
allows automatic selection when the request is unavailable. Ties select the
earliest node in source order. Advertised capabilities are client input, not
hardware attestation.

## Response header: exactly 64 bytes

| Byte offset | Size | Meaning |
| --- | ---: | --- |
| 0 | 4 | ASCII `UEC2` |
| 4 | 4 | Status: 0 success, 1 bad request, 2 no compatible node, 3 requested technology unavailable, 4 internal error |
| 8 | 4 | Compact UEEC payload length, max 64 MiB; zero on failure |
| 12 | 2 | Original source node index, for diagnostics |
| 14 | 2 | Reserved, zero |
| 16 | 8 | Selected node's required ISA mask |
| 24 | 2 | Selected node's priority |
| 26 | 6 | Reserved, zero |
| 32 | 32 | SHA-256 of the complete compact UEEC payload |

On success, exactly `payload_length` bytes follow. No payload follows a
failure. The compact payload contains one node at index zero, one CODE and
one shared design-1 METADATA section; both metadata records target node zero.
Its source machine/OS/ABI and CODE endian are preserved. Its outer structure
is canonical LE SH16 and its MD is LE MD16. This output normalization does not
rewrite machine instructions.

The receiver checks the digest and the file's own metadata against the request
and the response header. It validates again after compact/granulated local
repacking and creates the output exclusively. TLS/digest/framing/format/
compatibility failures create no output file. Downloading never executes CODE.

This is a bounded demonstration, not a stable public protocol registry.
It is incompatible with the older UEC1 8-byte architecture-only request.
