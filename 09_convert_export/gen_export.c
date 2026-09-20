// gen_export.c -- erT-node-iani UEEC, COMPACT_GROUP export-iT --
// jgufebi sigrZiT (3,6,7), TiToeulis SigniT saxeliT dalagebuli entry-ebi.
// parametrizebulia arqiteqturis mixedviT: <code.bin> <UEEC_MACHINE_*> [be|le]
// (default: arch/answer_x86_64.bin, machine=10 (X86_64), le -- ZveltandeliviT).
//
// jgufi length=3: "bar"(DATA off=4), "foo"(DATA off=8)
// jgufi length=6: "answer"(CODE off=0), "banana"(DATA off=12)
// jgufi length=7: "counter"(DATA off=0)
//
// !!! DATA-Si int32 mniSvnelobebi iwereba target-arqiteqturis
//     bunebrivi byte-order-iT (BE arqiteqturebze -- BE), radgan loader
//     am mniSvnelobebs pirdapir memcpy-iT kiTxulobs, struqturuli
//     format-veliT ki ar aris marTuli (ix. README, "DATA content-ის
//     endianness" cnobili baga, RELOC/EXPORT-idan xelaxla gamoyenebuli
//     gakvetili) !!!
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

    int data_size = 16; // counter,bar,foo,banana -- titoeuli 4B

    int export_size = 4 + 4 + 3*4 + (6+6) + (6+12) + (3+7);

    fprintf(stderr, "code=%s machine=%d endian=%s header_block=%d export_size=%d total_file=%d\n",
            code_path, machine, be ? "BE" : "LE",
            header_block, export_size, (int)(header_block+data_size+code_size+export_size));

    printf("; erT-node-iani UEEC export-demo -- COMPACT_GROUP (machine=%d)\n\n", machine);
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

    printf("; DATA: counter=42(0) bar=11(4) foo=22(8) banana=33(12) -- %s byte-order\n", be ? "BE" : "LE");
    {
        int32_t vals[4] = {42, 11, 22, 33};
        printf("db ");
        for (int i = 0; i < 4; i++) {
            uint8_t b[4];
            uint32_t u = (uint32_t)vals[i];
            if (be) { b[0]=(u>>24)&0xFF; b[1]=(u>>16)&0xFF; b[2]=(u>>8)&0xFF; b[3]=u&0xFF; }
            else    { b[0]=u&0xFF; b[1]=(u>>8)&0xFF; b[2]=(u>>16)&0xFF; b[3]=(u>>24)&0xFF; }
            for (int k = 0; k < 4; k++) printf("0x%02x%s", b[k], (i==3 && k==3) ? "\n" : ",");
        }
    }

    printf("; CODE (answer -> 42)\ndb ");
    for (long i = 0; i < code_size; i++) printf("0x%02x%s", code_buf[i], i==code_size-1?"\n":",");

    printf("; EXPORT (COMPACT_GROUP)\n");
    printf("dw 0 ; format=FORMAT_16\ndw 0 ; design=COMPACT_GROUP\n");
    printf("dw 0 ; groups.format\ndw 3 ; groups.count\n");
    printf("dw 3\ndw 2 ; group[0]: length=3 count=2\n");
    printf("dw 6\ndw 2 ; group[1]: length=6 count=2\n");
    printf("dw 7\ndw 1 ; group[2]: length=7 count=1\n\n");

    printf("; group[0] (length=3): bar, foo\n");
    printf("db 0x00\ndw 4 ; bar -> DATA+4\n");
    printf("db 0x00\ndw 8 ; foo -> DATA+8\n");
    printf("db 0x62,0x61,0x72,0x66,0x6f,0x6f ; \"bar\"+\"foo\"\n\n");

    printf("; group[1] (length=6): answer, banana\n");
    printf("db 0x01\ndw 0 ; answer -> CODE+0\n");
    printf("db 0x00\ndw 12 ; banana -> DATA+12\n");
    printf("db 0x61,0x6e,0x73,0x77,0x65,0x72,0x62,0x61,0x6e,0x61,0x6e,0x61 ; \"answer\"+\"banana\"\n\n");

    printf("; group[2] (length=7): counter\n");
    printf("db 0x00\ndw 0 ; counter -> DATA+0\n");
    printf("db 0x63,0x6f,0x75,0x6e,0x74,0x65,0x72 ; \"counter\"\n");

    return 0;
}
