//
#ifndef UEEC_EXPORT_H
#define UEEC_EXPORT_H
//
#include <stdint.h>
//
#if defined(_MSC_VER)
# define UEEC_PACKED
# pragma pack(push, 1)
#elif defined(__GNUC__) || defined(__clang__)
# define UEEC_PACKED __attribute__((packed))
#else
# error "UEEC: packed structure support is required"
#endif
//
#define UEEC_EXPORT_DESIGN_COMPACT_GROUP UINT16_C(0)
#define UEEC_EXPORT_DESIGN_SIMPLE_FLAT   UINT16_C(1)
#define UEEC_EXPORT_DESIGN_ORDINAL       UINT16_C(2)
//
#define UEEC_EXPORT_FORMAT_16 UINT16_C(0)
#define UEEC_EXPORT_FORMAT_32 UINT16_C(1)
#define UEEC_EXPORT_FORMAT_64 UINT16_C(2)
//
typedef struct UEEC_PACKED {
    uint16_t    format;   // groups/group/info-ს საერთო სიგანე
    uint16_t    design;   // UEEC_EXPORT_DESIGN_*
} ueec_export, *pueec_export;
//
// groups : ჯგუფების რაოდენობა (თითოეული ჯგუფი -- ერთი სახელის-სიგრძე).
// !!! ჯგუფები ყოველთვის length-ით ანბანურად/რიცხობრივად დალაგებულია
//     (binary search-ისთვის) !!!
//
typedef struct UEEC_PACKED { uint16_t format; uint16_t groups; } ueec_export_groups16, *pueec_export_groups16;
typedef struct UEEC_PACKED { uint16_t format; uint32_t groups; } ueec_export_groups32, *pueec_export_groups32;
typedef struct UEEC_PACKED { uint16_t format; uint64_t groups; } ueec_export_groups64, *pueec_export_groups64;
//
// length : ამ ჯგუფის ყველა სახელის (ფიქსირებული) სიგრძე
// count  : ამ ჯგუფის entry-ების (info+სახელი) რაოდენობა
//
typedef struct UEEC_PACKED { uint16_t length; uint16_t count; } ueec_export_group16, *pueec_export_group16;
typedef struct UEEC_PACKED { uint16_t length; uint32_t count; } ueec_export_group32, *pueec_export_group32;
typedef struct UEEC_PACKED { uint16_t length; uint64_t count; } ueec_export_group64, *pueec_export_group64;
//
// section : node-local, resident-სექციის ინდექსი
// offset  : export-ის მისამართი section-ის დასაწყისიდან
//
// !!! ჯგუფის შიგნით info[] ანბანურადაა დალაგებული (იმავე-length
//     სახელებს შორის, name_data-ში, ორივე დონეზე binary search) !!!
//
typedef struct UEEC_PACKED { uint8_t section:5, reserved:3; uint16_t offset; } ueec_export_info16, *pueec_export_info16;
typedef struct UEEC_PACKED { uint8_t section:5, reserved:3; uint32_t offset; } ueec_export_info32, *pueec_export_info32;
typedef struct UEEC_PACKED { uint8_t section:5, reserved:3; uint64_t offset; } ueec_export_info64, *pueec_export_info64;
//
// Layout (design == COMPACT_GROUP):
//   ueec_export -> ueec_export_groups16/32/64
//     -> ueec_export_group16/32/64[groups]      (length-ით დალაგებული)
//       -> ueec_export_info16/32/64[group.count] (ჯგუფის შიგნით, სახელით დალაგებული)
//       -> name_data[group.count * group.length]  (ფიქსირებული-სიგანის სახელები, info[]-ის იმავე რიგით)
//
// (group-ების, info-ების და name_data-ს რიგი: group[0]-ს info+names,
//  შემდეგ group[1]-ს info+names, და ასე შემდეგ)
//
// ============================================================
// design == SIMPLE_FLAT: ერთდონიანი, სახელით ანბანურად დალაგებული
// ============================================================
//
typedef struct UEEC_PACKED { uint16_t format; uint16_t names; } ueec_export_names16, *pueec_export_names16;
typedef struct UEEC_PACKED { uint16_t format; uint32_t names; } ueec_export_names32, *pueec_export_names32;
typedef struct UEEC_PACKED { uint16_t format; uint64_t names; } ueec_export_names64, *pueec_export_names64;
//
// section     : node-local, resident-სექციის ინდექსი
// length      : სახელის სიგრძე ბაიტებში
// offset_rva  : export-ის მისამართი section-ის დასაწყისიდან
// offset_name : offset name_data blob-ში (ცალკე, ბოლოში)
//
// !!! ეს მასივი ყოველთვის სახელით ანბანურადაა დალაგებული
//     (ერთდონიანი binary search-ისთვის) !!!
//
typedef struct UEEC_PACKED {
    uint16_t    section : 5, length : 11;
    uint16_t    offset_rva;
    uint16_t    offset_name;
} ueec_export_flat16, *pueec_export_flat16;
//
typedef struct UEEC_PACKED {
    uint16_t    section : 5, length : 11;
    uint32_t    offset_rva;
    uint32_t    offset_name;
} ueec_export_flat32, *pueec_export_flat32;
//
typedef struct UEEC_PACKED {
    uint16_t    section : 5, length : 11;
    uint64_t    offset_rva;
    uint64_t    offset_name;
} ueec_export_flat64, *pueec_export_flat64;
//
// Layout (design == SIMPLE_FLAT):
//   ueec_export -> ueec_export_names16/32/64
//     -> ueec_export_flat16/32/64[names]   (ანბანურად დალაგებული, name-ით)
//     -> name_data (ყველა სახელის ტექსტი, ერთმანეთის მიყოლებით)
//
// ============================================================
// design == ORDINAL: წმინდა, სახელის-გარეშე, ordinal-ზე დაფუძნებული —
// PE-ის IMAGE_EXPORT_DIRECTORY-ის მოდერნიზებული, გამარტივებული ვერსია
// (ერთი, ბრტყელი მასივი -- density-array + name-array-ის გაყოფის
// ნაცვლად, ვინაიდან export-by-ordinal-only პრაქტიკაში იშვიათია).
// ============================================================
//
typedef struct UEEC_PACKED { uint16_t format; uint16_t count; } ueec_export_count16, *pueec_export_count16;
typedef struct UEEC_PACKED { uint16_t format; uint32_t count; } ueec_export_count32, *pueec_export_count32;
typedef struct UEEC_PACKED { uint16_t format; uint64_t count; } ueec_export_count64, *pueec_export_count64;
//
// section    : node-local, resident-სექციის ინდექსი
// offset_rva : export-ის მისამართი section-ის დასაწყისიდან
// ordinal    : რიცხვითი იდენტიფიკატორი (სახელის ნაცვლად)
//
// !!! ეს მასივი ყოველთვის ordinal-ით რიცხობრივადაა დალაგებული
//     (binary search-ისთვის) !!!
//
typedef struct UEEC_PACKED {
    uint8_t     section : 5, reserved : 3;
    uint16_t    offset_rva;
    uint16_t    ordinal;
} ueec_export_ordinal16, *pueec_export_ordinal16;
//
typedef struct UEEC_PACKED {
    uint8_t     section : 5, reserved : 3;
    uint32_t    offset_rva;
    uint32_t    ordinal;
} ueec_export_ordinal32, *pueec_export_ordinal32;
//
typedef struct UEEC_PACKED {
    uint8_t     section : 5, reserved : 3;
    uint64_t    offset_rva;
    uint64_t    ordinal;
} ueec_export_ordinal64, *pueec_export_ordinal64;
//
// Layout (design == ORDINAL):
//   ueec_export -> ueec_export_count16/32/64
//     -> ueec_export_ordinal16/32/64[count]   (ordinal-ით დალაგებული)
//
#endif /* UEEC_EXPORT_H */
