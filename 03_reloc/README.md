# 03_reloc — SECTION_FLAT RELOC + ახალი _GM core

ერთი UEEC container (`_GM` header-ფორმატი) — 1 HEADER + 1 DATA(გაზიარებული,
`"Hello world!\n"`) + 11 × (CODE+RELOCATION), `SECTION_FLAT` RELOC-დიზაინით
(`reloc_v1.h` — `couple`+`info`, page/block-ის გარეშე).

## ფაილები

- `03_reloc.asm` — თვითკმარი NASM container (279 ხაზი-მსგავსი, `db`/`dw`/`dd` ბაიტები)
- `gen.c` — generator, რომელმაც `.asm` შექმნა (dev-ხელსაწყო, საბოლოო deliverable არაა)
- `loader.c` — loader, ორი load-mode-ით (`--L=monolithic`, `--L=scattered`)
- `ueec.h` — ახალი `_GM` core header (`George Modebadze`-ის სპეციფიკაცია)
- `reloc.h` — `SECTION_FLAT` RELOC structures (`reloc_v1.h`)

## გაშვება

```
make run-mono         # x86-64, monolithic
make run-scattered      # x86-64, scattered
make all               # ყველა architecture, ორივე რეჟიმში (QEMU-ს ქვეშ)
make -k all            # თუ /opt/riscv32 არ გაქვთ
```

**20/20 (10 architecture × 2 load-mode) დამოწმებული** — PowerPC32/64, SPARC32/64
(big-endian) ჩათვლით.

## RELOC-მექანიზმი (SECTION_FLAT)

```
ueec_reloc(format+design) -> ueec_reloc_couple16(target+source+format+count)
  -> count x ueec_reloc_info(offset:12+type:4)
```

`target`/`source` — node-ის local (non-HEADER) სექცია-ინდექსები (`local 0`=DATA,
`local 1`=CODE). `addend`(CODE-ში ჩაწერილი, ყოველთვის `0`, ვინაიდან ყველა node
ერთსა და იმავე DATA-offset `0`-ს იყენებს — ერთადერთი, საერთო სტრიქონია).

## ⚠️ ნაპოვნი და გასწორებული ბაგი

**`monolithic` mode-ის DATA→CODE alignment.** DATA (`13` ბაიტი) და CODE
თავდაპირველად უშუალოდ ერთმანეთის მიყოლებით იწერებოდა monolithic buffer-ში —
CODE არაგასწორებულ (`13 % 4 = 1`) მისამართზე მოხვდებოდა, რაც ARM32/64-სა და
PowerPC32/64-ზე instruction-fetch-ს ამტვრევდა (segfault/bus-error/illegal
instruction). გასწორდა `16`-ბაიტიანი alignment-ის დამატებით monolithic-ის
თითოეულ სექციას შორის.

## ცნობილი შეზღუდვები

- `riscv32` საჭიროებს `/opt/riscv32` toolchain-ს.
- `loader.c`-ის `mprotect(..., PROT_READ|PROT_EXEC|PROT_WRITE)` (monolithic)
  არის მარტივი test-harness-ის გამარტივება (W+X) — production-ში ცალკე
  write-ფაზა და exec-ფაზა უნდა გაიყოს.
