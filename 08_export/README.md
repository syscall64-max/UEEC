# 08_export — სამი Export დიზაინი

`ueec_export.h` — სამი, `design`-ველით არჩევადი დიზაინი, RELOC-ის
კონვენციის გაგრძელებით:

| `design` | სახელი | ძებნის გასაღები | struct-ები |
|---|---|---|---|
| `0` | `COMPACT_GROUP` | სახელი (length-ით ჯგუფვა + ორდონიანი binary search) | `groups`+`group`+`info` |
| `1` | `SIMPLE_FLAT` | სახელი (ერთდონიანი binary search) | `names`+`flat` |
| `2` | `ORDINAL` | რიცხვითი `ordinal` (binary search, სახელის გარეშე) | `count`+`ordinal16/32/64` |

## `ORDINAL` (design=2) — PE-ის მოდერნიზებული, გამარტივებული ვერსია

PE-ის ნამდვილ `IMAGE_EXPORT_DIRECTORY`-ს აქვს **სამი** ცალკეული მასივი
(`AddressOfFunctions`/`AddressOfNames`/`AddressOfNameOrdinals`) —
ordinal-only export-ის მხარდასაჭერად (`NumberOfFunctions > NumberOfNames`).
ეს **პრაქტიკაში იშვიათია** (თანამედროვე DLL-ები თითქმის ყოველთვის named
export-ებს იყენებენ) — ამიტომ UEEC-ისთვის **გამარტივებული, ერთი, ბრტყელი
მასივი** ავირჩიეთ, სადაც `ordinal` უბრალოდ ცხადი, დამატებითი ველია
(`section`+`offset_rva`+`ordinal`), **ordinal-ითვე დალაგებული** —
binary search-ისთვის, სახელის (name_data) გარეშე.

```c
typedef struct __attribute__((packed)) {
    uint8_t  section : 5, reserved : 3;
    uint16_t offset_rva;
    uint16_t ordinal;
} ueec_export_ordinal16;   // 5 ბაიტი (+ 32/64 scaled ვარიანტები)
```

## დემონსტრირება (x86-64)

**`COMPACT_GROUP`**: `make run` — named ცვლადი+ფუნქცია, binary search
სახელით.

**`ORDINAL`**: `make run-ordinal` — `ordinal=1`→`answer`(ფუნქცია),
`ordinal=5`→`counter`(ცვლადი), სახელის გარეშე, `ordinal`-ის binary search-ით.

```
ordinal=5  -> value=42     (2 cmp)
ordinal=1  -> answer()=42  (1 cmp)
ordinal=99 -> ვერ მოიძებნა (2 cmp)
```

## Multi-architecture ვალიდაცია — 10/10 architecture, ორივე დიზაინით

```
make all
```

**ყველა 10 architecture** (`arm32, arm64, riscv32, riscv64, powerpc32,
powerpc64, sparc32, sparc64, loongarch64, x86`) **+ x86-64** — `COMPACT_GROUP`
-ისა და `ORDINAL`-ის ორივე demo-თი, `answer()` function-call-ისა და
value-lookup-ის ჩათვლით.

### გზად ნაპოვნი და გასწორებული ბაგები

ეს პროექტი თავდაპირველად **მხოლოდ x86-64-ზე** იყო დამოწმებული
(ჩაშენებული `arch/answer_x86_64.bin` (`mov eax,42; ret`) x86-64
-სპეციფიური opcode-ებია). სხვა architecture-ზე პირდაპირ გაშვება
"Illegal instruction"/"Segmentation fault"-ს იწვევდა. გასწორებულია —
`09_convert_export`-ში ნაპოვნი ოთხივე გამოსწორება აქაც გადმოტანილია:

1. **თითოეული architecture-სთვის ნამდვილი machine code.**
   `arch/answer.c` (`int answer(void){return 42;}`) ცალკე ითარგმნება
   თითოეული architecture-სთვის (`objcopy --only-section=.text`) —
   `Makefile`-ს ახლა აქვს rule თითოეული `arch/answer_<arqiteqtura>.bin`
   -სთვის, რომელიც **on demand** აშენებს (`gen_export.c`/
   `gen_export_ordinal.c` პარამეტრიზებულია: `<code.bin> <UEEC_MACHINE_*>
   [be|le]`).
2. **ARM32 Thumb-interworking.** `-marm`-ით გასწორებულია (32-ბიტიანი
   ARM encoding, არა Thumb-ის default).
3. **DATA content-ის endianness.** `counter`(`gen_export_ordinal.c`)
   და `counter/bar/foo/banana`(`gen_export.c`) int32 მნიშვნელობები
   ახლა target-architecture-ის ბუნებრივი byte-order-ით იწერება
   (BE `powerpc32/64`/`sparc32/64`-ზე, LE დანარჩენზე).
4. **PowerPC64 ELFv1 function-descriptor.** `call_answer()` ფუნქცია
   ორივე loader-ში (`loader_export.c`, `loader_export_ordinal.c`)
   `volatile unsigned long fdesc[3]`-ტრიუკს იყენებს PowerPC64
   ELFv1-ზე.

## გაშვება

```
make run              # COMPACT_GROUP, x86-64
make run-ordinal       # ORDINAL, x86-64
make all               # yvela 10 architecture, orive dizaini
make -k all             # Tu /opt/riscv32 an sparc32-is 32-bit multilib ar gaqvT
```
