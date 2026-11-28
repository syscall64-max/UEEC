//
// loader.c -- ირჩევს host-architecture-ის შესაბამის node-ს, ტვირთავს
// (--L=monolithic ან --L=scattered), აპლიკაციაშ SECTION_FLAT RELOC-ს,
// უშვებს CODE-ს.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "ueec.h"

#if defined(__x86_64__)
#  define HOST_MACHINE UEEC_MACHINE_X86_64
#elif defined(__i386__)
#  define HOST_MACHINE UEEC_MACHINE_X86
#elif defined(__aarch64__)
#  define HOST_MACHINE UEEC_MACHINE_ARM64
#elif defined(__arm__)
#  define HOST_MACHINE UEEC_MACHINE_ARM32
#elif defined(__riscv) && (__riscv_xlen == 64)
#  define HOST_MACHINE UEEC_MACHINE_RISCV64
#elif defined(__riscv) && (__riscv_xlen == 32)
#  define HOST_MACHINE UEEC_MACHINE_RISCV32
#elif defined(__powerpc64__)
#  define HOST_MACHINE UEEC_MACHINE_POWERPC64
#elif defined(__powerpc__)
#  define HOST_MACHINE UEEC_MACHINE_POWERPC32
#elif defined(__sparc__) && defined(__arch64__)
#  define HOST_MACHINE UEEC_MACHINE_SPARC64
#elif defined(__sparc__)
#  define HOST_MACHINE UEEC_MACHINE_SPARC32
#elif defined(__loongarch64)
#  define HOST_MACHINE UEEC_MACHINE_LOONGARCH64
#elif defined(__s390x__)
#  define HOST_MACHINE UEEC_MACHINE_S390X
#else
#  error "unknown host architecture"
#endif

// ---------------- endian-aware primitives ----------------
static uint16_t rd16(const uint8_t *p, unsigned file_endian) {
    uint16_t v; memcpy(&v, p, 2);
    return (file_endian == UEEC_HOST_ENDIAN) ? v : __builtin_bswap16(v);
}
static uint32_t rd32(const uint8_t *p, unsigned file_endian) {
    uint32_t v; memcpy(&v, p, 4);
    return (file_endian == UEEC_HOST_ENDIAN) ? v : __builtin_bswap32(v);
}
static uint64_t rd64(const uint8_t *p, unsigned file_endian) {
    uint64_t v; memcpy(&v, p, 8);
    return (file_endian == UEEC_HOST_ENDIAN) ? v : __builtin_bswap64(v);
}

static inline __attribute__((always_inline)) void apply_reloc(void **local_base, int target, int source, size_t patch_offset, int type, int trace) {
    void *target_addr = (uint8_t *)local_base[target] + patch_offset;
    unsigned long source_base = (unsigned long)local_base[source];
    if (type == 1) { // ABS64
        uint64_t addend; memcpy(&addend, target_addr, 8);
        uint64_t patched = source_base + addend;
        memcpy(target_addr, &patched, 8);
    } else { // ABS32
        uint32_t addend; memcpy(&addend, target_addr, 4);
        uint32_t patched = (uint32_t)(source_base + addend);
        memcpy(target_addr, &patched, 4);
    }
    if (trace) printf("  RELOC: local[%d]+0x%zx = local[%d] + addend (type=%s)\n",
           target, patch_offset, source, type ? "ABS64" : "ABS32");
}

/* Select width and file byte order once per relocation section. */
#define BLOCK_PROCESS(W, T, SHIFT, MASK, E) \
static int process_blocks##W##_##E(const uint8_t *p, size_t len, \
    unsigned blocks, void **base, int target, int source, size_t target_size, int trace) { \
    for (unsigned b = 0; b < blocks; ++b) { \
        if (len < 2*sizeof(T)) return -1; \
        uint64_t index = rd##W(p, E), count = rd##W(p+sizeof(T), E); \
        p += 2*sizeof(T); len -= 2*sizeof(T); \
        if (count > len/sizeof(T) || index > target_size) return -1; \
        for (uint64_t e = 0; e < count; ++e) { \
            T v = rd##W(p, E); p += sizeof(T); len -= sizeof(T); \
            uint64_t delta = v & (MASK); unsigned type = v >> (SHIFT); \
            size_t bytes = type == 1 ? 8 : 4; \
            if (type > 1 || delta > target_size-index || bytes > target_size-index-delta) return -1; \
            apply_reloc(base, target, source, (size_t)(index+delta), type, trace); \
        } \
    } \
    return 0; \
}
BLOCK_PROCESS(16, uint16_t, 12, UINT16_C(0xFFF), 0)
BLOCK_PROCESS(16, uint16_t, 12, UINT16_C(0xFFF), 1)
BLOCK_PROCESS(32, uint32_t, 28, UINT32_C(0xFFFFFFF), 0)
BLOCK_PROCESS(32, uint32_t, 28, UINT32_C(0xFFFFFFF), 1)
BLOCK_PROCESS(64, uint64_t, 60, UINT64_C(0xFFFFFFFFFFFFFFF), 0)
BLOCK_PROCESS(64, uint64_t, 60, UINT64_C(0xFFFFFFFFFFFFFFF), 1)
#undef BLOCK_PROCESS

static int process_reloc_section(const uint8_t *rp, size_t len, unsigned endian,
                                 void **base, const size_t *sizes, int n, int trace) {
    if (len < 8) return -1;
    unsigned format = rd16(rp, endian), design = rd16(rp+2, endian);
    unsigned couple = rd16(rp+4, endian), count = rd16(rp+6, endian);
    int target = couple & 31, source = (couple >> 5) & 31;
    if (target >= n || source >= n) return -1;
    rp += 8; len -= 8;
    if (design == 0) {
        if (format != 0 || count > len/2) return -1;
        for (unsigned e = 0; e < count; ++e) {
            uint16_t v = rd16(rp+2*e, endian);
            size_t offset = v & 0xFFF; unsigned type = v >> 12;
            size_t bytes = type == 1 ? 8 : 4;
            if (type > 1 || offset > sizes[target] || bytes > sizes[target]-offset) return -1;
            apply_reloc(base, target, source, offset, type, trace);
        }
        return 0;
    }
    if (design != 1 && design != 2) return -1;
    switch (format) {
        case 0: return endian == 0 ? process_blocks16_0(rp,len,count,base,target,source,sizes[target],trace) : process_blocks16_1(rp,len,count,base,target,source,sizes[target],trace);
        case 1: return endian == 0 ? process_blocks32_0(rp,len,count,base,target,source,sizes[target],trace) : process_blocks32_1(rp,len,count,base,target,source,sizes[target],trace);
        case 2: return endian == 0 ? process_blocks64_0(rp,len,count,base,target,source,sizes[target],trace) : process_blocks64_1(rp,len,count,base,target,source,sizes[target],trace);
        default: return -1;
    }
}

int main(int argc, char *argv[]) {
    const char *path = NULL;
    const char *load_mode = "monolithic";
    int trace_reloc = 1;
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "--L=", 4)) load_mode = argv[i] + 4;
        else if (!strcmp(argv[i], "--quiet-reloc")) trace_reloc = 0;
        else path = argv[i];
    }
    if (!path) { fprintf(stderr, "usage: %s [--L=monolithic|scattered] <file.ueec>\n", argv[0]); return 1; }

    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(fsize);
    if (fread(buf, 1, fsize, f) != (size_t)fsize) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);

    // ---------------- header parsing ----------------
    if (buf[0] != UEEC_MAGIC_0 || buf[1] != UEEC_MAGIC_1 || buf[2] != UEEC_MAGIC_2) {
        fprintf(stderr, "invalid magic\n"); return 1;
    }
    unsigned file_endian = (buf[3] >> 7) & 1;
    unsigned nodes = rd16(buf + 4, file_endian);
    unsigned names = rd16(buf + 6, file_endian);
    (void)names;
    size_t off = 8;

    typedef struct { int machine_type, machine, section_count; } node_t;
    node_t *node = malloc(sizeof(node_t) * nodes);
    long total_info = 0;
    for (unsigned i = 0; i < nodes; i++) {
        uint32_t v = rd32(buf + off, file_endian);
        node[i].machine_type = (v >> 7) & 0x7;
        node[i].machine = (v >> 10) & 0xFF;
        node[i].section_count = ((v >> 27) & 0x1F) + 1;
        total_info += node[i].section_count - 1; // HEADER ar iTvleba node_info-Si
        off += 4;
        off += 2; // format
    }

    typedef struct { int n, s; } info_t;
    info_t *info = malloc(sizeof(info_t) * total_info);
    long max_section = 0;
    for (long i = 0; i < total_info; i++) {
        info[i].n = rd16(buf + off, file_endian); off += 2;
        info[i].s = rd32(buf + off, file_endian); off += 4;
        if (info[i].s > max_section) max_section = info[i].s;
    }
    long total_sections = max_section + 1;

    int *sec_type = malloc(sizeof(int) * total_sections);
    long *sec_size = malloc(sizeof(long) * total_sections);
    long *sec_off = malloc(sizeof(long) * total_sections);
    for (long s = 0; s < total_sections; s++) {
        sec_type[s] = rd16(buf + off, file_endian); off += 2;
        sec_size[s] = rd16(buf + off, file_endian); off += 2;
        off += 2; // data_size (ar gvWirdeba am loader-Si)
    }
    sec_off[0] = 0;
    for (long s = 1; s < total_sections; s++) sec_off[s] = sec_off[s-1] + sec_size[s-1];

    // ---------------- node arCeva ----------------
    int chosen = -1;
    for (unsigned i = 0; i < nodes; i++) {
        if (node[i].machine_type == UEEC_MACHINE_TYPE_CPU && node[i].machine == HOST_MACHINE) {
            chosen = (int)i; break;
        }
    }
    if (chosen < 0) { fprintf(stderr, "no matching node\n"); return 1; }
    printf("node: %d, load-mode: %s\n", chosen, load_mode);

    // ---------------- resident + RELOC seqciebis Segroveba ----------------
    int resident_g[8], resident_type[8], n_resident = 0;
    int reloc_g[8], n_reloc = 0;
    for (long e = 0; e < total_info; e++) {
        if (info[e].n != chosen) continue;
        int s = info[e].s;
        if (sec_type[s] == UEEC_SECTION_RELOCATION) reloc_g[n_reloc++] = s;
        else { resident_type[n_resident] = sec_type[s]; resident_g[n_resident] = s; n_resident++; }
    }
    printf("resident sections: %d, RELOC sections: %d\n", n_resident, n_reloc);

    // ---------------- CamtvirTva: monolithic an scattered ----------------
    void *local_base[8];
    void *mono_mem = NULL;
    long total_mono_size = 0;
    long *local_off_in_mono = malloc(sizeof(long) * n_resident);

    if (!strcmp(load_mode, "monolithic")) {
        long total = 0;
        for (int i = 0; i < n_resident; i++) {
            local_off_in_mono[i] = total;
            long padded = ((sec_size[resident_g[i]] + 15) / 16) * 16; // 16-byte alignment
            total += padded;
        }
        mono_mem = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        for (int i = 0; i < n_resident; i++) {
            local_base[i] = (uint8_t *)mono_mem + local_off_in_mono[i];
            memcpy(local_base[i], buf + sec_off[resident_g[i]], sec_size[resident_g[i]]);
        }
        total_mono_size = total;
    } else if (!strcmp(load_mode, "scattered")) {
        for (int i = 0; i < n_resident; i++) {
            local_base[i] = mmap(NULL, sec_size[resident_g[i]], PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            memcpy(local_base[i], buf + sec_off[resident_g[i]], sec_size[resident_g[i]]);
        }
    } else {
        fprintf(stderr, "unknown --L mode: %s\n", load_mode); return 1;
    }

    size_t resident_sizes[8];
    for (int i = 0; i < n_resident; ++i) resident_sizes[i] = (size_t)sec_size[resident_g[i]];
    for (int r = 0; r < n_reloc; ++r) {
        int g = reloc_g[r];
        if (process_reloc_section(buf + sec_off[g], (size_t)sec_size[g], file_endian,
                                  local_base, resident_sizes, n_resident, trace_reloc) != 0) {
            fprintf(stderr, "invalid or unsupported relocation section %d\n", g);
            return 1;
        }
    }

    // ---------------- permission-ebi + gaSveba ----------------
    void *entry = NULL;
    if (mono_mem) {
        // monolithic: mTeli region erTad
        mprotect(mono_mem, total_mono_size, PROT_READ | PROT_EXEC | PROT_WRITE);
        // (martivi test-loader, W+X SamdvilobiT sazrunavia production-Si)
    }
    for (int i = 0; i < n_resident; i++) {
        if (!mono_mem) mprotect(local_base[i], sec_size[resident_g[i]],
                                 resident_type[i] == UEEC_SECTION_CODE ? (PROT_READ | PROT_EXEC) : PROT_READ);
        if (resident_type[i] == UEEC_SECTION_CODE && !entry) entry = local_base[i];
    }
    if (!entry) { fprintf(stderr, "no CODE section found\n"); return 1; }

    fflush(stdout);
#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
    volatile unsigned long fdesc[3] = { (unsigned long)entry, 0, 0 };
    void (*fn)(void) = (void (*)(void))(unsigned long)fdesc;
    fn();
#elif defined(__powerpc64__)
    __asm__ volatile("mtctr %0\n\tbctr" :: "r"(entry));
#else
    void (*fn)(void) = (void (*)(void))entry;
    fn();
#endif
    return 1;
}
