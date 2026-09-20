//
// loader_convert_export.c -- erT-node-iani UEEC, unificirebuli loader
// COMPACT_GROUP-isTvis (design=0, ordonian binary search) da
// SIMPLE_FLAT-isTvis (design=1, ertdoniani binary search) -- imave
// query-ebs uSvebs orive dizains, Sedegebis SedarebisTvis.
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
static uint64_t rd64(const uint8_t *p, unsigned file_endian) {
    uint64_t v; memcpy(&v, p, 8);
    return (file_endian == UEEC_HOST_ENDIAN) ? v : __builtin_bswap64(v);
}
static long readw(const uint8_t *p, int w, unsigned fe) {
    if (w == 2) return rd16(p, fe);
    if (w == 4) return rd32(p, fe);
    return (long)rd64(p, fe);
}
static int width_of(uint16_t format) { return (format == 0) ? 2 : (format == 1) ? 4 : 8; }

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
    int length, count;
    int section[64];
    long offset[64];
    const uint8_t *name_data;
} group_t;

typedef struct {
    int section;
    long offset;
    int namelen;
    const uint8_t *name;
} flat_t;

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

    if (n_export != 1) { fprintf(stderr, "amjerad mxolod 1 EXPORT seqcia\n"); return 1; }
    uint8_t *ep = buf + sec_off[export_g[0]];
    size_t p = 0;
    uint16_t format = rd16(ep + p, file_endian); p += 2;
    uint16_t design = rd16(ep + p, file_endian); p += 2;
    int w = width_of(format);
    printf("EXPORT design=%u (%s), format=%u\n", design,
           design == 0 ? "COMPACT_GROUP" : design == 1 ? "SIMPLE_FLAT" : "?", format);

    const char *queries[] = { "counter", "bar", "foo", "answer", "banana", "xyz", "nonexistent" };
    int n_queries = 7;

    if (design == 0) {
        p += 2; // groups.format
        long n_grp = readw(ep + p, w, file_endian); p += w;

        group_t groups[16]; int n_groups = 0;
        long gr_length[16], gr_count[16];
        for (long g = 0; g < n_grp; g++) {
            gr_length[g] = rd16(ep + p, file_endian); p += 2;
            gr_count[g] = readw(ep + p, w, file_endian); p += w;
        }
        for (long g = 0; g < n_grp; g++) {
            groups[n_groups].length = (int)gr_length[g];
            groups[n_groups].count = (int)gr_count[g];
            for (int i = 0; i < gr_count[g]; i++) {
                uint8_t sb = ep[p]; p += 1;
                groups[n_groups].section[i] = sb & 0x1F;
                groups[n_groups].offset[i] = readw(ep + p, w, file_endian); p += w;
            }
            groups[n_groups].name_data = ep + p;
            p += (size_t)gr_count[g] * gr_length[g];
            n_groups++;
        }
        printf("groups: %d\n", n_groups);

        for (int q = 0; q < n_queries; q++) {
            int qlen = (int)strlen(queries[q]);
            int total_cmp = 0;
            int glo = 0, ghi = n_groups - 1, g_found = -1;
            while (glo <= ghi) {
                int gmid = (glo + ghi) / 2;
                total_cmp++;
                if (groups[gmid].length == qlen) { g_found = gmid; break; }
                if (qlen < groups[gmid].length) ghi = gmid - 1; else glo = gmid + 1;
            }
            if (g_found < 0) { printf("  '%s' -> ver moidzebna (%d cmp)\n", queries[q], total_cmp); continue; }
            group_t *gr = &groups[g_found];
            int elo = 0, ehi = gr->count - 1, e_found = -1;
            while (elo <= ehi) {
                int emid = (elo + ehi) / 2;
                total_cmp++;
                int c = memcmp(queries[q], gr->name_data + emid * gr->length, qlen);
                if (c == 0) { e_found = emid; break; }
                if (c < 0) ehi = emid - 1; else elo = emid + 1;
            }
            if (e_found < 0) { printf("  '%s' -> ver moidzebna (%d cmp)\n", queries[q], total_cmp); continue; }
            void *addr = (uint8_t *)local_base[gr->section[e_found]] + gr->offset[e_found];
            printf("  '%s' -> section=%d offset=%ld (%d cmp)\n", queries[q], gr->section[e_found], gr->offset[e_found], total_cmp);
            if (!strcmp(queries[q], "answer")) { printf("    answer() = %d\n", call_answer(addr)); }
            else { int32_t v; memcpy(&v, addr, 4); printf("    value = %d\n", v); }
        }
    } else if (design == 1) {
        p += 2; // names.format
        long n_names = readw(ep + p, w, file_endian); p += w;

        flat_t *entries = malloc(sizeof(flat_t) * n_names);
        long *sec = malloc(sizeof(long) * n_names), *len = malloc(sizeof(long) * n_names);
        long *orva = malloc(sizeof(long) * n_names), *onam = malloc(sizeof(long) * n_names);
        for (long i = 0; i < n_names; i++) {
            uint16_t bits = rd16(ep + p, file_endian); p += 2;
            sec[i] = bits & 0x1F;
            len[i] = (bits >> 5) & 0x7FF;
            orva[i] = readw(ep + p, w, file_endian); p += w;
            onam[i] = readw(ep + p, w, file_endian); p += w;
        }
        const uint8_t *name_data_base = ep + p;
        for (long i = 0; i < n_names; i++) {
            entries[i].section = (int)sec[i];
            entries[i].offset = orva[i];
            entries[i].namelen = (int)len[i];
            entries[i].name = name_data_base + onam[i];
        }
        printf("names: %ld\n", n_names);

        for (int q = 0; q < n_queries; q++) {
            int qlen = (int)strlen(queries[q]);
            int cmp = 0;
            int lo = 0, hi = (int)n_names - 1, found = -1;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                cmp++;
                int minlen = qlen < entries[mid].namelen ? qlen : entries[mid].namelen;
                int c = memcmp(queries[q], entries[mid].name, minlen);
                if (c == 0) c = qlen - entries[mid].namelen;
                if (c == 0) { found = mid; break; }
                if (c < 0) hi = mid - 1; else lo = mid + 1;
            }
            if (found < 0) { printf("  '%s' -> ver moidzebna (%d cmp)\n", queries[q], cmp); continue; }
            void *addr = (uint8_t *)local_base[entries[found].section] + entries[found].offset;
            printf("  '%s' -> section=%d offset=%ld (%d cmp)\n", queries[q], entries[found].section, entries[found].offset, cmp);
            if (!strcmp(queries[q], "answer")) { printf("    answer() = %d\n", call_answer(addr)); }
            else { int32_t v; memcpy(&v, addr, 4); printf("    value = %d\n", v); }
        }
    } else {
        fprintf(stderr, "es loader mxolod design=0/1-s uWers\n"); return 1;
    }

    return 0;
}
