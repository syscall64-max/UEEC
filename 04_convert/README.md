# 04_convert — უნივერსალური RELOC-კონვერტერი (ყველა → ყველა)

კითხულობს UEEC RELOC-სექციას **ნებისმიერი** დიზაინით (`SECTION_FLAT`,
`SECTION_BLOCK_4096`, `SECTION_BLOCK_N`) და გადაჰყავს **ნებისმიერ** სხვაში.

## საკვანძო დიზაინის გადაწყვეტილება

`block.index`(`ueec_reloc_block16/32/64`) აღინიშნება, როგორც **პირდაპირი,
absolute base** (`index + offset = სრული offset`), **არა** "page-номерად"
(რომელიც `page_size`-ის ცოდნას მოითხოვდა decode-ის დროს — `SECTION_BLOCK_N`-ს
კი ეს ინფორმაცია ფაილში საერთოდ არ აქვს დამახსოვრებული). ამის წყალობით,
decode **თვითკმარია** ნებისმიერი block-დიზაინისთვის, `page_size`-ის გარე
ცოდნის გარეშე — round-trip კონვერტაცია ყოველთვის სწორია.

## გამოყენება

```
./convert input.ueec output.ueec --design flat
./convert input.ueec output.ueec --design block4096
./convert input.ueec output.ueec --design blockn --page-size N
```

`loader.c` ავტომატურად ცნობს input-ის დიზაინს (`design`-ველიდან) და სამივეს
ერთნაირად უმართავს.

## გაშვება

```
make run-all-directions   # x86-64: 6 კონვერტაციის მიმართულება (round-trip-ის ჩათვლით)
make all                   # ყველა architecture, BLOCK_4096 + BLOCK_N
make -k all                # თუ /opt/riscv32 არ გაქვთ
```

**26/26 დამოწმებული** (6 კონვერტაციის მიმართულება x86-64-ზე + 10 architecture
× 2 დიზაინი) — round-trip-ის ჩათვლით (`BLOCK_4096→FLAT` ზუსტად აღადგენს
ორიგინალურ, `1028`-ბაიტიან ფაილს).

## ცნობილი შეზღუდვა

`PE`-სტილის მეოთხე დიზაინი (`reloc_v4.h`, `section`-ველით `target`/`source`-ის
ნაცვლად) **განზრახ გამოტოვებულია** ამ ეტაპზე — მუშაობს მხოლოდ ერთ-node-იან
UEEC ფაილებში (სადაც node-ის ყველა სექცია ერთ, გაერთიანებულ, page-aligned
"ვირტუალურ image"-ად განიხილება, UEF-ის მოდელის ანალოგიით). ცალკე,
დამატებით ეტაპად დარჩება.


## S390X (IBM Z) — 12th architecture (2026-11-03)

The UEEC input now has 12 CPU nodes and 26 physical sections: one shared DATA,
12 CODE and 12 RELOCATION sections, plus HEADER. New node index 11 is s390x
(code-endian big). The metadata remains little-endian. S390X code is 40 bytes,
with an ABS64 relocation at CODE offset 0x20 into the shared DATA section.

The baseline code and relocation bytes are inherited unchanged from the
previously validated `03_reloc_12arch_s390x` stage.

To run the S390X test on Ubuntu (cross-GCC/QEMU required):

```sh
make clean
make s390x
make run-all-directions
make all
```

`make s390x` converts all 12 RELOCATION sections from FLAT to BLOCK_4096
and BLOCK_N, then uses `qemu-s390x` to execute the s390x node in monolithic
and scattered modes (expect `Hello world!` twice).

`make run-all-directions` checks all six conversion directions on the native
x86-64 host. This is not a full 12-architecture round-trip equivalence proof;
it is a loader-based functional test suite. Other architectures use the same
converted 12-node container and select their own node.

NOTE: This reference converter is built for little-endian x86-64 hosts and
is not itself endianness-independent; the S390X loader parses little-endian
metadata correctly. Do not cross-compile the converter without first adapting
its byte-order handling.

Run `make check-roundtrip` to check byte-identical conversion on the supplied fixture.
