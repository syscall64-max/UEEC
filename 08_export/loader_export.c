//
// loader_export.c -- erT-node-iani UEEC, COMPACT_GROUP EXPORT. ORDONIANI
// binary search: jer groups[]-Si (length-iT), Semdeg jgufis Signiট
// info[]+name_data-Si (saxeliT, TanabarsigrZe SedarebiT).
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

typedef struct {
    int length;
    int count;
    int section[16];
    int offset[16];
    const uint8_t *name_data; // count * length bytes, entry-ebis igive rigit
} group_t;

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

    // ---------------- EXPORT parsing (COMPACT_GROUP, format=16) ----------------
    group_t groups[16]; int n_groups = 0;

    for (int e = 0; e < n_export; e++) {
        uint8_t *ep = buf + sec_off[export_g[e]];
        size_t p = 0;
        uint16_t format = rd16(ep + p, file_endian); p += 2;
        uint16_t design = rd16(ep + p, file_endian); p += 2;
        if (design != 0 || format != 0) { fprintf(stderr, "ucnobi/mxardauWeri\n"); return 1; }

        p += 2; // groups.format
        uint16_t n_grp = rd16(ep + p, file_endian); p += 2;

        int gr_length[16], gr_count[16];
        for (int g = 0; g < n_grp; g++) {
            gr_length[g] = rd16(ep + p, file_endian); p += 2;
            gr_count[g] = rd16(ep + p, file_endian); p += 2;
        }
        for (int g = 0; g < n_grp; g++) {
            groups[n_groups].length = gr_length[g];
            groups[n_groups].count = gr_count[g];
            for (int i = 0; i < gr_count[g]; i++) {
                uint8_t sb = ep[p]; p += 1;
                groups[n_groups].section[i] = sb & 0x1F;
                groups[n_groups].offset[i] = rd16(ep + p, file_endian); p += 2;
            }
            groups[n_groups].name_data = ep + p;
            p += (size_t)gr_count[g] * gr_length[g];
            n_groups++;
        }
    }
    printf("groups: %d\n", n_groups);
    for (int g = 0; g < n_groups; g++) {
        printf("  group[%d]: length=%d count=%d ->", g, groups[g].length, groups[g].count);
        for (int i = 0; i < groups[g].count; i++)
            printf(" %.*s", groups[g].length, groups[g].name_data + i*groups[g].length);
        printf("\n");
    }

    // ---------------- ORDONIANI BINARY SEARCH ----------------
    const char *queries[] = { "counter", "bar", "foo", "answer", "banana", "xyz", "nonexistent" };
    for (int q = 0; q < 7; q++) {
        int qlen = (int)strlen(queries[q]);
        int total_cmp = 0;

        // done 1: groups[] Ziebs length-iT
        int glo = 0, ghi = n_groups - 1, g_found = -1;
        while (glo <= ghi) {
            int gmid = (glo + ghi) / 2;
            total_cmp++;
            if (groups[gmid].length == qlen) { g_found = gmid; break; }
            if (qlen < groups[gmid].length) ghi = gmid - 1; else glo = gmid + 1;
        }
        if (g_found < 0) {
            printf("  '%s' -> ver moidzebna (jgufi length=%d ar arsebobs, %d cmp)\n", queries[q], qlen, total_cmp);
            continue;
        }

        // done 2: jgufis Signiট info[]+name_data Ziebs saxeliT
        group_t *gr = &groups[g_found];
        int elo = 0, ehi = gr->count - 1, e_found = -1;
        while (elo <= ehi) {
            int emid = (elo + ehi) / 2;
            total_cmp++;
            int c = memcmp(queries[q], gr->name_data + emid*gr->length, qlen);
            if (c == 0) { e_found = emid; break; }
            if (c < 0) ehi = emid - 1; else elo = emid + 1;
        }
        if (e_found < 0) {
            printf("  '%s' -> ver moidzebna (jgufSi ver ipoves, %d cmp)\n", queries[q], total_cmp);
            continue;
        }

        void *addr = (uint8_t *)local_base[gr->section[e_found]] + gr->offset[e_found];
        printf("  '%s' -> section=%d offset=%d (%d cmp sul)\n",
               queries[q], gr->section[e_found], gr->offset[e_found], total_cmp);

        if (!strcmp(queries[q], "answer")) {
            printf("    answer() = %d\n", call_answer(addr));
        } else {
            int32_t v; memcpy(&v, addr, 4);
            printf("    value = %d\n", v);
        }
    }

    return 0;
}
