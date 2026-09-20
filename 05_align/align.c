//
// align.c -- kiTxulobs UEEC-s da titoeuli seqciis section_size-s
// afarToebs granularity-is jeradamde (gansxvavebiT data_size-isgan,
// romelic namdvili Semcvelobis zoma rCeba). granularity=1 niSnavs,
// rom section_size = data_size (padding-is gareSe).
//
// --granularity N   -- explicit
// (araferi)          -- sistemis sysconf(_SC_PAGESIZE)
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "ueec.h"

static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

typedef struct { int n, s; } info_t;

static long round_up(long v, long g) {
    if (g <= 1) return v;
    return ((v + g - 1) / g) * g;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "gamoyeneba: %s <input.ueec> <output.ueec> [--granularity N]\n", argv[0]);
        return 1;
    }
    const char *in_path = argv[1], *out_path = argv[2];
    long granularity = sysconf(_SC_PAGESIZE); // default: sistemis granularity
    for (int i = 3; i < argc; i++)
        if (!strcmp(argv[i], "--granularity") && i + 1 < argc) granularity = atol(argv[++i]);

    fprintf(stderr, "granularity = %ld%s\n", granularity,
            (argc <= 3) ? " (sistemis default, sysconf)" : " (explicit)");

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

    for (unsigned i = 0; i < nodes; i++) { off += 4; off += 2; }
    long total_info = 0;
    {
        // meored gavuaro, section_count-ebis mixedviT -- unda gavigoT
        // raodenoba jer sworad. (martivad: xelaxla gadavikiTxoT node_hdr)
        size_t off2 = 8;
        for (unsigned i = 0; i < nodes; i++) {
            uint32_t v = rd32(buf + off2);
            int sc = ((v >> 27) & 0x1F) + 1;
            total_info += sc - 1;
            off2 += 4; off2 += 2;
        }
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
    long *sec_data_size = malloc(sizeof(long) * total_sections);
    long sh_table_start = off;
    for (long s = 0; s < total_sections; s++) {
        sec_type[s] = rd16(buf + off); off += 2;
        sec_size[s] = rd16(buf + off); off += 2;
        sec_data_size[s] = rd16(buf + off); off += 2;
    }
    long name_table_start = sh_table_start + 6 * total_sections;
    long *sec_off = malloc(sizeof(long) * total_sections);
    sec_off[0] = 0;
    for (long s = 1; s < total_sections; s++) sec_off[s] = sec_off[s - 1] + sec_size[s - 1];

    // ---------------- axali section_size-ebis gaTvla ----------------
    // HEADER (section 0): data_size = namdvili wire-metadata zoma
    // (sec_data_size[0], romelic ukve swori unda iyos), section_size
    // ganaxldeba granularity-is mixedviT.
    long *new_size = malloc(sizeof(long) * total_sections);
    for (long s = 0; s < total_sections; s++) {
        long data_sz = sec_data_size[s]; // namdvili Semcveloba, ucvlelad rCeba
        new_size[s] = round_up(data_sz, granularity);
    }
    long *new_off = malloc(sizeof(long) * total_sections);
    new_off[0] = 0;
    for (long s = 1; s < total_sections; s++) new_off[s] = new_off[s - 1] + new_size[s - 1];
    long new_total = new_off[total_sections - 1] + new_size[total_sections - 1];

    fprintf(stderr, "seqciebi:\n");
    for (long s = 0; s < total_sections; s++)
        fprintf(stderr, "  [%ld] data_size=%ld -> section_size: %ld -> %ld\n",
                s, sec_data_size[s], sec_size[s], new_size[s]);

    uint8_t *out = calloc(1, new_total); // calloc -- padding avtomaturad 0-ebiT ivseba

    // header_block-is dasawyisi ucvlelia sh_table_start-mde
    memcpy(out, buf, sh_table_start);
    // magram HEADER section_size-c unda ganaxldes (out-Si jer arsebuli
    // header_block Zveli iyo, magram sinamdvileSi hdr/node_hdr/node_info
    // baitebi TviTon ar icvlebian -- mxolod sh16[]-Si sxvadasxva size iwereba)

    size_t wp = sh_table_start;
    for (long s = 0; s < total_sections; s++) {
        uint16_t t = (uint16_t)sec_type[s];
        uint16_t sz = (uint16_t)new_size[s];
        uint16_t dsz = (uint16_t)sec_data_size[s];
        memcpy(out + wp, &t, 2); wp += 2;
        memcpy(out + wp, &sz, 2); wp += 2;
        memcpy(out + wp, &dsz, 2); wp += 2;
    }
    // name-table -- ucvlelad
    long name_table_bytes = sec_off[1] - name_table_start; // = Zveli HEADER section_size-dan gamoklebuli
    memcpy(out + wp, buf + name_table_start, name_table_bytes);
    wp += name_table_bytes;

    // yoveli seqciis real content + padding (0-ebi ukve calloc-iT arian)
    for (long s = 1; s < total_sections; s++) {
        memcpy(out + new_off[s], buf + sec_off[s], sec_data_size[s]);
    }

    FILE *of = fopen(out_path, "wb");
    if (!of) { perror("fopen(out)"); return 1; }
    fwrite(out, 1, new_total, of);
    fclose(of);

    fprintf(stderr, "\n%s -> %s: %ld -> %ld bytes\n", in_path, out_path, fsize, new_total);
    return 0;
}
