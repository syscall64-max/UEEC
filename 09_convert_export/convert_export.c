//
// convert_export.c -- universaluri konverteri: EXPORT- სექციის
// name-based დიზაინებს შორის (COMPACT_GROUP=0 <-> SIMPLE_FLAT=1),
// ნებისმიერი მიმართულებით. საერთო შუალედური სტრუქტურა: brtyeli
// {section,offset,name} entry-ebis sia.
//
// ORDINAL (design=2) gancxadebulad ar aris am konverteris scope-Si --
// ის ganTavsebulia gansxvavebul, saxelis-gareSe keyspace-Si (ordinal
// mТvlelSi), da name<->ordinal gardaqmna arasrulad-gansazRvruli
// operaciaa (ordinal-ebis gamogonebas moiTxovs), zustad iseve, rogorc
// 04_convert-Si PE_STYLE (reloc_v4.h) ganzrax gamotovebulia calke
// safexuris rigistvis. Ix. README.md.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ueec.h"

static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t rd64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }

static long readw(const uint8_t *p, int w) {
    if (w == 2) return rd16(p);
    if (w == 4) return rd32(p);
    return (long)rd64(p);
}
static void writew(uint8_t *p, long v, int w) {
    if (w == 2) { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); }
    else if (w == 4) { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); }
    else { uint64_t x = (uint64_t)v; memcpy(p, &x, 8); }
}
static int width_of(uint16_t format) { return (format == 0) ? 2 : (format == 1) ? 4 : 8; }
static int format_for(long maxval) {
    if (maxval <= 0xFFFFL) return 0;
    if (maxval <= 0xFFFFFFFFL) return 1;
    return 2;
}

typedef struct { int n, s; } info_t;
typedef struct { int section; long offset; char name[256]; int namelen; } exp_entry;

// ---------------- decode: COMPACT_GROUP an SIMPLE_FLAT -> brtyeli sia ----------------
static exp_entry *decode_export(const uint8_t *ep, int *out_count) {
    size_t p = 0;
    uint16_t format = rd16(ep + p); p += 2;
    uint16_t design = rd16(ep + p); p += 2;
    int w = width_of(format);

    exp_entry *out = malloc(sizeof(exp_entry) * 4096);
    int n = 0;

    if (design == 0) { // COMPACT_GROUP
        p += 2; // groups.format (uxmarad)
        long n_groups = readw(ep + p, w); p += w;

        long *gr_length = malloc(sizeof(long) * n_groups);
        long *gr_count  = malloc(sizeof(long) * n_groups);
        for (long g = 0; g < n_groups; g++) {
            gr_length[g] = rd16(ep + p); p += 2;
            gr_count[g]  = readw(ep + p, w); p += w;
        }
        for (long g = 0; g < n_groups; g++) {
            long base = n;
            for (long i = 0; i < gr_count[g]; i++) {
                uint8_t sb = ep[p]; p += 1;
                out[n].section = sb & 0x1F;
                out[n].offset = readw(ep + p, w); p += w;
                out[n].namelen = (int)gr_length[g];
                n++;
            }
            const uint8_t *name_data = ep + p;
            p += (size_t)gr_count[g] * gr_length[g];
            for (long i = 0; i < gr_count[g]; i++) {
                exp_entry *e = &out[base + i];
                memcpy(e->name, name_data + i * gr_length[g], gr_length[g]);
                e->name[gr_length[g]] = 0;
            }
        }
        free(gr_length); free(gr_count);
    } else if (design == 1) { // SIMPLE_FLAT
        p += 2; // names.format (uxmarad)
        long n_names = readw(ep + p, w); p += w;

        long *sec = malloc(sizeof(long) * n_names);
        long *len = malloc(sizeof(long) * n_names);
        long *orva = malloc(sizeof(long) * n_names);
        long *onam = malloc(sizeof(long) * n_names);
        for (long i = 0; i < n_names; i++) {
            uint16_t bits = rd16(ep + p); p += 2;
            sec[i] = bits & 0x1F;
            len[i] = (bits >> 5) & 0x7FF;
            orva[i] = readw(ep + p, w); p += w;
            onam[i] = readw(ep + p, w); p += w;
        }
        const uint8_t *name_data_base = ep + p; // blob dawyebulia array-is Semdeg, TvitkmaraT
        for (long i = 0; i < n_names; i++) {
            out[n].section = (int)sec[i];
            out[n].offset = orva[i];
            out[n].namelen = (int)len[i];
            memcpy(out[n].name, name_data_base + onam[i], len[i]);
            out[n].name[len[i]] = 0;
            n++;
        }
        free(sec); free(len); free(orva); free(onam);
    } else {
        fprintf(stderr,
            "am konverters ar uWers ORDINAL dizaini (design=2) -- gansxvavebuli,\n"
            "saxelis-gareSe (ordinal) keyspace-a, name<->ordinal gardaqmna\n"
            "arasrulad-gansazRvrulia. ix. README.md\n");
        exit(1);
    }
    *out_count = n;
    return out;
}

static int cmp_name(const void *a, const void *b) {
    const exp_entry *ea = a, *eb = b;
    int c = memcmp(ea->name, eb->name, ea->namelen < eb->namelen ? ea->namelen : eb->namelen);
    if (c) return c;
    return ea->namelen - eb->namelen;
}
static int cmp_len_then_name(const void *a, const void *b) {
    const exp_entry *ea = a, *eb = b;
    if (ea->namelen != eb->namelen) return ea->namelen - eb->namelen;
    return memcmp(ea->name, eb->name, ea->namelen);
}

// ---------------- encode: brtyeli sia -> COMPACT_GROUP ----------------
static uint8_t *encode_compact_group(exp_entry *entries, int count, long *out_size) {
    exp_entry *sorted = malloc(sizeof(exp_entry) * count);
    memcpy(sorted, entries, sizeof(exp_entry) * count);
    qsort(sorted, count, sizeof(exp_entry), cmp_len_then_name);

    // jgufebis mogonebis (length-is mixedviT)
    int *gr_length = malloc(sizeof(int) * count);
    int *gr_count = malloc(sizeof(int) * count);
    int n_groups = 0;
    for (int i = 0; i < count; ) {
        int len = sorted[i].namelen;
        int c = 0;
        while (i + c < count && sorted[i + c].namelen == len) c++;
        gr_length[n_groups] = len;
        gr_count[n_groups] = c;
        n_groups++;
        i += c;
    }

    long max_val = n_groups;
    for (int g = 0; g < n_groups; g++) if (gr_count[g] > max_val) max_val = gr_count[g];
    for (int e = 0; e < count; e++) if (sorted[e].offset > max_val) max_val = sorted[e].offset;
    uint16_t format = (uint16_t)format_for(max_val);
    int w = width_of(format);

    long sz = 4 + 2 + w; // outer(4) + groups.format(2) + groups.groups(w)
    for (int g = 0; g < n_groups; g++) sz += 2 + w; // group.length(2) + group.count(w)
    for (int e = 0; e < count; e++) sz += 1 + w;    // info: section(1) + offset(w)
    for (int g = 0; g < n_groups; g++) sz += (long)gr_count[g] * gr_length[g]; // name blobs

    uint8_t *nb = malloc(sz);
    size_t wp = 0;
    uint16_t fmt = format; memcpy(nb + wp, &fmt, 2); wp += 2;
    uint16_t des = 0; memcpy(nb + wp, &des, 2); wp += 2;
    uint16_t inner_fmt = 0; memcpy(nb + wp, &inner_fmt, 2); wp += 2;
    writew(nb + wp, n_groups, w); wp += w;

    for (int g = 0; g < n_groups; g++) {
        uint16_t len16 = (uint16_t)gr_length[g]; memcpy(nb + wp, &len16, 2); wp += 2;
        writew(nb + wp, gr_count[g], w); wp += w;
    }
    int base = 0;
    for (int g = 0; g < n_groups; g++) {
        for (int i = 0; i < gr_count[g]; i++) {
            exp_entry *e = &sorted[base + i];
            uint8_t sb = (uint8_t)(e->section & 0x1F);
            memcpy(nb + wp, &sb, 1); wp += 1;
            writew(nb + wp, e->offset, w); wp += w;
        }
        for (int i = 0; i < gr_count[g]; i++) {
            exp_entry *e = &sorted[base + i];
            memcpy(nb + wp, e->name, e->namelen); wp += e->namelen;
        }
        base += gr_count[g];
    }
    free(gr_length); free(gr_count); free(sorted);
    *out_size = sz;
    return nb;
}

// ---------------- encode: brtyeli sia -> SIMPLE_FLAT ----------------
static uint8_t *encode_simple_flat(exp_entry *entries, int count, long *out_size) {
    exp_entry *sorted = malloc(sizeof(exp_entry) * count);
    memcpy(sorted, entries, sizeof(exp_entry) * count);
    qsort(sorted, count, sizeof(exp_entry), cmp_name);

    long name_total = 0;
    for (int i = 0; i < count; i++) name_total += sorted[i].namelen;

    long max_val = count;
    for (int i = 0; i < count; i++) if (sorted[i].offset > max_val) max_val = sorted[i].offset;
    if (name_total > max_val) max_val = name_total;
    uint16_t format = (uint16_t)format_for(max_val);
    int w = width_of(format);

    long sz = 4 + 2 + w + (long)count * (2 + w + w) + name_total;
    uint8_t *nb = malloc(sz);
    size_t wp = 0;
    uint16_t fmt = format; memcpy(nb + wp, &fmt, 2); wp += 2;
    uint16_t des = 1; memcpy(nb + wp, &des, 2); wp += 2;
    uint16_t inner_fmt = 0; memcpy(nb + wp, &inner_fmt, 2); wp += 2;
    writew(nb + wp, count, w); wp += w;

    long running_name_off = 0;
    long *name_off = malloc(sizeof(long) * count);
    for (int i = 0; i < count; i++) { name_off[i] = running_name_off; running_name_off += sorted[i].namelen; }

    for (int i = 0; i < count; i++) {
        uint16_t bits = (uint16_t)((sorted[i].section & 0x1F) | ((sorted[i].namelen & 0x7FF) << 5));
        memcpy(nb + wp, &bits, 2); wp += 2;
        writew(nb + wp, sorted[i].offset, w); wp += w;
        writew(nb + wp, name_off[i], w); wp += w;
    }
    for (int i = 0; i < count; i++) { memcpy(nb + wp, sorted[i].name, sorted[i].namelen); wp += sorted[i].namelen; }

    free(name_off); free(sorted);
    *out_size = sz;
    return nb;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "gamoyeneba: %s <input.ueec> <output.ueec> --design compact|flat\n", argv[0]);
        return 1;
    }
    const char *in_path = argv[1], *out_path = argv[2];
    int out_design = 1; // default: SIMPLE_FLAT
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--design") && i + 1 < argc) {
            const char *d = argv[++i];
            if (!strcmp(d, "compact")) out_design = 0;
            else if (!strcmp(d, "flat")) out_design = 1;
            else { fprintf(stderr, "ucnobi design: %s (mxolod compact|flat)\n", d); return 1; }
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

    uint8_t *new_export_content[64];
    long new_export_size[64];
    int n_export_sections = 0;

    for (long s = 0; s < total_sections; s++) {
        if (sec_type[s] != UEEC_SECTION_EXPORT) continue;
        int count;
        exp_entry *entries = decode_export(buf + sec_off[s], &count);

        long sz;
        uint8_t *nb;
        if (out_design == 0) nb = encode_compact_group(entries, count, &sz);
        else nb = encode_simple_flat(entries, count, &sz);

        new_export_content[n_export_sections] = nb;
        new_export_size[n_export_sections] = sz;
        n_export_sections++;
        free(entries);
    }

    // ---------------- axali failis awyoba ----------------
    long name_table_start = sh_table_start + 6 * total_sections;
    long header_block = sec_size[0];

    long *new_sec_size = malloc(sizeof(long) * total_sections);
    int exp_idx = 0;
    for (long s = 0; s < total_sections; s++) {
        if (sec_type[s] == UEEC_SECTION_EXPORT) new_sec_size[s] = new_export_size[exp_idx++];
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

    exp_idx = 0;
    for (long s = 1; s < total_sections; s++) {
        if (sec_type[s] == UEEC_SECTION_EXPORT) {
            memcpy(out + wp, new_export_content[exp_idx], new_export_size[exp_idx]);
            wp += new_export_size[exp_idx];
            exp_idx++;
        } else {
            memcpy(out + wp, buf + sec_off[s], sec_size[s]);
            wp += sec_size[s];
        }
    }

    FILE *of = fopen(out_path, "wb");
    if (!of) { perror("fopen(out)"); return 1; }
    fwrite(out, 1, wp, of);
    fclose(of);

    const char *dname = out_design == 0 ? "COMPACT_GROUP" : "SIMPLE_FLAT";
    fprintf(stderr, "%s -> %s: design=%s, %d export-seqcia, %zu bytes sruli\n",
            in_path, out_path, dname, n_export_sections, wp);
    return 0;
}
