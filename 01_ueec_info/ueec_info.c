#define _POSIX_C_SOURCE 200809L

#include "ueec.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    const uint8_t *data;
    size_t size;
} file_view;

typedef struct {
    uint16_t type;
    uint64_t section_size;
    uint64_t data_size;
} section_view;

typedef struct {
    size_t nodes;
    size_t names;
    unsigned version;
    unsigned endian;
    uint16_t section_format;
    size_t node_headers_offset;
    size_t node_info_offset;
    size_t node_info_count;
    size_t section_headers_offset;
    size_t section_count;
    size_t name_lengths_offset;
    size_t name_indexes_offset;
    size_t name_data_offset;
    size_t name_data_size;
    size_t metadata_size;
} ueec_view;

static int span_ok(const file_view *file, size_t offset, size_t length)
{
    return offset <= file->size && length <= file->size - offset;
}

static int add_size(size_t a, size_t b, size_t *result)
{
    if (a > SIZE_MAX - b) {
        return -1;
    }
    *result = a + b;
    return 0;
}

static int multiply_size(size_t a, size_t b, size_t *result)
{
    if (a != 0U && b > SIZE_MAX / a) {
        return -1;
    }
    *result = a * b;
    return 0;
}

static uint16_t read_u16(const uint8_t *p, unsigned endian)
{
    if (endian == UEEC_ENDIAN_LITTLE) {
        return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
    }
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t read_u32(const uint8_t *p, unsigned endian)
{
    if (endian == UEEC_ENDIAN_LITTLE) {
        return (uint32_t)p[0] |
               ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) |
               ((uint32_t)p[3] << 24);
    }
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static uint64_t read_u64(const uint8_t *p, unsigned endian)
{
    if (endian == UEEC_ENDIAN_LITTLE) {
        return (uint64_t)read_u32(p, endian) |
               ((uint64_t)read_u32(p + 4, endian) << 32);
    }
    return ((uint64_t)read_u32(p, endian) << 32) |
           (uint64_t)read_u32(p + 4, endian);
}

static size_t section_header_size(uint16_t format)
{
    switch (format) {
    case UEEC_FORMAT_SH16:
        return sizeof(ueec_sh16);
    case UEEC_FORMAT_SH32:
        return sizeof(ueec_sh32);
    case UEEC_FORMAT_SH64:
        return sizeof(ueec_sh64);
    default:
        return 0U;
    }
}

static const char *section_format_name(uint16_t format)
{
    switch (format) {
    case UEEC_FORMAT_SH16:
        return "SH16";
    case UEEC_FORMAT_SH32:
        return "SH32";
    case UEEC_FORMAT_SH64:
        return "SH64";
    default:
        return "UNKNOWN";
    }
}

static const char *machine_type_name(unsigned value)
{
    static const char *const names[] = {"CPU", "MPU", "VM", "GPU", "FPGA"};
    return value < sizeof(names) / sizeof(names[0]) ? names[value] : "unknown";
}

static const char *cpu_name(unsigned value)
{
    static const char *const names[] = {
        "arm32", "arm64", "loongarch64", "powerpc32", "powerpc64",
        "riscv32", "riscv64", "sparc32", "sparc64", "x86", "x86-64"
    };
    return value < sizeof(names) / sizeof(names[0]) ? names[value] : "unknown";
}

static const char *system_type_name(unsigned value)
{
    static const char *const names[] = {
        "bare-metal", "embedded", "general-os", "hypervisor"
    };
    return value < sizeof(names) / sizeof(names[0]) ? names[value] : "unknown";
}

static const char *general_os_name(unsigned value)
{
    static const char *const names[] = {
        "Android", "iOS", "Linux", "macOS", "Windows"
    };
    return value < sizeof(names) / sizeof(names[0]) ? names[value] : "unknown";
}

static const char *section_type_name(uint16_t value)
{
    static const char *const names[] = {
        "HEADER", "CODE", "INIT_DATA", "UNINIT_DATA", "CONSTANT_DATA",
        "RELOCATION", "EXPORT", "IMPORT", "DEBUG", "TLS", "STACK",
        "HEAP", "RESOURCE", "METADATA"
    };
    return value < sizeof(names) / sizeof(names[0]) ? names[value] : "UNKNOWN";
}

static uint32_t node_word(const file_view *file, const ueec_view *view,
                          size_t node)
{
    size_t offset = view->node_headers_offset + node * sizeof(ueec_node_hdr);
    return read_u32(file->data + offset, view->endian);
}

static uint16_t node_format(const file_view *file, const ueec_view *view,
                            size_t node)
{
    size_t offset = view->node_headers_offset + node * sizeof(ueec_node_hdr) + 4U;
    return read_u16(file->data + offset, view->endian);
}

static size_t node_section_count(const file_view *file, const ueec_view *view,
                                 size_t node)
{
    return (size_t)((node_word(file, view, node) >> 27) & UINT32_C(0x1f)) + 1U;
}

static uint16_t mapping_node(const file_view *file, const ueec_view *view,
                             size_t mapping)
{
    size_t offset = view->node_info_offset + mapping * sizeof(ueec_node_info);
    return read_u16(file->data + offset, view->endian);
}

static uint32_t mapping_section(const file_view *file, const ueec_view *view,
                                size_t mapping)
{
    size_t offset = view->node_info_offset + mapping * sizeof(ueec_node_info) + 2U;
    return read_u32(file->data + offset, view->endian);
}

static int read_section(const file_view *file, const ueec_view *view,
                        size_t index, section_view *section)
{
    size_t header_size = section_header_size(view->section_format);
    size_t offset = view->section_headers_offset + index * header_size;
    const uint8_t *p;

    if (header_size == 0U || !span_ok(file, offset, header_size)) {
        return -1;
    }
    p = file->data + offset;
    section->type = read_u16(p, view->endian);
    if (view->section_format == UEEC_FORMAT_SH16) {
        section->section_size = read_u16(p + 2, view->endian);
        section->data_size = read_u16(p + 4, view->endian);
    } else if (view->section_format == UEEC_FORMAT_SH32) {
        section->section_size = read_u32(p + 2, view->endian);
        section->data_size = read_u32(p + 6, view->endian);
    } else {
        section->section_size = read_u64(p + 2, view->endian);
        section->data_size = read_u64(p + 10, view->endian);
    }
    return 0;
}

static int parse_layout(const file_view *file, ueec_view *view)
{
    size_t bytes;
    size_t i;

    memset(view, 0, sizeof(*view));
    if (!span_ok(file, 0U, sizeof(ueec_hdr))) {
        fprintf(stderr, "error: file is smaller than ueec_hdr\n");
        return -1;
    }
    if (file->data[0] != UEEC_MAGIC_0 || file->data[1] != UEEC_MAGIC_1 ||
        file->data[2] != UEEC_MAGIC_2) {
        fprintf(stderr, "error: invalid _GM signature\n");
        return -1;
    }

    view->version = (unsigned)(file->data[3] & UINT8_C(0x7f));
    view->endian = (unsigned)(file->data[3] >> 7);
    view->nodes = read_u16(file->data + 4, view->endian);
    view->names = read_u16(file->data + 6, view->endian);
    view->node_headers_offset = sizeof(ueec_hdr);

    if (view->nodes == 0U || view->names == 0U || view->names > view->nodes) {
        fprintf(stderr, "error: invalid nodes or names count\n");
        return -1;
    }
    if (multiply_size(view->nodes, sizeof(ueec_node_hdr), &bytes) != 0 ||
        add_size(view->node_headers_offset, bytes, &view->node_info_offset) != 0 ||
        !span_ok(file, view->node_headers_offset, bytes)) {
        fprintf(stderr, "error: truncated node-header array\n");
        return -1;
    }

    view->section_format = node_format(file, view, 0U);
    if (section_header_size(view->section_format) == 0U) {
        fprintf(stderr, "error: unsupported section-header format\n");
        return -1;
    }
    for (i = 0U; i < view->nodes; ++i) {
        size_t count = node_section_count(file, view, i);
        if (node_format(file, view, i) != view->section_format) {
            fprintf(stderr,
                    "error: mixed node section formats need the final mixed-format wire rule\n");
            return -1;
        }
        if (count == 0U || count > UEEC_MAX_NODE_SECTIONS) {
            fprintf(stderr, "error: node %zu has invalid section_count\n", i);
            return -1;
        }
        if (add_size(view->node_info_count, count - 1U,
                     &view->node_info_count) != 0) {
            fprintf(stderr, "error: node-info count overflow\n");
            return -1;
        }
    }

    if (multiply_size(view->node_info_count, sizeof(ueec_node_info), &bytes) != 0 ||
        add_size(view->node_info_offset, bytes, &view->section_headers_offset) != 0 ||
        !span_ok(file, view->node_info_offset, bytes)) {
        fprintf(stderr, "error: truncated node-info array\n");
        return -1;
    }

    view->section_count = 1U;
    for (i = 0U; i < view->node_info_count; ++i) {
        uint16_t node = mapping_node(file, view, i);
        uint32_t section = mapping_section(file, view, i);
        if ((size_t)node >= view->nodes || section == UEEC_HEADER_SECTION_INDEX) {
            fprintf(stderr, "error: invalid node-info record %zu\n", i);
            return -1;
        }
        if ((uint64_t)section + UINT64_C(1) > (uint64_t)SIZE_MAX) {
            fprintf(stderr, "error: section index is too large for this host\n");
            return -1;
        }
        if ((size_t)section + 1U > view->section_count) {
            view->section_count = (size_t)section + 1U;
        }
    }

    if (multiply_size(view->section_count,
                      section_header_size(view->section_format), &bytes) != 0 ||
        add_size(view->section_headers_offset, bytes,
                 &view->name_lengths_offset) != 0 ||
        !span_ok(file, view->section_headers_offset, bytes)) {
        fprintf(stderr, "error: truncated section-header array\n");
        return -1;
    }
    if (add_size(view->name_lengths_offset, view->names,
                 &view->name_indexes_offset) != 0 ||
        !span_ok(file, view->name_lengths_offset, view->names)) {
        fprintf(stderr, "error: truncated name-length array\n");
        return -1;
    }
    if (multiply_size(view->nodes, sizeof(ueec_node_name_index), &bytes) != 0 ||
        add_size(view->name_indexes_offset, bytes, &view->name_data_offset) != 0 ||
        !span_ok(file, view->name_indexes_offset, bytes)) {
        fprintf(stderr, "error: truncated node-name-index array\n");
        return -1;
    }

    for (i = 0U; i < view->names; ++i) {
        uint8_t length = file->data[view->name_lengths_offset + i];
        if (length == 0U ||
            add_size(view->name_data_size, (size_t)length,
                     &view->name_data_size) != 0) {
            fprintf(stderr, "error: invalid name length\n");
            return -1;
        }
    }
    if (!span_ok(file, view->name_data_offset, view->name_data_size) ||
        add_size(view->name_data_offset, view->name_data_size,
                 &view->metadata_size) != 0) {
        fprintf(stderr, "error: truncated name data\n");
        return -1;
    }

    for (i = 0U; i < view->nodes; ++i) {
        uint16_t index = read_u16(file->data + view->name_indexes_offset + i * 2U,
                                  view->endian);
        size_t actual_mappings = 0U;
        size_t j;
        if ((size_t)index >= view->names) {
            fprintf(stderr, "error: node %zu has invalid name index\n", i);
            return -1;
        }
        for (j = 0U; j < view->node_info_count; ++j) {
            if ((size_t)mapping_node(file, view, j) == i) {
                ++actual_mappings;
            }
        }
        if (actual_mappings + 1U != node_section_count(file, view, i)) {
            fprintf(stderr, "error: node %zu section_count does not match mappings\n", i);
            return -1;
        }
    }

    return 0;
}

static size_t name_offset(const file_view *file, const ueec_view *view,
                          size_t name_index)
{
    size_t offset = view->name_data_offset;
    size_t i;
    for (i = 0U; i < name_index; ++i) {
        offset += file->data[view->name_lengths_offset + i];
    }
    return offset;
}

static void print_node_name(const file_view *file, const ueec_view *view,
                            size_t node)
{
    uint16_t index = read_u16(file->data + view->name_indexes_offset + node * 2U,
                              view->endian);
    unsigned length = file->data[view->name_lengths_offset + index];
    printf("%.*s", (int)length,
           (const char *)(file->data + name_offset(file, view, index)));
}

static int validate_sections(const file_view *file, const ueec_view *view)
{
    section_view header;
    uint64_t physical_end;
    size_t i;

    if (read_section(file, view, 0U, &header) != 0 ||
        header.type != UEEC_SECTION_HEADER) {
        fprintf(stderr, "error: global section 0 is not HEADER\n");
        return -1;
    }
    if (header.data_size != (uint64_t)view->metadata_size ||
        header.section_size < header.data_size ||
        header.section_size > (uint64_t)file->size) {
        fprintf(stderr, "error: invalid HEADER data_size or section_size\n");
        return -1;
    }
    physical_end = header.section_size;
    for (i = 1U; i < view->section_count; ++i) {
        section_view section;
        if (read_section(file, view, i, &section) != 0 ||
            section.section_size == 0U || section.data_size > section.section_size ||
            physical_end > UINT64_MAX - section.section_size) {
            fprintf(stderr, "error: invalid section %zu\n", i);
            return -1;
        }
        physical_end += section.section_size;
    }
    if (physical_end > (uint64_t)file->size) {
        fprintf(stderr, "error: physical sections exceed file size\n");
        return -1;
    }
    if (physical_end < (uint64_t)file->size) {
        fprintf(stderr, "warning: trailing bytes after the last section\n");
    }
    return 0;
}

static void print_information(const file_view *file, const ueec_view *view)
{
    size_t i;
    uint64_t physical_offset = 0U;

    printf("UEEC file\n");
    printf("  signature:          _GM\n");
    printf("  version:            %u\n", view->version);
    printf("  metadata endian:    %s\n",
           view->endian == UEEC_ENDIAN_LITTLE ? "little" : "big");
    printf("  file bytes:         %zu\n", file->size);
    printf("  nodes:              %zu\n", view->nodes);
    printf("  unique names:       %zu\n", view->names);
    printf("  node mappings:      %zu\n", view->node_info_count);
    printf("  global sections:    %zu\n", view->section_count);
    printf("  section format:     %s\n", section_format_name(view->section_format));
    printf("  HEADER data bytes:  %zu\n", view->metadata_size);

    printf("\nNodes\n");
    for (i = 0U; i < view->nodes; ++i) {
        uint32_t word = node_word(file, view, i);
        unsigned encode = (unsigned)(word & UINT32_C(1));
        unsigned kernel_mode = (unsigned)((word >> 1) & UINT32_C(1));
        unsigned user_mode = (unsigned)((word >> 2) & UINT32_C(1));
        unsigned program = (unsigned)((word >> 3) & UINT32_C(1));
        unsigned library = (unsigned)((word >> 4) & UINT32_C(1));
        unsigned console = (unsigned)((word >> 5) & UINT32_C(1));
        unsigned service = (unsigned)((word >> 6) & UINT32_C(1));
        unsigned machine_type = (unsigned)((word >> 7) & UINT32_C(7));
        unsigned machine = (unsigned)((word >> 10) & UINT32_C(0xff));
        unsigned system_type = (unsigned)((word >> 18) & UINT32_C(3));
        unsigned system = (unsigned)((word >> 20) & UINT32_C(0x7f));
        size_t j;

        printf("  [%zu] name=\"", i);
        print_node_name(file, view, i);
        printf("\" machine=%s/%s system=%s/%s code-endian=%s\n",
               machine_type_name(machine_type), cpu_name(machine),
               system_type_name(system_type), general_os_name(system),
               encode != 0U ? "big" : "little");
        printf("       flags: kernel=%u user=%u program=%u library=%u console=%u service=%u\n",
               kernel_mode, user_mode, program, library, console, service);
        printf("       sections: 0");
        for (j = 0U; j < view->node_info_count; ++j) {
            if ((size_t)mapping_node(file, view, j) == i) {
                printf(" %" PRIu32, mapping_section(file, view, j));
            }
        }
        putchar('\n');
    }

    printf("\nSections\n");
    for (i = 0U; i < view->section_count; ++i) {
        section_view section;
        uint64_t section_offset;
        size_t j;
        int first = 1;
        (void)read_section(file, view, i, &section);
        section_offset = i == 0U ? UINT64_C(0) : physical_offset;
        printf("  [%zu] type=%s offset=%" PRIu64
               " section_size=%" PRIu64 " data_size=%" PRIu64 " nodes=",
               i, section_type_name(section.type), section_offset,
               section.section_size, section.data_size);
        if (i == 0U) {
            printf("all (implicit)");
            physical_offset = section.section_size;
        } else {
            for (j = 0U; j < view->node_info_count; ++j) {
                if ((size_t)mapping_section(file, view, j) == i) {
                    printf("%s%u", first != 0 ? "" : ",",
                           (unsigned)mapping_node(file, view, j));
                    first = 0;
                }
            }
            if (first != 0) {
                printf("none");
            }
            physical_offset += section.section_size;
        }
        putchar('\n');
    }
}

int main(int argc, char **argv)
{
    int fd;
    struct stat status;
    void *mapping;
    file_view file;
    ueec_view view;
    int result = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s FILE.ueec\n", argv[0]);
        return 2;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open %s: %s\n", argv[1], strerror(errno));
        return 2;
    }
    if (fstat(fd, &status) == -1) {
        fprintf(stderr, "fstat %s: %s\n", argv[1], strerror(errno));
        (void)close(fd);
        return 2;
    }
    if (status.st_size <= 0 || (uintmax_t)status.st_size > (uintmax_t)SIZE_MAX) {
        fprintf(stderr, "error: unsupported file size\n");
        (void)close(fd);
        return 2;
    }

    mapping = mmap(NULL, (size_t)status.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping == MAP_FAILED) {
        fprintf(stderr, "mmap %s: %s\n", argv[1], strerror(errno));
        (void)close(fd);
        return 2;
    }
    (void)close(fd);

    file.data = mapping;
    file.size = (size_t)status.st_size;
    if (parse_layout(&file, &view) == 0 && validate_sections(&file, &view) == 0) {
        print_information(&file, &view);
        result = 0;
    }
    if (munmap(mapping, file.size) == -1) {
        fprintf(stderr, "munmap: %s\n", strerror(errno));
        result = 2;
    }
    return result;
}
