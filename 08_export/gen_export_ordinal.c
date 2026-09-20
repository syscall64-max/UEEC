// gen_export_ordinal.c -- erT-node-iani UEEC, ORDINAL export-iT
// (design=2) -- ordinal=1 -> "answer" (CODE, funqcia), ordinal=5 ->
// "counter" (DATA, int32=42). saxeli saerTod ar iwereba failSi.
// parametrizebulia arqiteqturis mixedviT: <code.bin> <UEEC_MACHINE_*> [be|le]
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    const char *code_path = argc > 1 ? argv[1] : "arch/answer_x86_64.bin";
    int machine = argc > 2 ? atoi(argv[2]) : 10; // UEEC_MACHINE_X86_64
    int be = (argc > 3 && !strcmp(argv[3], "be")) ? 1 : 0;

    FILE *f = fopen(code_path, "rb");
    if (!f) { fprintf(stderr, "ver gavxsen %s\n", code_path); return 1; }
    fseek(f, 0, SEEK_END); long code_size = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *code_buf = malloc(code_size);
    if (fread(code_buf, 1, code_size, f) != (size_t)code_size) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);

    int nodes = 1, names = 1;
    int total_sections = 4, total_info = 3;
    int header_block = 8 + 6*nodes + 6*total_info + 6*total_sections + 1*names + 2*nodes + 5;
    int data_size = 4; // counter=42
    // EXPORT: hdr(4)+count16(4)+2*ordinal16(5B)
    int export_size = 4 + 4 + 2*5;

    fprintf(stderr, "code=%s machine=%d endian=%s header_block=%d export_size=%d total_file=%d\n",
            code_path, machine, be ? "BE" : "LE",
            header_block, export_size, (int)(header_block+data_size+code_size+export_size));

    printf("; erT-node-iani UEEC export-demo -- ORDINAL (machine=%d)\n\n", machine);
    printf("db 0x5f,0x47,0x4d\ndb 0x01\n");
    printf("dw %d\ndw %d\n\n", nodes, names);

    {
        uint32_t v = 0;
        v |= 1 << 2; v |= 1 << 3; v |= 1 << 5;
        v |= (0 & 0x7) << 7;
        v |= ((uint32_t)machine & 0xFF) << 10;
        v |= (2 & 0x3) << 18;
        v |= (2 & 0x7F) << 20;
        v |= (3 & 0x1F) << 27;
        printf("dd 0x%08x\ndw 0x0000\n\n", v);
    }

    printf("dw 0\ndd 1\ndw 0\ndd 2\ndw 0\ndd 3\n\n");

    printf("dw 0\ndw %d\ndw %d ; HEADER\n", header_block, header_block);
    printf("dw 2\ndw %d\ndw %d ; DATA\n", data_size, data_size);
    printf("dw 1\ndw %ld\ndw %ld ; CODE\n", code_size, code_size);
    printf("dw 6\ndw %d\ndw %d ; EXPORT\n\n", export_size, export_size);

    printf("db 5\ndw 0\ndb 0x68,0x65,0x6c,0x6c,0x6f\n\n");

    printf("; DATA (counter=42) -- %s byte-order\n", be ? "BE" : "LE");
    {
        uint32_t u = 42;
        uint8_t b[4];
        if (be) { b[0]=(u>>24)&0xFF; b[1]=(u>>16)&0xFF; b[2]=(u>>8)&0xFF; b[3]=u&0xFF; }
        else    { b[0]=u&0xFF; b[1]=(u>>8)&0xFF; b[2]=(u>>16)&0xFF; b[3]=(u>>24)&0xFF; }
        printf("db 0x%02x,0x%02x,0x%02x,0x%02x\n", b[0], b[1], b[2], b[3]);
    }

    printf("; CODE (answer -> 42)\ndb ");
    for (long i = 0; i < code_size; i++) printf("0x%02x%s", code_buf[i], i==code_size-1?"\n":",");

    printf("; EXPORT (ORDINAL, ordinal-ით დალაგებული)\n");
    printf("dw 0 ; format=FORMAT_16\ndw 2 ; design=ORDINAL\n");
    printf("dw 0 ; count.format\ndw 2 ; count\n");
    // ordinal=1 -> answer (CODE,0)
    printf("db 0x01\ndw 0\ndw 1 ; ordinal=1 -> section=CODE offset=0\n");
    // ordinal=5 -> counter (DATA,0)
    printf("db 0x00\ndw 0\ndw 5 ; ordinal=5 -> section=DATA offset=0\n");

    return 0;
}
