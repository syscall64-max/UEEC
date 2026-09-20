# 07_pe — PE_STYLE RELOC

`reloc_v4.h`-ის დიზაინი — `couple`-ს აქვს მხოლოდ **ერთი** `section` ველი
(`target`), `source` **იმპლიციტურია** — ყოველთვის `image_base` (მთელი
node-ის, monolithic-ად ჩატვირთული resident-სექციების საერთო საწყისი).

## ⚠️ შეზღუდვა — ერთ-node-იანი, `monolithic`-only

ეს დიზაინი **მხოლოდ** მაშინ მუშაობს, თუ:
1. **ერთი node** ფაილში (`06_extract`-ის ბუნებრივი output) — რომ ერთი,
   ცალსახა `image_base` არსებობდეს.
2. **`monolithic` load-mode** — `scattered`-ში თითო სექცია ცალკე mmap-ია,
   საერთო `image_base` საერთოდ არ არსებობს.

`loader_pe.c` ორივეს **ცხადად** ამოწმებს (`nodes != 1` → შეცდომა).

## Layout

```
ueec_reloc(format,design=3) -> couple16(section:5,format:11,count)
  -> count x [ block16(block,count) -> block.count x info16(offset:12,type:4) ]
```

`block`+`offset` — **absolute base + delta** კონვენცია (იგივე, რაც
`BLOCK_4096`/`BLOCK_N`-ს ჰქონდა) — `patch_offset = block + offset`,
`patch_addr = local_base[section] + patch_offset`, `addend`(ფაილში
ჩაწერილი) → `image_base + addend`.

## გაშვება

```
./gen_pe 10 > node_x86_64.asm     # arch-index: 0-10 (იხ. Makefile)
nasm -f bin node_x86_64.asm -o node_x86_64.ueec
./loader_pe node_x86_64.ueec

make run-x86_64
make all              # ყველა architecture (QEMU-ს ქვეშ)
make -k all           # თუ /opt/riscv32 არ გაქვთ
```

**10/10 architecture დამოწმებული** (PowerPC32/64, SPARC32/64 big-endian
ჩათვლით).

## ნაპოვნი და გასწორებული ბაგი

**Endianness.** `loader_pe.c`-ის თავდაპირველ ვერსიაში `rd16`/`rd32`
`file_endian`-შემოწმების გარეშე იყო დაწერილი (სხვა ჩვენს loader-ებთან
შედარებით გამარტივებისას შემთხვევით გამოვტოვე) — BE architecture-ებზე
(`PowerPC32/64`, `SPARC32/64`) `nodes=1` (`0x0001` LE) არასწორად
იკითხებოდა, როგორც `256`. გასწორდა `file_endian`-პარამეტრის ყველგან
დამატებით.
