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
