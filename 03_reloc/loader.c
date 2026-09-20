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

static void apply_reloc(void **local_base, int target, int source, int patch_offset, int type) {
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
    printf("  RELOC: local[%d]+0x%x = local[%d] + addend (type=%s)\n",
           target, patch_offset, source, type ? "ABS64" : "ABS32");
}

int main(int argc, char *argv[]) {
    const char *path = NULL;
    const char *load_mode = "monolithic";
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "--L=", 4)) load_mode = argv[i] + 4;
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

    // ---------------- RELOC aplikacia (FLAT/BLOCK_4096/BLOCK_N) ----------------
    for (int r = 0; r < n_reloc; r++) {
        uint8_t *rp = buf + sec_off[reloc_g[r]];
        size_t p = 0;
        uint16_t format = rd16(rp + p, file_endian); p += 2;
        uint16_t design = rd16(rp + p, file_endian); p += 2;

        uint16_t couple = rd16(rp + p, file_endian); p += 2;
        int target = couple & 0x1F;
        int source = (couple >> 5) & 0x1F;
        uint16_t couple_count = rd16(rp + p, file_endian); p += 2;

        if (design == 0) { // SECTION_FLAT
            for (int e = 0; e < couple_count; e++) {
                uint16_t v = rd16(rp + p, file_endian); p += 2;
                int patch_offset = v & 0xFFF;
                int type = (v >> 12) & 0xF;
                apply_reloc(local_base, target, source, patch_offset, type);
            }
        } else if (design == 1 || design == 2) { // BLOCK_4096 / BLOCK_N
            // index = base (ara "page nomeri") -- patch_offset = index + delta
            int w = (format == 0) ? 2 : (format == 1) ? 4 : 8;
            for (int b = 0; b < couple_count; b++) {
                long index, blk_count;
                if (w == 2) { index = rd16(rp+p,file_endian); p+=2; blk_count = rd16(rp+p,file_endian); p+=2; }
                else if (w == 4) { index = rd32(rp+p,file_endian); p+=4; blk_count = rd32(rp+p,file_endian); p+=4; }
                else { index = (long)rd64(rp+p,file_endian); p+=8; blk_count = (long)rd64(rp+p,file_endian); p+=8; }

                for (long e = 0; e < blk_count; e++) {
                    long delta; int type;
                    if (w == 2) { uint16_t v = rd16(rp+p,file_endian); p+=2; delta = v & 0xFFF; type = (v>>12)&0xF; }
                    else if (w == 4) { uint32_t v = rd32(rp+p,file_endian); p+=4; delta = v & 0xFFFFFFFL; type = (v>>28)&0xF; }
                    else { uint64_t v = rd64(rp+p,file_endian); p+=8; delta = (long)(v & 0xFFFFFFFFFFFFFFFULL); type = (int)((v>>60)&0xF); }
                    apply_reloc(local_base, target, source, (int)(index + delta), type);
                }
            }
        } else {
            fprintf(stderr, "unsupported reloc design: %u\n", design); return 1;
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
