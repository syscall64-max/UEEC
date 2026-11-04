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
./gen_pe 10 > node_x86_64.asm     # arch-index: 0-11 (იხ. Makefile)
nasm -f bin node_x86_64.asm -o node_x86_64.ueec
./loader_pe node_x86_64.ueec

make run-x86_64
make all              # 12 architecture (QEMU-ს ქვეშ)
make check-layout     # 12/12 PE_STYLE-ის სტრუქტურული შემოწმება (nasm-ის გარეშეც)
make verify-s390x-bytes  # სურვილისამებრ: s390x ასემბლერის კოდის ბაიტური შედარება
make -k all           # თუ /opt/riscv32 არ გაქვთ
```

**12 არქიტექტურა მხარდაჭერილია** (12-არქიტექტურული QEMU მატრიცა გასაშვებია Ubuntu-ზე) (PowerPC32/64, SPARC32/64 big-endian
ჩათვლით).

## ნაპოვნი და გასწორებული ბაგი

**Endianness.** `loader_pe.c`-ის თავდაპირველ ვერსიაში `rd16`/`rd32`
`file_endian`-შემოწმების გარეშე იყო დაწერილი (სხვა ჩვენს loader-ებთან
შედარებით გამარტივებისას შემთხვევით გამოვტოვე) — BE architecture-ებზე
(`PowerPC32/64`, `SPARC32/64`) `nodes=1` (`0x0001` LE) არასწორად
იკითხებოდა, როგორც `256`. გასწორდა `file_endian`-პარამეტრის ყველგან
დამატებით.

## IBM Z / S390X

S390X არის მე-12 არქიტექტურა (`machine=11`, Big-Endian კოდი, `ABS64`).
რელოკაციაა CODE-ში `0x20` ოფსეტზე, `PE_STYLE` `design=3`;
DATA და CODE ერთად იტვირთება monolithic რეჟიმში, ამიტომ `image_base`
DATA-ს საწყისი მისამართია.

```sh
make s390x
make check-layout
make all
```

`scattered` რეჟიმი აქ **არ არის მხარდაჭერილი** — PE_STYLE-ის implicit
`image_base` მოდელი ერთიან მონოლითურ ჩატვირთვას მოითხოვს.
