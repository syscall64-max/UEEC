// gen_pe.c -- awarmoebs erT-node-ian UEEC-s, PE_STYLE RELOC-dizainiT
// (reloc_v4.h). section=CODE(target), source implicit == image_base
// (== local[0]==DATA-s mono_mem-Si, radgan DATA yovelTvis pirveli
// resident seqciaa).
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    const char *arch;
    int machine, encode, reloc_offset, reloc_size;
} arch_info;

static arch_info archs[] = {
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
#define N_ARCH (int)(sizeof(archs)/sizeof(archs[0]))

static uint8_t *load_bin(const char *arch, long *out_size) {
    char path[128]; snprintf(path, sizeof(path), "arch/%s.bin", arch);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "ver gavxsen %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(sz);
    if (fread(buf, 1, sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); exit(1); }
    fclose(f); *out_size = sz; return buf;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "gamoyeneba: %s <arch-index 0-10>\n", argv[0]); return 1; }
    int ai = atoi(argv[1]);
    if (ai < 0 || ai >= N_ARCH) { fprintf(stderr, "arasworia arch-index\n"); return 1; }
    arch_info a = archs[ai];

    long code_size;
    uint8_t *code_buf = load_bin(a.arch, &code_size);
    memset(code_buf + a.reloc_offset, 0, a.reloc_size); // addend=0

    const char *data_str = "Hello world!\n";
    int data_size = (int)strlen(data_str);

    int nodes = 1, names = 1;
    int total_sections = 3; // HEADER + DATA + CODE (RELOC calke, 4-e)
    total_sections = 4; // HEADER + DATA + CODE + RELOCATION
    int total_info = 3; // DATA + CODE + RELOC (HEADER ar iTvleba)

    int header_block = 8 + 6*nodes + 6*total_info + 6*total_sections + 1*names + 2*nodes + 5;

    // RELOC section: hdr(4) + couple16(4) + 1*block16(4) + 1*info16(2) = 14
    int reloc_size = 4 + 4 + 4 + 2;

    fprintf(stderr, "arch=%s header_block=%d reloc_size=%d total_file=%d\n",
            a.arch, header_block, reloc_size, (int)(header_block+data_size+code_size+reloc_size));

    printf("; PE_STYLE erT-node-iani UEEC -- %s\n\n", a.arch);
    // ueec_hdr
    printf("db 0x5f,0x47,0x4d\n");
    printf("db 0x01\n");
    printf("dw %d ; nodes\n", nodes);
    printf("dw %d ; names\n\n", names);
    // node_hdr[1]
    {
        uint32_t v = 0;
        v |= (a.encode & 1) << 0;
        v |= (1 & 1) << 2; // user_mode
        v |= (1 & 1) << 3; // program
        v |= (1 & 1) << 5; // console
        v |= (0 & 0x7) << 7; // machine_type=CPU
        v |= (a.machine & 0xFF) << 10;
        v |= (2 & 0x3) << 18; // system_type=GENERAL_OS
        v |= (2 & 0x7F) << 20; // system=LINUX
        v |= (3 & 0x1F) << 27; // section_count field=3 (real=4)
        printf("dd 0x%08x\n", v);
        printf("dw 0x0000 ; format=SH16\n\n");
    }
    // node_info[3]: DATA(1), CODE(2), RELOC(3)
    printf("; node_info\n");
    printf("dw 0\ndd 1\n");
    printf("dw 0\ndd 2\n");
    printf("dw 0\ndd 3\n\n");
    // sh16[4]
    printf("; sh16\n");
    printf("dw 0\ndw %d\ndw %d ; HEADER\n", header_block, header_block);
    printf("dw 2\ndw %d\ndw %d ; DATA\n", data_size, data_size);
    printf("dw 1\ndw %ld\ndw %ld ; CODE\n", code_size, code_size);
    printf("dw 5\ndw %d\ndw %d ; RELOCATION\n\n", reloc_size, reloc_size);
    // name table
    printf("db 5\ndw 0\ndb 0x68,0x65,0x6c,0x6c,0x6f\n\n");

    // DATA
    printf("; DATA\ndb ");
    for (int c = 0; c < data_size; c++) printf("0x%02x%s", (unsigned char)data_str[c], c==data_size-1?"\n":",");

    // CODE
    printf("; CODE\n");
    for (long i = 0; i < code_size; i++) {
        if (i % 16 == 0) printf("db ");
        printf("0x%02x%s", code_buf[i], (i%16==15 || i==code_size-1) ? "\n" : ",");
    }

    // RELOC (PE_STYLE): hdr(format,design=3) + couple16(section=CODE(local1),format=0,count=1)
    //                    + block16(block=0,count=1) + info16(offset,type)
    printf("; RELOCATION (PE_STYLE)\n");
    printf("dw 0 ; format\n");
    printf("dw 3 ; design=PE_STYLE\n");
    {
        uint16_t cpl = (uint16_t)((1 & 0x1F) | ((0 & 0x7FF) << 5)); // section=1(CODE local), format=0
        printf("dw 0x%04x ; section=1(CODE) format=0\n", cpl);
        printf("dw 1 ; couple.count = n_blocks\n");
    }
    printf("dw 0 ; block=0\n");
    printf("dw 1 ; block.count\n");
    {
        int type = (a.reloc_size == 8) ? 1 : 0;
        uint16_t v = (uint16_t)((a.reloc_offset & 0xFFF) | ((type & 0xF) << 12));
        printf("dw 0x%04x ; offset=%d type=%s\n", v, a.reloc_offset, type ? "ABS64" : "ABS32");
    }

    return 0;
}
