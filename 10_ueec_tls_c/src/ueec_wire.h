#ifndef UEEC_WIRE_H
#define UEEC_WIRE_H
#include <stddef.h>
#include <stdint.h>

#define UEEC_MAX_FILE (64u * 1024u * 1024u)
#define UEEC_MAX_NODES 1024u
#define UEEC_MAX_SECTIONS 32769u
#define UEEC_MAX_REFS 31u

typedef struct {
    uint32_t raw;
    uint16_t format;
    uint16_t name_len;
    const uint8_t *name;
    uint8_t ref_count;
    uint32_t refs[UEEC_MAX_REFS];
} UEECNode;

typedef struct {
    uint16_t kind;
    const uint8_t *data;
    size_t size;
} UEECSection;

typedef struct {
    size_t node_count;
    size_t section_count;
    UEECNode *nodes;
    UEECSection *sections;
} UEECDoc;

void ueec_free(UEECDoc *doc);
int ueec_parse(const uint8_t *data, size_t size, UEECDoc *out, char *err, size_t errlen);
int ueec_select(const UEECDoc *in, unsigned machine, unsigned system, UEECDoc *out, char *err, size_t errlen);
int ueec_pack(const UEECDoc *doc, unsigned granule, uint8_t **result, size_t *size, char *err, size_t errlen);
int ueec_read_file(const char *path, uint8_t **data, size_t *size);
int ueec_write_exclusive(const char *path, const uint8_t *data, size_t size);
#endif
