# UEEC METADATA section — draft design (not yet implemented)

**Status:** approved data-layout direction, pending serialization and implementation. This document is a proposal; it is **not** evidence that a METADATA section already exists in the published loader.

The section uses the same initial two-field order as RELOC:

```c
typedef struct {
    uint16_t format;
    uint16_t design;
} ueec_md_hdr;
```

`design` selects a metadata storage model. The first proposed model is an indexed record table. `format` selects 16-, 32-, or 64-bit widths for both record count and each offset.

```c
typedef struct { uint16_t count; } ueec_md_count16;
typedef struct { uint32_t count; } ueec_md_count32;
typedef struct { uint64_t count; } ueec_md_count64;

typedef struct { uint16_t type; uint16_t offset; } ueec_md_entry16;
typedef struct { uint16_t type; uint32_t offset; } ueec_md_entry32;
typedef struct { uint16_t type; uint64_t offset; } ueec_md_entry64;
```

Binary layout:

```
  [format: u16][design: u16]
  [count: u16/u32/u64 according to format]
  [entries: count + 1 elements, each (type:u16, offset:u16/u32/u64)]
  [record_data: contiguous bytes without embedded type or length fields]
```

For record i (0 <= i < count):
- `type = entries[i].type`
- `start = entries[i].offset`
- `end = entries[i+1].offset`
- `length = end - start`
- `value = record_data + start`

The sentinel entry at index `count` has `type=0` and its offset equals the record-data area length.

**Wire-format requirements:** explicit byte order; packed serialized field sizes (4-byte header, entries of 4/6/10 bytes, not native sizeof(C struct)); nondecreasing offsets; first offset 0; final offset equal to data byte count; checked arithmetic for `count+1`, multiplication and bounds. Unknown optional metadata types should be safely skippable. Values and type IDs are not finalized, and compatibility with existing UEEC readers needs testing.

The planned METADATA section is informational/optional. It must not become a prerequisite for loading older UEEC files. Reader, writer, info, converter, extract, and TLS paths need synchronized changes when this draft is implemented.
