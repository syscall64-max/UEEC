//
// loader_export_ordinal.c -- erT-node-iani UEEC, ORDINAL EXPORT. binary
// search ordinal-is (ricxviTi identifikatoris) mixedviT, saxelis gareSe.
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

// PowerPC64 ELFv1: funqciis mაჩvenebeli unda miuTiTebdes descriptor-s
// {entry,TOC,env}-ze, ara pirdapir kods -- gansxvavebiT sxva arqiteqturebisgan.
static int call_answer(void *addr) {
#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
    volatile unsigned long fdesc[3] = { (unsigned long)addr, 0, 0 };
    int (*fn)(void) = (int (*)(void))(unsigned long)fdesc;
    return fn();
#else
    int (*fn)(void) = (int (*)(void))addr;
    return fn();
#endif
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
    if (nodes != 1) { fprintf(stderr, "es loader mxolod erT-node-ian failebs uWers\n"); return 1; }
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
        off += 2;
    }
    long *sec_off = malloc(sizeof(long) * total_sections);
    sec_off[0] = 0;
    for (long s = 1; s < total_sections; s++) sec_off[s] = sec_off[s-1] + sec_size[s-1];

    int resident_g[8], n_resident = 0;
    int export_g[8], n_export = 0;
    for (int i = 0; i < total_info; i++) {
        int s = info[i].s;
        if (sec_type[s] == UEEC_SECTION_EXPORT) export_g[n_export++] = s;
        else resident_g[n_resident++] = s;
    }
    printf("resident sections: %d, EXPORT sections: %d\n", n_resident, n_export);

    void *local_base[8];
    long *local_off = malloc(sizeof(long) * n_resident);
    long total = 0;
    for (int i = 0; i < n_resident; i++) {
        local_off[i] = total;
        total += ((sec_size[resident_g[i]] + 15) / 16) * 16;
    }
    void *mono_mem = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (int i = 0; i < n_resident; i++) {
        local_base[i] = (uint8_t *)mono_mem + local_off[i];
        memcpy(local_base[i], buf + sec_off[resident_g[i]], sec_size[resident_g[i]]);
    }
    mprotect(mono_mem, total, PROT_READ | PROT_WRITE | PROT_EXEC);

    // ---------------- EXPORT parsing (ORDINAL, format=16) ----------------
    typedef struct { int section, offset, ordinal; } ord_entry;
    ord_entry entries[64]; int n_entries = 0;

    for (int e = 0; e < n_export; e++) {
        uint8_t *ep = buf + sec_off[export_g[e]];
        size_t p = 0;
        uint16_t format = rd16(ep + p, file_endian); p += 2;
        uint16_t design = rd16(ep + p, file_endian); p += 2;
        if (design != 2 || format != 0) { fprintf(stderr, "ucnobi/mxardauWeri\n"); return 1; }

        p += 2; // count.format
        uint16_t count = rd16(ep + p, file_endian); p += 2;

        for (int i = 0; i < count; i++) {
            uint8_t sb = ep[p]; p += 1;
            entries[n_entries].section = sb & 0x1F;
            entries[n_entries].offset = rd16(ep + p, file_endian); p += 2;
            entries[n_entries].ordinal = rd16(ep + p, file_endian); p += 2;
            n_entries++;
        }
    }
    printf("export entries: %d\n", n_entries);
    for (int i = 0; i < n_entries; i++)
        printf("  [%d] ordinal=%d section=%d offset=%d\n", i, entries[i].ordinal, entries[i].section, entries[i].offset);

    // ---------------- BINARY SEARCH ordinal-iT ----------------
    int queries[] = { 5, 1, 99 };
    for (int q = 0; q < 3; q++) {
        int lo = 0, hi = n_entries - 1, found = -1, cmp = 0;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            cmp++;
            if (entries[mid].ordinal == queries[q]) { found = mid; break; }
            if (queries[q] < entries[mid].ordinal) hi = mid - 1; else lo = mid + 1;
        }
        if (found < 0) { printf("  ordinal=%d -> ver moidzebna (%d cmp)\n", queries[q], cmp); continue; }
        void *addr = (uint8_t *)local_base[entries[found].section] + entries[found].offset;
        printf("  ordinal=%d -> section=%d offset=%d (%d cmp)\n", queries[q], entries[found].section, entries[found].offset, cmp);
        if (entries[found].section == 1) {
            printf("    answer() = %d\n", call_answer(addr));
        } else {
            int32_t v; memcpy(&v, addr, 4);
            printf("    value = %d\n", v);
        }
    }

    return 0;
}
