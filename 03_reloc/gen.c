// gen.c -- აწარმოებს 03_reloc.asm-ს: 1 HEADER + 1 DATA("Hello world!\n",
// გაზიარებული) + 11 x (CODE+RELOCATION), ახალი _GM header-ფორმატით,
// SECTION_FLAT (couple+info, page-ის გარეშე) RELOC-დიზაინით.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define N 11

typedef struct {
    const char *arch;
    int machine;
    int encode;      // 0=LE, 1=BE
    int reloc_offset;
    int reloc_size;  // 4 ან 8
} arch_info;

static arch_info archs[N] = {
    {"arm32",       0, 0, 32, 4},
    {"arm64",       1, 0, 32, 8},
    {"longarm64",   2, 0, 36, 8},
    {"powerpc32",   3, 1, 40, 4},
    {"powerpc64",   4, 1, 40, 8},
    {"riscv32",     5, 0, 36, 4},
    {"riscv64",     6, 0, 36, 8},
    {"sparc32",     7, 1, 40, 4},
    {"sparc64",     8, 1, 40, 8},
    {"x86",         9, 0,  1, 4},
    {"x86_64",     10, 0,  2, 8},
};

static uint8_t *load_bin(const char *arch, long *out_size) {
    char path[128];
    snprintf(path, sizeof(path), "arch/%s.bin", arch);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "ver gavxsen %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(sz);
    if (fread(buf, 1, sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); exit(1); }
    fclose(f);
    *out_size = sz;
    return buf;
}

static void emit_bytes(const uint8_t *buf, long n) {
    for (long i = 0; i < n; i++) {
        if (i % 16 == 0) printf("db ");
        printf("0x%02x%s", buf[i], (i % 16 == 15 || i == n - 1) ? "\n" : ",");
    }
    if (n == 0) printf("\n");
}

int main(void) {
    long code_size[N];
    uint8_t *code_buf[N];
    for (int i = 0; i < N; i++) code_buf[i] = load_bin(archs[i].arch, &code_size[i]);

    const char *data_str = "Hello world!\n";
    int data_size = (int)strlen(data_str); // 13, null aRc gvWirdeba

    // addend = 0 yvela architecture-Tvis, radgan yvela erT saerTo
    // striqons (offset=0 DATA-Si) miuTiTebs.
    for (int i = 0; i < N; i++) {
        memset(code_buf[i] + archs[i].reloc_offset, 0, archs[i].reloc_size);
    }

    // ---------------- header_block zomis gaTvla ----------------
    int nodes = N;
    int names = 1;               // yvela nodes-s saerTo saxeli "hello"
    int total_sections = 2 + N*2; // HEADER + DATA + (CODE+RELOC) x N
    int total_info_entries = N*3; // tito node-s: DATA + own CODE + own RELOC

    int header_block =
        8 +                        // ueec_hdr
        6 * nodes +                // ueec_node_hdr[]
        6 * total_info_entries +   // ueec_node_info[]
        6 * total_sections +       // ueec_sh16[] (yvela sekcia SH16 formats)
        1 * names +                // name_len[]
        2 * nodes +                // node_name_index[]
        5;                         // name_data ("hello")

    // ---------------- section offset/zomebi ----------------
    long *sec_size = malloc(sizeof(long) * total_sections);
    int *sec_type = malloc(sizeof(int) * total_sections);
    sec_size[0] = header_block; sec_type[0] = 0; // HEADER
    sec_size[1] = data_size;    sec_type[1] = 2; // INIT_DATA
    // RELOC section zoma: reloc_hdr(4) + couple16(4) + 1*info(2) = 10
    int reloc_section_size = 4 + 4 + 1*2;
    for (int i = 0; i < N; i++) {
        sec_size[2+i*2]   = code_size[i]; sec_type[2+i*2]   = 1; // CODE
        sec_size[2+i*2+1] = reloc_section_size; sec_type[2+i*2+1] = 5; // RELOCATION
    }
    long *sec_off = malloc(sizeof(long) * total_sections);
    sec_off[0] = 0;
    for (int s = 1; s < total_sections; s++) sec_off[s] = sec_off[s-1] + sec_size[s-1];

    fprintf(stderr, "header_block=%d data_size=%d total_sections=%d total_info=%d\n",
            header_block, data_size, total_sections, total_info_entries);
    fprintf(stderr, "total file size = %ld\n", sec_off[total_sections-1] + sec_size[total_sections-1]);

    // ================================================================
    printf("; avtomaturad generirebuli -- 03_reloc.asm (_GM header, SECTION_FLAT RELOC)\n\n");

    // ---- ueec_hdr ----
    printf("; ueec_hdr\n");
    printf("db 0x5f,0x47,0x4d ; magic _GM\n");
    printf("db 0x01 ; version=1(bit0-6), endian=0(bit7, LE)\n");
    printf("dw %d ; nodes\n", nodes);
    printf("dw %d ; names\n\n", names);

    // ---- ueec_node_hdr[N] ----
    printf("; ueec_node_hdr[%d]\n", N);
    for (int i = 0; i < N; i++) {
        uint32_t v = 0;
        v |= (archs[i].encode & 1) << 0;
        v |= (0 & 1) << 1;   // kernel_mode
        v |= (1 & 1) << 2;   // user_mode
        v |= (1 & 1) << 3;   // program
        v |= (0 & 1) << 4;   // library
        v |= (1 & 1) << 5;   // console
        v |= (0 & 1) << 6;   // service
        v |= (0 & 0x7) << 7; // machine_type=CPU
        v |= (archs[i].machine & 0xFF) << 10;
        v |= (2 & 0x3) << 18; // system_type=GENERAL_OS
        v |= (2 & 0x7F) << 20; // system=LINUX
        v |= (3 & 0x1F) << 27; // section_count field = 3 (real=4: HEADER+DATA+CODE+RELOC)
        printf("dd 0x%08x ; node[%d] %s section_count_field=3\n", v, i, archs[i].arch);
        printf("dw 0x0000 ; format=SH16\n");
    }
    printf("\n");

    // ---- ueec_node_info[] ----
    printf("; ueec_node_info[%d]\n", total_info_entries);
    for (int i = 0; i < N; i++) {
        int code_g = 2+i*2, reloc_g = 2+i*2+1;
        int refs[3] = {1, code_g, reloc_g}; // DATA(shared), CODE(own), RELOC(own)
        for (int k = 0; k < 3; k++) {
            printf("dw %d\n", i);        // node
            printf("dd %d\n", refs[k]);  // section
        }
    }
    printf("\n");

    // ---- section headers (SH16 hyvelgan) ----
    printf("; ueec_sh16[%d]\n", total_sections);
    for (int s = 0; s < total_sections; s++) {
        printf("dw %d ; type\n", sec_type[s]);
        printf("dw %ld ; section_size\n", sec_size[s]);
        printf("dw %ld ; data_size\n", sec_size[s]);
    }
    printf("\n");

    // ---- name table ----
    printf("; name table\n");
    printf("db 5 ; name_len[0]\n");
    for (int i = 0; i < N; i++) printf("dw 0 ; node_name_index[%d]\n", i);
    printf("db 0x68,0x65,0x6c,0x6c,0x6f ; \"hello\"\n");
    printf("\n; ===== header block ends here (%d bytes) =====\n\n", header_block);

    // ---- DATA section ----
    printf("; ===== DATA (%d bytes): \"Hello world!\\n\" =====\n", data_size);
    printf("db ");
    for (int c = 0; c < data_size; c++)
        printf("0x%02x%s", (unsigned char)data_str[c], c == data_size-1 ? "\n" : ",");
    printf("\n");

    // ---- CODE + RELOCATION titoeuli architecture-Tvis ----
    for (int i = 0; i < N; i++) {
        printf("; ===== node[%d] %s: CODE (%ld bytes) =====\n", i, archs[i].arch, code_size[i]);
        emit_bytes(code_buf[i], code_size[i]);

        printf("; ===== node[%d] %s: RELOCATION SECTION_FLAT (%d bytes) =====\n",
               i, archs[i].arch, reloc_section_size);
        // ueec_reloc: format(u16)+design(u16) -- orive PLAIN, ara bitfield
        printf("dw 0 ; format\n");
        printf("dw 0 ; design=SECTION_FLAT\n");
        // ueec_reloc_couple16: target:5,source:5,format:6 (erT sityvaSi) + count(u16)
        {
            uint16_t w = (uint16_t)((1 & 0x1F) | ((0 & 0x1F) << 5) | ((0 & 0x3F) << 10));
            printf("dw 0x%04x ; target=1(CODE) source=0(DATA) format=0\n", w);
            printf("dw 1 ; count\n");
        }
        // ueec_reloc_info: offset:12,type:4
        {
            int type = (archs[i].reloc_size == 8) ? 1 : 0;
            uint16_t v = (uint16_t)((archs[i].reloc_offset & 0xFFF) | ((type & 0xF) << 12));
            printf("dw 0x%04x ; offset=%d type=%s\n\n", v, archs[i].reloc_offset,
                   type ? "ABS64" : "ABS32");
        }
    }

    return 0;
}
