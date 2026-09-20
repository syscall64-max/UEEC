//
// loader_pe.c -- erT-node-iani UEEC, PE_STYLE RELOC (design=3). mxolod
// monolithic load-Ti muSaobs (image_base=mono_mem, saerTo, gaerTianebuli
// safuZveli mTeli node-is resident-seqciebisTvis).
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "ueec.h"

static uint16_t rd16(const uint8_t *p, unsigned file_endian) {
    uint16_t v; memcpy(&v, p, 2);
    return (file_endian == UEEC_HOST_ENDIAN) ? v : __builtin_bswap16(v);
}
static uint32_t rd32(const uint8_t *p, unsigned file_endian) {
    uint32_t v; memcpy(&v, p, 4);
    return (file_endian == UEEC_HOST_ENDIAN) ? v : __builtin_bswap32(v);
}

int main(int argc, char *argv[]) {
    if (argc != 2) { fprintf(stderr, "gamoyeneba: %s <file.ueec>\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long fsize = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(fsize);
    if (fread(buf, 1, fsize, f) != (size_t)fsize) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);

    if (buf[0] != UEEC_MAGIC_0 || buf[1] != UEEC_MAGIC_1 || buf[2] != UEEC_MAGIC_2) {
        fprintf(stderr, "invalid magic\n"); return 1;
    }
    unsigned file_endian = (buf[3] >> 7) & 1;
    unsigned nodes = rd16(buf + 4, file_endian);
    if (nodes != 1) { fprintf(stderr, "es loader mxolod erT-node-ian failebs uWers (nodes=%u)\n", nodes); return 1; }
    size_t off = 8;
    uint32_t nh = rd32(buf + off, file_endian); off += 4; off += 2;
    int section_count = ((nh >> 27) & 0x1F) + 1;
    int total_info = section_count - 1;

    typedef struct { int n, s; } info_t;
    info_t *info = malloc(sizeof(info_t) * total_info);
    long max_section = 0;
    for (int i = 0; i < total_info; i++) {
        info[i].n = rd16(buf + off, file_endian); off += 2;
        info[i].s = rd32(buf + off, file_endian); off += 4;
        if (info[i].s > max_section) max_section = info[i].s;
    }
    long total_sections = max_section + 1;

    int *sec_type = malloc(sizeof(int) * total_sections);
    long *sec_size = malloc(sizeof(long) * total_sections);
    for (long s = 0; s < total_sections; s++) {
        sec_type[s] = rd16(buf + off, file_endian); off += 2;
        sec_size[s] = rd16(buf + off, file_endian); off += 2;
        off += 2; // data_size
    }
    long *sec_off = malloc(sizeof(long) * total_sections);
    sec_off[0] = 0;
    for (long s = 1; s < total_sections; s++) sec_off[s] = sec_off[s-1] + sec_size[s-1];

    int resident_g[8], resident_type[8], n_resident = 0;
    int reloc_g[8], n_reloc = 0;
    for (int i = 0; i < total_info; i++) {
        int s = info[i].s;
        if (sec_type[s] == UEEC_SECTION_RELOCATION) reloc_g[n_reloc++] = s;
        else { resident_type[n_resident] = sec_type[s]; resident_g[n_resident] = s; n_resident++; }
    }
    printf("resident sections: %d, RELOC sections: %d\n", n_resident, n_reloc);

    // ---------------- monolithic Camtvirtva (image_base=mono_mem) ----------------
    long *local_off = malloc(sizeof(long) * n_resident);
    long total = 0;
    for (int i = 0; i < n_resident; i++) {
        local_off[i] = total;
        total += ((sec_size[resident_g[i]] + 15) / 16) * 16; // 16-byte alignment
    }
    void *mono_mem = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void *local_base[8];
    for (int i = 0; i < n_resident; i++) {
        local_base[i] = (uint8_t *)mono_mem + local_off[i];
        memcpy(local_base[i], buf + sec_off[resident_g[i]], sec_size[resident_g[i]]);
    }
    unsigned long image_base = (unsigned long)mono_mem;
    printf("image_base: %p (%ld ბაიტი)\n", mono_mem, total);

    // ---------------- PE_STYLE RELOC ----------------
    for (int r = 0; r < n_reloc; r++) {
        uint8_t *rp = buf + sec_off[reloc_g[r]];
        size_t p = 0;
        p += 2; // format (ar gvWirdeba am doneze)
        uint16_t design = rd16(rp + p, file_endian); p += 2;
        if (design != 3) { fprintf(stderr, "es loader mxolod PE_STYLE(3)-s uWers\n"); return 1; }

        uint16_t couple = rd16(rp + p, file_endian); p += 2;
        int section = couple & 0x1F;
        uint16_t couple_count = rd16(rp + p, file_endian); p += 2; // n_blocks

        for (int b = 0; b < couple_count; b++) {
            uint16_t block = rd16(rp + p, file_endian); p += 2;
            uint16_t block_count = rd16(rp + p, file_endian); p += 2;
            for (int e = 0; e < block_count; e++) {
                uint16_t v = rd16(rp + p, file_endian); p += 2;
                int in_block = v & 0xFFF;
                int type = (v >> 12) & 0xF;
                int patch_offset = block + in_block;

                void *target_addr = (uint8_t *)local_base[section] + patch_offset;
                if (type == 1) {
                    uint64_t addend; memcpy(&addend, target_addr, 8);
                    uint64_t patched = image_base + addend;
                    memcpy(target_addr, &patched, 8);
                } else {
                    uint32_t addend; memcpy(&addend, target_addr, 4);
                    uint32_t patched = (uint32_t)(image_base + addend);
                    memcpy(target_addr, &patched, 4);
                }
                printf("  RELOC(PE_STYLE): local[%d]+0x%x = image_base + addend (type=%s)\n",
                       section, patch_offset, type ? "ABS64" : "ABS32");
            }
        }
    }

    mprotect(mono_mem, total, PROT_READ | PROT_EXEC | PROT_WRITE);
    void *entry = NULL;
    for (int i = 0; i < n_resident; i++) if (resident_type[i] == UEEC_SECTION_CODE) { entry = local_base[i]; break; }
    if (!entry) { fprintf(stderr, "CODE ver moidzebna\n"); return 1; }

    fflush(stdout);
#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
    volatile unsigned long fdesc[3] = { (unsigned long)entry, 0, 0 };
    void (*fn)(void) = (void (*)(void))(unsigned long)fdesc;
    fn();
#else
    void (*fn)(void) = (void (*)(void))entry;
    fn();
#endif
    return 1;
}
