#define _POSIX_C_SOURCE 200809L

#include "ueec.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
    LOAD_AUTO,
    LOAD_MONOLITHIC,
    LOAD_SCATTERED,
    LOAD_HYBRID
} load_mode;

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

typedef struct {
    void *address;
    size_t size;
} mapped_region;

typedef struct {
    mapped_region regions[UEEC_MAX_NODE_SECTIONS];
    size_t region_count;
    uintptr_t section_address[UEEC_MAX_NODE_SECTIONS];
    size_t allocation_size[UEEC_MAX_NODE_SECTIONS];
    size_t global_section[UEEC_MAX_NODE_SECTIONS];
    section_view section[UEEC_MAX_NODE_SECTIONS];
    size_t local_count;
    size_t runtime_data_size;
    void *entry;
    load_mode actual_mode;
} loaded_image;

static int span_ok(size_t total, size_t offset, size_t length)
{
    return offset <= total && length <= total - offset;
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

static int align_up(size_t value, size_t alignment, size_t *result)
{
    size_t remainder;
    size_t addition;
    if (alignment == 0U) {
        return -1;
    }
    remainder = value % alignment;
    addition = remainder == 0U ? 0U : alignment - remainder;
    return add_size(value, addition, result);
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
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
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

static void write_u16(uint8_t *p, uint16_t value, unsigned endian)
{
    if (endian == UEEC_ENDIAN_LITTLE) {
        p[0] = (uint8_t)value;
        p[1] = (uint8_t)(value >> 8);
    } else {
        p[0] = (uint8_t)(value >> 8);
        p[1] = (uint8_t)value;
    }
}

static void write_u32(uint8_t *p, uint32_t value, unsigned endian)
{
    if (endian == UEEC_ENDIAN_LITTLE) {
        p[0] = (uint8_t)value;
        p[1] = (uint8_t)(value >> 8);
        p[2] = (uint8_t)(value >> 16);
        p[3] = (uint8_t)(value >> 24);
    } else {
        p[0] = (uint8_t)(value >> 24);
        p[1] = (uint8_t)(value >> 16);
        p[2] = (uint8_t)(value >> 8);
        p[3] = (uint8_t)value;
    }
}

static void write_u64(uint8_t *p, uint64_t value, unsigned endian)
{
    if (endian == UEEC_ENDIAN_LITTLE) {
        write_u32(p, (uint32_t)value, endian);
        write_u32(p + 4, (uint32_t)(value >> 32), endian);
    } else {
        write_u32(p, (uint32_t)(value >> 32), endian);
        write_u32(p + 4, (uint32_t)value, endian);
    }
}

static size_t section_header_size(uint16_t format)
{
    switch (format) {
    case UEEC_FORMAT_SH16: return sizeof(ueec_sh16);
    case UEEC_FORMAT_SH32: return sizeof(ueec_sh32);
    case UEEC_FORMAT_SH64: return sizeof(ueec_sh64);
    default: return 0U;
    }
}

static uint32_t node_word(const file_view *file, const ueec_view *view,
                          size_t node)
{
    return read_u32(file->data + view->node_headers_offset +
                    node * sizeof(ueec_node_hdr), view->endian);
}

static uint16_t node_format(const file_view *file, const ueec_view *view,
                            size_t node)
{
    return read_u16(file->data + view->node_headers_offset +
                    node * sizeof(ueec_node_hdr) + 4U, view->endian);
}

static size_t node_section_count(const file_view *file, const ueec_view *view,
                                 size_t node)
{
    return (size_t)((node_word(file, view, node) >> 27) & UINT32_C(0x1f)) + 1U;
}

static uint16_t mapping_node(const file_view *file, const ueec_view *view,
                             size_t mapping)
{
    return read_u16(file->data + view->node_info_offset +
                    mapping * sizeof(ueec_node_info), view->endian);
}

static uint32_t mapping_section(const file_view *file, const ueec_view *view,
                                size_t mapping)
{
    return read_u32(file->data + view->node_info_offset +
                    mapping * sizeof(ueec_node_info) + 2U, view->endian);
}

static int get_section(const file_view *file, const ueec_view *view,
                       size_t index, section_view *section)
{
    size_t size = section_header_size(view->section_format);
    size_t offset = view->section_headers_offset + index * size;
    const uint8_t *p;
    if (index >= view->section_count || size == 0U ||
        !span_ok(file->size, offset, size)) {
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

static int section_file_offset(const file_view *file, const ueec_view *view,
                               size_t wanted, uint64_t *offset)
{
    uint64_t value = 0U;
    size_t i;
    for (i = 0U; i < wanted; ++i) {
        section_view section;
        if (get_section(file, view, i, &section) != 0 ||
            value > UINT64_MAX - section.section_size) {
            return -1;
        }
        value += section.section_size;
    }
    *offset = value;
    return 0;
}

static int parse_ueec(const file_view *file, ueec_view *view)
{
    size_t bytes;
    size_t i;
    section_view header;
    uint64_t physical_end;

    memset(view, 0, sizeof(*view));
    if (!span_ok(file->size, 0U, sizeof(ueec_hdr)) ||
        file->data[0] != UEEC_MAGIC_0 || file->data[1] != UEEC_MAGIC_1 ||
        file->data[2] != UEEC_MAGIC_2) {
        fprintf(stderr, "error: invalid or truncated _GM header\n");
        return -1;
    }
    view->version = (unsigned)(file->data[3] & UINT8_C(0x7f));
    view->endian = (unsigned)(file->data[3] >> 7);
    view->nodes = read_u16(file->data + 4, view->endian);
    view->names = read_u16(file->data + 6, view->endian);
    view->node_headers_offset = sizeof(ueec_hdr);
    if (view->nodes == 0U || view->names == 0U || view->names > view->nodes ||
        multiply_size(view->nodes, sizeof(ueec_node_hdr), &bytes) != 0 ||
        add_size(view->node_headers_offset, bytes, &view->node_info_offset) != 0 ||
        !span_ok(file->size, view->node_headers_offset, bytes)) {
        fprintf(stderr, "error: invalid node metadata\n");
        return -1;
    }

    view->section_format = node_format(file, view, 0U);
    if (section_header_size(view->section_format) == 0U) {
        fprintf(stderr, "error: unsupported section format\n");
        return -1;
    }
    for (i = 0U; i < view->nodes; ++i) {
        size_t count = node_section_count(file, view, i);
        if (node_format(file, view, i) != view->section_format ||
            count == 0U || count > UEEC_MAX_NODE_SECTIONS ||
            add_size(view->node_info_count, count - 1U,
                     &view->node_info_count) != 0) {
            fprintf(stderr, "error: invalid or mixed node section format\n");
            return -1;
        }
    }
    if (multiply_size(view->node_info_count, sizeof(ueec_node_info), &bytes) != 0 ||
        add_size(view->node_info_offset, bytes, &view->section_headers_offset) != 0 ||
        !span_ok(file->size, view->node_info_offset, bytes)) {
        fprintf(stderr, "error: truncated node mappings\n");
        return -1;
    }

    view->section_count = 1U;
    for (i = 0U; i < view->node_info_count; ++i) {
        uint16_t node = mapping_node(file, view, i);
        uint32_t section = mapping_section(file, view, i);
        if ((size_t)node >= view->nodes || section == UEEC_HEADER_SECTION_INDEX ||
            (uint64_t)section + UINT64_C(1) > (uint64_t)SIZE_MAX) {
            fprintf(stderr, "error: invalid node mapping %zu\n", i);
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
        !span_ok(file->size, view->section_headers_offset, bytes) ||
        add_size(view->name_lengths_offset, view->names,
                 &view->name_indexes_offset) != 0 ||
        !span_ok(file->size, view->name_lengths_offset, view->names) ||
        multiply_size(view->nodes, sizeof(ueec_node_name_index), &bytes) != 0 ||
        add_size(view->name_indexes_offset, bytes, &view->name_data_offset) != 0 ||
        !span_ok(file->size, view->name_indexes_offset, bytes)) {
        fprintf(stderr, "error: truncated section or name metadata\n");
        return -1;
    }
    for (i = 0U; i < view->names; ++i) {
        uint8_t length = file->data[view->name_lengths_offset + i];
        if (length == 0U || add_size(view->name_data_size, length,
                                     &view->name_data_size) != 0) {
            fprintf(stderr, "error: invalid name length\n");
            return -1;
        }
    }
    if (add_size(view->name_data_offset, view->name_data_size,
                 &view->metadata_size) != 0 ||
        !span_ok(file->size, view->name_data_offset, view->name_data_size)) {
        fprintf(stderr, "error: truncated name data\n");
        return -1;
    }
    for (i = 0U; i < view->nodes; ++i) {
        uint16_t name = read_u16(file->data + view->name_indexes_offset + i * 2U,
                                 view->endian);
        size_t mappings = 0U;
        size_t j;
        if ((size_t)name >= view->names) {
            fprintf(stderr, "error: invalid node name index\n");
            return -1;
        }
        for (j = 0U; j < view->node_info_count; ++j) {
            if ((size_t)mapping_node(file, view, j) == i) {
                ++mappings;
            }
        }
        if (mappings + 1U != node_section_count(file, view, i)) {
            fprintf(stderr, "error: node %zu section_count mismatch\n", i);
            return -1;
        }
    }

    if (get_section(file, view, 0U, &header) != 0 ||
        header.type != UEEC_SECTION_HEADER ||
        header.data_size != (uint64_t)view->metadata_size ||
        header.section_size < header.data_size ||
        header.section_size > (uint64_t)file->size) {
        fprintf(stderr, "error: invalid HEADER section\n");
        return -1;
    }
    physical_end = header.section_size;
    for (i = 1U; i < view->section_count; ++i) {
        section_view section;
        if (get_section(file, view, i, &section) != 0 ||
            section.section_size == 0U || section.data_size > section.section_size ||
            physical_end > UINT64_MAX - section.section_size) {
            fprintf(stderr, "error: invalid section %zu\n", i);
            return -1;
        }
        physical_end += section.section_size;
    }
    if (physical_end > (uint64_t)file->size) {
        fprintf(stderr, "error: sections exceed file size\n");
        return -1;
    }
    return 0;
}

static size_t name_data_offset(const file_view *file, const ueec_view *view,
                               size_t name)
{
    size_t offset = view->name_data_offset;
    size_t i;
    for (i = 0U; i < name; ++i) {
        offset += file->data[view->name_lengths_offset + i];
    }
    return offset;
}

static void node_name(const file_view *file, const ueec_view *view, size_t node,
                      const uint8_t **name, size_t *length)
{
    uint16_t index = read_u16(file->data + view->name_indexes_offset + node * 2U,
                              view->endian);
    *length = file->data[view->name_lengths_offset + index];
    *name = file->data + name_data_offset(file, view, index);
}

static int host_machine(void)
{
#if defined(__arm__)
    return UEEC_MACHINE_ARM32;
#elif defined(__aarch64__)
    return UEEC_MACHINE_ARM64;
#elif defined(__loongarch64) || defined(__loongarch__)
    return UEEC_MACHINE_LOONGARCH64;
#elif defined(__powerpc64__)
    return UEEC_MACHINE_POWERPC64;
#elif defined(__powerpc__)
    return UEEC_MACHINE_POWERPC32;
#elif defined(__riscv) && (__riscv_xlen == 32)
    return UEEC_MACHINE_RISCV32;
#elif defined(__riscv) && (__riscv_xlen == 64)
    return UEEC_MACHINE_RISCV64;
#elif defined(__sparc__) && defined(__arch64__)
    return UEEC_MACHINE_SPARC64;
#elif defined(__sparc__)
    return UEEC_MACHINE_SPARC32;
#elif defined(__i386__)
    return UEEC_MACHINE_X86;
#elif defined(__x86_64__)
    return UEEC_MACHINE_X86_64;
#else
    return -1;
#endif
}

static int node_matches_host(const file_view *file, const ueec_view *view,
                             size_t node)
{
    uint32_t word = node_word(file, view, node);
    int machine = host_machine();
    return machine >= 0 &&
           ((word >> 7) & UINT32_C(7)) == UEEC_MACHINE_TYPE_CPU &&
           ((word >> 10) & UINT32_C(0xff)) == (uint32_t)machine &&
           ((word >> 18) & UINT32_C(3)) == UEEC_SYSTEM_TYPE_GENERAL_OS &&
           ((word >> 20) & UINT32_C(0x7f)) == UEEC_SYSTEM_LINUX;
}

static int node_name_equals(const file_view *file, const ueec_view *view,
                            size_t node, const char *wanted)
{
    const uint8_t *name;
    size_t length;
    node_name(file, view, node, &name, &length);
    return strlen(wanted) == length && memcmp(wanted, name, length) == 0;
}

static int select_node(const file_view *file, const ueec_view *view,
                       const char *selector, size_t *selected)
{
    size_t i;
    if (selector != NULL) {
        char *end = NULL;
        unsigned long number;
        errno = 0;
        number = strtoul(selector, &end, 10);
        if (errno == 0 && end != selector && *end == '\0') {
            if (number >= view->nodes ||
                !node_matches_host(file, view, (size_t)number)) {
                fprintf(stderr, "error: node index is invalid for this host\n");
                return -1;
            }
            *selected = (size_t)number;
            return 0;
        }
    }

    for (i = 0U; i < view->nodes; ++i) {
        if (node_matches_host(file, view, i) &&
            (selector == NULL || node_name_equals(file, view, i, selector))) {
            *selected = i;
            return 0;
        }
    }
    fprintf(stderr, "error: no compatible Linux node was found\n");
    return -1;
}

static int selected_global_sections(const file_view *file,
                                    const ueec_view *view, size_t node,
                                    loaded_image *image)
{
    size_t i;
    image->local_count = node_section_count(file, view, node);
    image->global_section[0] = UEEC_HEADER_SECTION_INDEX;
    {
        size_t local = 1U;
        for (i = 0U; i < view->node_info_count; ++i) {
            if ((size_t)mapping_node(file, view, i) == node) {
                uint32_t section = mapping_section(file, view, i);
                size_t previous;
                if (local >= image->local_count) {
                    return -1;
                }
                for (previous = 1U; previous < local; ++previous) {
                    if (image->global_section[previous] == (size_t)section) {
                        fprintf(stderr,
                                "error: duplicate section mapping for node %zu\n",
                                node);
                        return -1;
                    }
                }
                image->global_section[local++] = section;
            }
        }
        if (local != image->local_count) {
            return -1;
        }
    }
    for (i = 0U; i < image->local_count; ++i) {
        if (get_section(file, view, image->global_section[i],
                        &image->section[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int pread_full(int fd, void *buffer, size_t length, uint64_t offset)
{
    uint8_t *p = buffer;
    size_t done = 0U;
    if (offset > (uint64_t)INT64_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    while (done < length) {
        ssize_t count = pread(fd, p + done, length - done,
                              (off_t)(offset + done));
        if (count == 0) {
            errno = EIO;
            return -1;
        }
        if (count < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)count;
    }
    return 0;
}

static void release_image(loaded_image *image)
{
    size_t i;
    for (i = 0U; i < image->region_count; ++i) {
        (void)munmap(image->regions[i].address, image->regions[i].size);
    }
    memset(image, 0, sizeof(*image));
}

static int allocate_monolithic(loaded_image *image)
{
    size_t total = 0U;
    size_t i;
    uint8_t *cursor;
    for (i = 0U; i < image->local_count; ++i) {
        if (add_size(total, image->allocation_size[i], &total) != 0) return -1;
    }
    cursor = mmap(NULL, total, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (cursor == MAP_FAILED) return -1;
    image->regions[0].address = cursor;
    image->regions[0].size = total;
    image->region_count = 1U;
    for (i = 0U; i < image->local_count; ++i) {
        image->section_address[i] = (uintptr_t)cursor;
        cursor += image->allocation_size[i];
    }
    return 0;
}

static int allocate_scattered(loaded_image *image)
{
    size_t i;
    for (i = 0U; i < image->local_count; ++i) {
        void *p = mmap(NULL, image->allocation_size[i], PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            release_image(image);
            return -1;
        }
        image->section_address[i] = (uintptr_t)p;
        image->regions[image->region_count].address = p;
        image->regions[image->region_count].size = image->allocation_size[i];
        ++image->region_count;
    }
    return 0;
}

static int allocate_hybrid(loaded_image *image)
{
    size_t data_total = 0U;
    size_t i;
    uint8_t *cursor;
    void *header = mmap(NULL, image->allocation_size[0], PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (header == MAP_FAILED) return -1;
    image->section_address[0] = (uintptr_t)header;
    image->regions[0].address = header;
    image->regions[0].size = image->allocation_size[0];
    image->region_count = 1U;
    for (i = 1U; i < image->local_count; ++i) {
        if (add_size(data_total, image->allocation_size[i], &data_total) != 0) {
            release_image(image);
            return -1;
        }
    }
    cursor = mmap(NULL, data_total, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (cursor == MAP_FAILED) {
        release_image(image);
        return -1;
    }
    image->regions[1].address = cursor;
    image->regions[1].size = data_total;
    image->region_count = 2U;
    for (i = 1U; i < image->local_count; ++i) {
        image->section_address[i] = (uintptr_t)cursor;
        cursor += image->allocation_size[i];
    }
    return 0;
}

static int write_runtime_section(uint8_t *p, uint16_t format,
                                 const section_view *section,
                                 uint64_t section_size, uint64_t data_size)
{
    write_u16(p, section->type, UEEC_HOST_ENDIAN);
    if (format == UEEC_FORMAT_SH16) {
        if (section_size > UINT16_MAX || data_size > UINT16_MAX) return -1;
        write_u16(p + 2, (uint16_t)section_size, UEEC_HOST_ENDIAN);
        write_u16(p + 4, (uint16_t)data_size, UEEC_HOST_ENDIAN);
    } else if (format == UEEC_FORMAT_SH32) {
        if (section_size > UINT32_MAX || data_size > UINT32_MAX) return -1;
        write_u32(p + 2, (uint32_t)section_size, UEEC_HOST_ENDIAN);
        write_u32(p + 6, (uint32_t)data_size, UEEC_HOST_ENDIAN);
    } else if (format == UEEC_FORMAT_SH64) {
        write_u64(p + 2, section_size, UEEC_HOST_ENDIAN);
        write_u64(p + 10, data_size, UEEC_HOST_ENDIAN);
    } else {
        return -1;
    }
    return 0;
}

static int build_runtime_header(const file_view *file, const ueec_view *view,
                                size_t node, loaded_image *image,
                                size_t address_offset, size_t name_length)
{
    uint8_t *runtime = (uint8_t *)image->section_address[0];
    const uint8_t *name;
    size_t ignored;
    size_t header_size = section_header_size(view->section_format);
    size_t cursor = sizeof(ueec_hdr) + sizeof(ueec_node_hdr);
    size_t i;

    runtime[0] = UEEC_MAGIC_0;
    runtime[1] = UEEC_MAGIC_1;
    runtime[2] = UEEC_MAGIC_2;
    runtime[3] = (uint8_t)((UEEC_HOST_ENDIAN << 7) | (view->version & 0x7fU));
    write_u16(runtime + 4, UINT16_C(1), UEEC_HOST_ENDIAN);
    write_u16(runtime + 6, UINT16_C(1), UEEC_HOST_ENDIAN);
    write_u32(runtime + sizeof(ueec_hdr), node_word(file, view, node),
              UEEC_HOST_ENDIAN);
    write_u16(runtime + sizeof(ueec_hdr) + 4U, view->section_format,
              UEEC_HOST_ENDIAN);

    for (i = 0U; i < image->local_count; ++i) {
        uint64_t data_size = i == 0U ? (uint64_t)image->runtime_data_size :
                                      image->section[i].data_size;
        if (write_runtime_section(runtime + cursor, view->section_format,
                                  &image->section[i], image->allocation_size[i],
                                  data_size) != 0) {
            return -1;
        }
        cursor += header_size;
    }
    for (i = 0U; i < image->local_count; ++i) {
        memcpy(runtime + address_offset + i * sizeof(uintptr_t),
               &image->section_address[i], sizeof(uintptr_t));
    }
    cursor = address_offset + image->local_count * sizeof(uintptr_t);
    runtime[cursor++] = (uint8_t)name_length;
    write_u16(runtime + cursor, UINT16_C(0), UEEC_HOST_ENDIAN);
    cursor += sizeof(ueec_node_name_index);
    node_name(file, view, node, &name, &ignored);
    memcpy(runtime + cursor, name, name_length);
    return 0;
}

static int build_runtime_image(int fd, const file_view *file,
                               const ueec_view *view, size_t node,
                               load_mode requested, loaded_image *image)
{
    const uint8_t *name;
    size_t name_length;
    size_t page_size;
    size_t section_bytes;
    size_t address_offset;
    size_t cursor;
    size_t code_index = SIZE_MAX;
    size_t i;
    long page = sysconf(_SC_PAGESIZE);

    memset(image, 0, sizeof(*image));
    if (page <= 0 || selected_global_sections(file, view, node, image) != 0) {
        fprintf(stderr, "error: cannot create the selected section list\n");
        return -1;
    }
    page_size = (size_t)page;
    node_name(file, view, node, &name, &name_length);
    (void)name;
    if (multiply_size(image->local_count,
                      section_header_size(view->section_format),
                      &section_bytes) != 0 ||
        add_size(sizeof(ueec_hdr) + sizeof(ueec_node_hdr), section_bytes,
                 &cursor) != 0 ||
        align_up(cursor, _Alignof(uintptr_t), &address_offset) != 0 ||
        multiply_size(image->local_count, sizeof(uintptr_t), &section_bytes) != 0 ||
        add_size(address_offset, section_bytes, &cursor) != 0 ||
        add_size(cursor, sizeof(ueec_name_len) + sizeof(ueec_node_name_index),
                 &cursor) != 0 ||
        add_size(cursor, name_length, &image->runtime_data_size) != 0 ||
        align_up(image->runtime_data_size, page_size,
                 &image->allocation_size[0]) != 0) {
        fprintf(stderr, "error: runtime HEADER size overflow\n");
        return -1;
    }
    for (i = 1U; i < image->local_count; ++i) {
        if (image->section[i].section_size == 0U ||
            image->section[i].section_size > (uint64_t)SIZE_MAX ||
            align_up((size_t)image->section[i].section_size, page_size,
                     &image->allocation_size[i]) != 0) {
            fprintf(stderr, "error: invalid section allocation size\n");
            return -1;
        }
        if (image->section[i].type == UEEC_SECTION_CODE && code_index == SIZE_MAX) {
            code_index = i;
        }
    }
    if (code_index == SIZE_MAX) {
        fprintf(stderr, "error: selected node has no CODE section\n");
        return -1;
    }

    if (requested == LOAD_AUTO || requested == LOAD_MONOLITHIC) {
        if (allocate_monolithic(image) == 0) {
            image->actual_mode = LOAD_MONOLITHIC;
        } else if (requested == LOAD_AUTO && allocate_scattered(image) == 0) {
            image->actual_mode = LOAD_SCATTERED;
        } else {
            fprintf(stderr, "error: monolithic mmap failed: %s\n", strerror(errno));
            return -1;
        }
    } else if (requested == LOAD_SCATTERED) {
        if (allocate_scattered(image) != 0) {
            fprintf(stderr, "error: scattered mmap failed: %s\n", strerror(errno));
            return -1;
        }
        image->actual_mode = LOAD_SCATTERED;
    } else {
        if (allocate_hybrid(image) != 0) {
            fprintf(stderr, "error: hybrid mmap failed: %s\n", strerror(errno));
            return -1;
        }
        image->actual_mode = LOAD_HYBRID;
    }

    if (build_runtime_header(file, view, node, image, address_offset,
                             name_length) != 0) {
        fprintf(stderr, "error: runtime HEADER does not fit section format\n");
        release_image(image);
        return -1;
    }
    for (i = 1U; i < image->local_count; ++i) {
        uint64_t offset;
        if (image->section[i].data_size > (uint64_t)SIZE_MAX ||
            section_file_offset(file, view, image->global_section[i], &offset) != 0 ||
            pread_full(fd, (void *)image->section_address[i],
                       (size_t)image->section[i].data_size, offset) != 0) {
            fprintf(stderr, "error: cannot load section %zu: %s\n", i,
                    strerror(errno));
            release_image(image);
            return -1;
        }
        __builtin___clear_cache((char *)image->section_address[i],
                                (char *)image->section_address[i] +
                                (size_t)image->section[i].data_size);
    }
    for (i = 0U; i < image->local_count; ++i) {
        int protection = PROT_READ;
        if (i != 0U && image->section[i].type == UEEC_SECTION_CODE) {
            protection |= PROT_EXEC;
        } else if (i != 0U && image->section[i].type != UEEC_SECTION_CONSTANT_DATA) {
            protection |= PROT_WRITE;
        }
        if (mprotect((void *)image->section_address[i], image->allocation_size[i],
                     protection) != 0) {
            fprintf(stderr, "error: mprotect section %zu: %s\n", i,
                    strerror(errno));
            release_image(image);
            return -1;
        }
    }
    image->entry = (void *)image->section_address[code_index];
    return 0;
}

static const char *mode_name(load_mode mode)
{
    switch (mode) {
    case LOAD_AUTO: return "auto";
    case LOAD_MONOLITHIC: return "monolithic";
    case LOAD_SCATTERED: return "scattered";
    case LOAD_HYBRID: return "hybrid";
    }
    return "unknown";
}

static int parse_mode(const char *text, load_mode *mode)
{
    if (strcmp(text, "auto") == 0) *mode = LOAD_AUTO;
    else if (strcmp(text, "monolithic") == 0) *mode = LOAD_MONOLITHIC;
    else if (strcmp(text, "scattered") == 0) *mode = LOAD_SCATTERED;
    else if (strcmp(text, "hybrid") == 0) *mode = LOAD_HYBRID;
    else return -1;
    return 0;
}

static void call_raw_entry(void *address)
{
#if defined(__powerpc64__)
    __asm__ volatile("mtctr %0\n\tbctr" : : "r"(address) : "ctr", "memory");
    __builtin_unreachable();
#else
    void (*entry)(void);
    _Static_assert(sizeof(entry) == sizeof(address), "unsupported function pointer");
    memcpy(&entry, &address, sizeof(entry));
    entry();
#endif
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--node NAME|INDEX] "
            "[--load-mode auto|monolithic|scattered|hybrid] [--no-run] FILE.ueec\n",
            program);
}

int main(int argc, char **argv)
{
    const char *selector = NULL;
    const char *path = NULL;
    load_mode requested = LOAD_AUTO;
    int run = 1;
    int fd;
    int i;
    struct stat status;
    void *mapping;
    file_view file;
    ueec_view view;
    loaded_image image;
    size_t selected;
    const uint8_t *name;
    size_t name_length;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--node") == 0) {
            if (++i >= argc || selector != NULL) { usage(argv[0]); return 2; }
            selector = argv[i];
        } else if (strcmp(argv[i], "--load-mode") == 0) {
            if (++i >= argc || parse_mode(argv[i], &requested) != 0) {
                usage(argv[0]); return 2;
            }
        } else if (strcmp(argv[i], "--no-run") == 0) {
            run = 0;
        } else if (argv[i][0] == '-' || path != NULL) {
            usage(argv[0]); return 2;
        } else {
            path = argv[i];
        }
    }
    if (path == NULL) { usage(argv[0]); return 2; }

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        return 2;
    }
    if (fstat(fd, &status) != 0 || status.st_size <= 0 ||
        (uintmax_t)status.st_size > (uintmax_t)SIZE_MAX) {
        fprintf(stderr, "error: invalid file size\n");
        (void)close(fd);
        return 2;
    }
    mapping = mmap(NULL, (size_t)status.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping == MAP_FAILED) {
        fprintf(stderr, "mmap %s: %s\n", path, strerror(errno));
        (void)close(fd);
        return 2;
    }
    file.data = mapping;
    file.size = (size_t)status.st_size;
    if (parse_ueec(&file, &view) != 0 ||
        select_node(&file, &view, selector, &selected) != 0 ||
        build_runtime_image(fd, &file, &view, selected, requested, &image) != 0) {
        (void)munmap(mapping, file.size);
        (void)close(fd);
        return 1;
    }

    node_name(&file, &view, selected, &name, &name_length);
    printf("UEEC loader\n");
    printf("  node:                %zu (%.*s)\n", selected, (int)name_length,
           (const char *)name);
    printf("  requested mode:      %s\n", mode_name(requested));
    printf("  actual mode:         %s\n", mode_name(image.actual_mode));
    printf("  runtime sections:    %zu\n", image.local_count);
    printf("  runtime HEADER bytes:%zu\n", image.runtime_data_size);
    printf("  section_address[0]:  %p\n", (void *)image.section_address[0]);
    printf("  entry:               %p\n", image.entry);
    fflush(stdout);

    (void)munmap(mapping, file.size);
    (void)close(fd);
    if (run != 0) {
        call_raw_entry(image.entry);
    }
    release_image(&image);
    return 0;
}
