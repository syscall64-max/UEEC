//
// extract.c -- mravali-node-iani UEEC-dan amoigebs erT node-s (saxelis,
// architecture-type-is, architecture-is, system-type-is, system-is, an
// maTi kombinaciis mixedviT) da qmnis axal, calke UEEC fails.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ueec.h"

static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

typedef struct { int n, s; } info_t;

typedef struct {
    uint32_t raw;
    int machine_type, machine, system_type, system, section_count;
} node_t;

static node_t parse_node_hdr(uint32_t v) {
    node_t n;
    n.raw = v;
    n.machine_type = (v >> 7) & 0x7;
    n.machine = (v >> 10) & 0xFF;
    n.system_type = (v >> 18) & 0x3;
    n.system = (v >> 20) & 0x7F;
    n.section_count = ((v >> 27) & 0x1F) + 1;
    return n;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr,
            "gamoyeneba: %s <input.ueec> <output.ueec> [--name N] [--machine-type T]\n"
            "            [--machine M] [--system-type ST] [--system S]\n", argv[0]);
        return 1;
    }
    const char *in_path = argv[1], *out_path = argv[2];
    const char *want_name = NULL;
    int want_mt = -1, want_m = -1, want_st = -1, want_s = -1;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--name") && i+1 < argc) want_name = argv[++i];
        else if (!strcmp(argv[i], "--machine-type") && i+1 < argc) want_mt = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--machine") && i+1 < argc) want_m = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--system-type") && i+1 < argc) want_st = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--system") && i+1 < argc) want_s = atoi(argv[++i]);
        else { fprintf(stderr, "ucnobi parametri: %s\n", argv[i]); return 1; }
    }
    if (!want_name && want_mt<0 && want_m<0 && want_st<0 && want_s<0) {
        fprintf(stderr, "sul mcire erTi selektori aucilebelia\n"); return 1;
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
    unsigned names = rd16(buf + 6);
    size_t off = 8;

    node_t *node = malloc(sizeof(node_t) * nodes);
    long total_info = 0;
    for (unsigned i = 0; i < nodes; i++) {
        node[i] = parse_node_hdr(rd32(buf + off));
        total_info += node[i].section_count - 1;
        off += 4; off += 2; // format
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
    for (long s = 0; s < total_sections; s++) {
        sec_type[s] = rd16(buf + off); off += 2;
        sec_size[s] = rd16(buf + off); off += 2;
        off += 2; // data_size
    }
    long *sec_off = malloc(sizeof(long) * total_sections);
    sec_off[0] = 0;
    for (long s = 1; s < total_sections; s++) sec_off[s] = sec_off[s-1] + sec_size[s-1];

    // ---------------- saxelebis cxrili ----------------
    uint8_t *name_len = malloc(names);
    for (unsigned i = 0; i < names; i++) name_len[i] = buf[off++];
    uint16_t *node_name_idx = malloc(sizeof(uint16_t) * nodes);
    for (unsigned i = 0; i < nodes; i++) { node_name_idx[i] = rd16(buf + off); off += 2; }
    long *name_data_off = malloc(sizeof(long) * names);
    long cur = off;
    for (unsigned i = 0; i < names; i++) { name_data_off[i] = cur; cur += name_len[i]; }

    // ---------------- shesabamisi node(ebi)-s Zieba ----------------
    int matches[64], n_matches = 0;
    for (unsigned i = 0; i < nodes; i++) {
        if (want_mt >= 0 && node[i].machine_type != want_mt) continue;
        if (want_m >= 0 && node[i].machine != want_m) continue;
        if (want_st >= 0 && node[i].system_type != want_st) continue;
        if (want_s >= 0 && node[i].system != want_s) continue;
        if (want_name) {
            int ni = node_name_idx[i];
            if ((int)strlen(want_name) != name_len[ni] ||
                memcmp(want_name, buf + name_data_off[ni], name_len[ni]) != 0) continue;
        }
        matches[n_matches++] = i;
    }
    if (n_matches == 0) { fprintf(stderr, "node ver moidzebna\n"); return 1; }
    if (n_matches > 1) {
        fprintf(stderr, "gaugebrobaa: %d node emTxveva selektors (id-ebi:", n_matches);
        for (int k = 0; k < n_matches; k++) fprintf(stderr, " %d", matches[k]);
        fprintf(stderr, ") -- daazuste selektorebi\n");
        return 1;
    }
    int chosen = matches[0];
    fprintf(stderr, "archeulia: node[%d] (machine_type=%d machine=%d system_type=%d system=%d)\n",
            chosen, node[chosen].machine_type, node[chosen].machine,
            node[chosen].system_type, node[chosen].system);

    // ---------------- am node-is sakuTari seqciebis (HEADER-is gareSe) povna ----------------
    int own_sections[8], n_own = 0;
    for (long i = 0; i < total_info; i++) if (info[i].n == chosen) own_sections[n_own++] = info[i].s;

    // ---------------- axali, erTi-node-iani failis awyoba ----------------
    int out_names = 1;
    long chosen_name_len = node_name_idx[chosen] < names ? name_len[node_name_idx[chosen]] : 0;
    const uint8_t *chosen_name_data = buf + name_data_off[node_name_idx[chosen]];

    long out_total_sections = 1 + n_own; // HEADER + own
    long out_total_info = n_own;
    long out_header_block =
        8 + 6*1 + 6*out_total_info + 6*out_total_sections + 1*out_names + 2*1 + chosen_name_len;

    long *out_sec_size = malloc(sizeof(long) * out_total_sections);
    int *out_sec_type = malloc(sizeof(int) * out_total_sections);
    out_sec_size[0] = out_header_block; out_sec_type[0] = UEEC_SECTION_HEADER;
    for (int i = 0; i < n_own; i++) {
        out_sec_size[1+i] = sec_size[own_sections[i]];
        out_sec_type[1+i] = sec_type[own_sections[i]];
    }
    long *out_sec_off = malloc(sizeof(long) * out_total_sections);
    out_sec_off[0] = 0;
    for (long s = 1; s < out_total_sections; s++) out_sec_off[s] = out_sec_off[s-1] + out_sec_size[s-1];
    long out_total_size = out_sec_off[out_total_sections-1] + out_sec_size[out_total_sections-1];

    uint8_t *out = malloc(out_total_size);
    size_t wp = 0;

    // hdr
    out[wp++] = UEEC_MAGIC_0; out[wp++] = UEEC_MAGIC_1; out[wp++] = UEEC_MAGIC_2;
    out[wp++] = buf[3]; // version+endian igive
    uint16_t n1 = 1; memcpy(out+wp,&n1,2); wp+=2;
    uint16_t nm1 = (uint16_t)out_names; memcpy(out+wp,&nm1,2); wp+=2;

    // node_hdr[1]
    uint32_t raw = node[chosen].raw;
    raw &= ~(0x1FU << 27); // section_count moaxle
    raw |= (uint32_t)((out_total_sections - 1) & 0x1F) << 27;
    memcpy(out+wp,&raw,4); wp+=4;
    uint16_t fmt0 = 0; memcpy(out+wp,&fmt0,2); wp+=2;

    // node_info[n_own] -- yvela {node=0, section=1+i}
    for (int i = 0; i < n_own; i++) {
        uint16_t nd = 0; memcpy(out+wp,&nd,2); wp+=2;
        uint32_t sc = (uint32_t)(1+i); memcpy(out+wp,&sc,4); wp+=4;
    }

    // sh16[out_total_sections]
    for (long s = 0; s < out_total_sections; s++) {
        uint16_t t = (uint16_t)out_sec_type[s]; memcpy(out+wp,&t,2); wp+=2;
        uint16_t sz = (uint16_t)out_sec_size[s]; memcpy(out+wp,&sz,2); wp+=2;
        memcpy(out+wp,&sz,2); wp+=2;
    }

    // name table
    out[wp++] = (uint8_t)chosen_name_len;
    uint16_t nidx0 = 0; memcpy(out+wp,&nidx0,2); wp+=2;
    memcpy(out+wp, chosen_name_data, chosen_name_len); wp += chosen_name_len;

    // sruli seqciebi
    for (int i = 0; i < n_own; i++) {
        memcpy(out+wp, buf + sec_off[own_sections[i]], sec_size[own_sections[i]]);
        wp += sec_size[own_sections[i]];
    }

    FILE *of = fopen(out_path, "wb");
    if (!of) { perror("fopen(out)"); return 1; }
    fwrite(out, 1, wp, of);
    fclose(of);

    fprintf(stderr, "%s -> %s: %d seqcia amoRebulia, %zu bytes sruli\n",
            in_path, out_path, n_own, wp);
    return 0;
}
