//
// convert.c -- universaluri konverteri: nebismieri RELOC design
// (SECTION_FLAT=0, SECTION_BLOCK_4096=1, SECTION_BLOCK_N=2) nebismier
// sxvaSi. saerTo shualeduri sturqtura: {target,source,offset,type}
// entry-ebis brtyeli sia. block.index aq ariTmeti "base"-ia (ara
// "page nomeri") -- decode Tvitkmaria, gare page_size-is codnis gareSe.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ueec.h"

static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t rd64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }

typedef struct { int n, s; } info_t;
typedef struct { int target, source; long offset; int type; } reloc_entry;

// ---------------- decode: nebismieri design -> brtyeli sia ----------------
static reloc_entry *decode_reloc(const uint8_t *rp, int *out_count) {
    size_t p = 0;
    uint16_t format = rd16(rp + p); p += 2;
    uint16_t design = rd16(rp + p); p += 2;
    uint16_t couple = rd16(rp + p); p += 2;
    int target = couple & 0x1F;
    int source = (couple >> 5) & 0x1F;
    uint16_t couple_count = rd16(rp + p); p += 2;

    reloc_entry *out = malloc(sizeof(reloc_entry) * 4096);
    int n = 0;

    if (design == 0) { // FLAT
        for (int e = 0; e < couple_count; e++) {
            uint16_t v = rd16(rp + p); p += 2;
            out[n].target = target; out[n].source = source;
            out[n].offset = v & 0xFFF;
            out[n].type = (v >> 12) & 0xF;
            n++;
        }
    } else if (design == 1 || design == 2) { // BLOCK_4096 an BLOCK_N
        int idx_bytes = (format == 0) ? 2 : (format == 1) ? 4 : 8;
        for (int b = 0; b < couple_count; b++) {
            long index, blk_count;
            if (idx_bytes == 2) { index = rd16(rp+p); p+=2; blk_count = rd16(rp+p); p+=2; }
            else if (idx_bytes == 4) { index = rd32(rp+p); p+=4; blk_count = rd32(rp+p); p+=4; }
            else { index = (long)rd64(rp+p); p+=8; blk_count = (long)rd64(rp+p); p+=8; }

            for (long e = 0; e < blk_count; e++) {
                long in_block; int type;
                if (idx_bytes == 2) { uint16_t v = rd16(rp+p); p+=2; in_block = v & 0xFFF; type = (v>>12)&0xF; }
                else if (idx_bytes == 4) { uint32_t v = rd32(rp+p); p+=4; in_block = v & 0xFFFFFFFL; type = (v>>28)&0xF; }
                else { uint64_t v = rd64(rp+p); p+=8; in_block = (long)(v & 0xFFFFFFFFFFFFFFFULL); type = (int)((v>>60)&0xF); }
                out[n].target = target; out[n].source = source;
                out[n].offset = index + in_block; // index = base, TVITKMARI
                out[n].type = type;
                n++;
            }
        }
    } else {
        fprintf(stderr, "ucnobi design: %u\n", design);
        exit(1);
    }
    *out_count = n;
    return out;
}

// ---------------- encode: brtyeli sia -> archeuli design ----------------
static uint8_t *encode_flat(reloc_entry *entries, int count, long *out_size) {
    long sz = 4 + 4 + (long)count * 2;
    uint8_t *nb = malloc(sz);
    size_t wp = 0;
    uint16_t fmt = 0; memcpy(nb+wp,&fmt,2); wp+=2;
    uint16_t des = 0; memcpy(nb+wp,&des,2); wp+=2;
    uint16_t cpl = (uint16_t)((entries[0].target & 0x1F) | ((entries[0].source & 0x1F) << 5));
    memcpy(nb+wp,&cpl,2); wp+=2;
    uint16_t cnt = (uint16_t)count; memcpy(nb+wp,&cnt,2); wp+=2;
    for (int e = 0; e < count; e++) {
        uint16_t v = (uint16_t)((entries[e].offset & 0xFFF) | ((entries[e].type & 0xF) << 12));
        memcpy(nb+wp,&v,2); wp+=2;
    }
    *out_size = sz;
    return nb;
}

static uint8_t *encode_block(reloc_entry *entries, int count, long page_size, int design, long *out_size) {
    long *base_of = malloc(sizeof(long) * count);
    for (int e = 0; e < count; e++) base_of[e] = (entries[e].offset / page_size) * page_size;

    long *uniq = malloc(sizeof(long) * count);
    int n_pages = 0;
    for (int e = 0; e < count; e++) {
        int found = 0;
        for (int u = 0; u < n_pages; u++) if (uniq[u] == base_of[e]) { found = 1; break; }
        if (!found) uniq[n_pages++] = base_of[e];
    }

    long max_delta = 0;
    for (int e = 0; e < count; e++) {
        long d = entries[e].offset - base_of[e];
        if (d > max_delta) max_delta = d;
    }
    int format = 0;
    if (max_delta > 0xFFF) format = 1;
    if (max_delta > 0xFFFFFFFL) format = 2;
    int idx_bytes = (format == 0) ? 2 : (format == 1) ? 4 : 8;

    long sz = 4 + 4;
    for (int u = 0; u < n_pages; u++) {
        int c = 0;
        for (int e = 0; e < count; e++) if (base_of[e] == uniq[u]) c++;
        sz += 2L * idx_bytes + (long)c * idx_bytes;
    }

    uint8_t *nb = malloc(sz);
    size_t wp = 0;
    uint16_t fmt = (uint16_t)format; memcpy(nb+wp,&fmt,2); wp+=2;
    uint16_t des = (uint16_t)design; memcpy(nb+wp,&des,2); wp+=2;
    uint16_t cpl = (uint16_t)((entries[0].target & 0x1F) | ((entries[0].source & 0x1F) << 5));
    memcpy(nb+wp,&cpl,2); wp+=2;
    uint16_t npg = (uint16_t)n_pages; memcpy(nb+wp,&npg,2); wp+=2;

    for (int u = 0; u < n_pages; u++) {
        int c = 0;
        for (int e = 0; e < count; e++) if (base_of[e] == uniq[u]) c++;
        if (idx_bytes==2){uint16_t v=(uint16_t)uniq[u];memcpy(nb+wp,&v,2);wp+=2;uint16_t cc=(uint16_t)c;memcpy(nb+wp,&cc,2);wp+=2;}
        else if (idx_bytes==4){uint32_t v=(uint32_t)uniq[u];memcpy(nb+wp,&v,4);wp+=4;uint32_t cc=(uint32_t)c;memcpy(nb+wp,&cc,4);wp+=4;}
        else {uint64_t v=(uint64_t)uniq[u];memcpy(nb+wp,&v,8);wp+=8;uint64_t cc=(uint64_t)c;memcpy(nb+wp,&cc,8);wp+=8;}

        for (int e = 0; e < count; e++) {
            if (base_of[e] != uniq[u]) continue;
            long delta = entries[e].offset - uniq[u];
            if (idx_bytes==2){uint16_t v=(uint16_t)((delta&0xFFF)|((entries[e].type&0xF)<<12));memcpy(nb+wp,&v,2);wp+=2;}
            else if (idx_bytes==4){uint32_t v=(uint32_t)((delta&0xFFFFFFFL)|((long)(entries[e].type&0xF)<<28));memcpy(nb+wp,&v,4);wp+=4;}
            else {uint64_t v=(uint64_t)((delta&0xFFFFFFFFFFFFFFFLL)|((int64_t)(entries[e].type&0xF)<<60));memcpy(nb+wp,&v,8);wp+=8;}
        }
    }
    free(base_of); free(uniq);
    *out_size = sz;
    return nb;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "gamoyeneba: %s <input.ueec> <output.ueec> --design flat|block4096|blockn [--page-size N]\n", argv[0]);
        return 1;
    }
    const char *in_path = argv[1], *out_path = argv[2];
    int out_design = 1; // default: block4096
    long page_size = 4096;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--design") && i + 1 < argc) {
            const char *d = argv[++i];
            if (!strcmp(d, "flat")) out_design = 0;
            else if (!strcmp(d, "block4096")) { out_design = 1; page_size = 4096; }
            else if (!strcmp(d, "blockn")) out_design = 2;
            else { fprintf(stderr, "ucnobi design: %s\n", d); return 1; }
        } else if (!strcmp(argv[i], "--page-size") && i + 1 < argc) {
            page_size = atol(argv[++i]);
        }
    }

    FILE *f = fopen(in_path, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(fsize);
    if (fread(buf, 1, fsize, f) != (size_t)fsize) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);

    if (buf[0] != UEEC_MAGIC_0 || buf[1] != UEEC_MAGIC_1 || buf[2] != UEEC_MAGIC_2) {
        fprintf(stderr, "invalid magic\n"); return 1;
    }
    unsigned nodes = rd16(buf + 4);
    size_t off = 8;

    long total_info = 0;
    for (unsigned i = 0; i < nodes; i++) {
        uint32_t v = rd32(buf + off);
        total_info += ((v >> 27) & 0x1F) + 1 - 1;
        off += 4; off += 2;
    }

    info_t *info = malloc(sizeof(info_t) * total_info);
    long max_section = 0;
    for (long i = 0; i < total_info; i++) {
        info[i].n = rd16(buf + off); off += 2;
        info[i].s = rd32(buf + off); off += 4;
        if (info[i].s > max_section) max_section = info[i].s;
    }
    long total_sections = max_section + 1;

    int *sec_type = malloc(sizeof(int) * total_sections);
    long *sec_size = malloc(sizeof(long) * total_sections);
    long sh_table_start = off;
    for (long s = 0; s < total_sections; s++) {
        sec_type[s] = rd16(buf + off); off += 2;
        sec_size[s] = rd16(buf + off); off += 2;
        off += 2; // data_size
    }
    long *sec_off = malloc(sizeof(long) * total_sections);
    sec_off[0] = 0;
    for (long s = 1; s < total_sections; s++) sec_off[s] = sec_off[s - 1] + sec_size[s - 1];

    uint8_t *new_reloc_content[64];
    long new_reloc_size[64];
    int n_reloc_sections = 0;

    for (long s = 0; s < total_sections; s++) {
        if (sec_type[s] != UEEC_SECTION_RELOCATION) continue;
        int count;
        reloc_entry *entries = decode_reloc(buf + sec_off[s], &count);

        long sz;
        uint8_t *nb;
        if (out_design == 0) nb = encode_flat(entries, count, &sz);
        else nb = encode_block(entries, count, page_size, out_design, &sz);

        new_reloc_content[n_reloc_sections] = nb;
        new_reloc_size[n_reloc_sections] = sz;
        n_reloc_sections++;
        free(entries);
    }

    // ---------------- axali failis awyoba ----------------
    long name_table_start = sh_table_start + 6 * total_sections;
    long header_block = sec_size[0];

    long *new_sec_size = malloc(sizeof(long) * total_sections);
    int reloc_idx = 0;
    for (long s = 0; s < total_sections; s++) {
        if (sec_type[s] == UEEC_SECTION_RELOCATION) new_sec_size[s] = new_reloc_size[reloc_idx++];
        else new_sec_size[s] = sec_size[s];
    }
    long *new_sec_off = malloc(sizeof(long) * total_sections);
    new_sec_off[0] = 0;
    for (long s = 1; s < total_sections; s++) new_sec_off[s] = new_sec_off[s - 1] + new_sec_size[s - 1];
    long new_total = new_sec_off[total_sections - 1] + new_sec_size[total_sections - 1];

    uint8_t *out = malloc(new_total);
    memcpy(out, buf, sh_table_start);
    size_t wp = sh_table_start;
    for (long s = 0; s < total_sections; s++) {
        uint16_t t = (uint16_t)sec_type[s];
        uint16_t sz = (uint16_t)((s == 0) ? header_block : new_sec_size[s]);
        memcpy(out + wp, &t, 2); wp += 2;
        memcpy(out + wp, &sz, 2); wp += 2;
        memcpy(out + wp, &sz, 2); wp += 2;
    }
    long name_table_bytes = sec_off[1] - name_table_start;
    memcpy(out + wp, buf + name_table_start, name_table_bytes);
    wp += name_table_bytes;

    reloc_idx = 0;
    for (long s = 1; s < total_sections; s++) {
        if (sec_type[s] == UEEC_SECTION_RELOCATION) {
            memcpy(out + wp, new_reloc_content[reloc_idx], new_reloc_size[reloc_idx]);
            wp += new_reloc_size[reloc_idx];
            reloc_idx++;
        } else {
            memcpy(out + wp, buf + sec_off[s], sec_size[s]);
            wp += sec_size[s];
        }
    }

    FILE *of = fopen(out_path, "wb");
    if (!of) { perror("fopen(out)"); return 1; }
    fwrite(out, 1, wp, of);
    fclose(of);

    const char *dname = out_design == 0 ? "FLAT" : out_design == 1 ? "BLOCK_4096" : "BLOCK_N";
    fprintf(stderr, "%s -> %s: design=%s, %ld reloc-seqcia, %zu bytes sruli\n",
            in_path, out_path, dname, (long)n_reloc_sections, wp);
    return 0;
}
